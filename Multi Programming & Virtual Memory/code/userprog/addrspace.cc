// addrspace.cc 
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -n -T 0 option 
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "main.h"
#include "addrspace.h"
#include "machine.h"
#include "noff.h"

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the 
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void 
SwapHeader (NoffHeader *noffH)
{
    noffH->noffMagic = WordToHost(noffH->noffMagic);
    noffH->code.size = WordToHost(noffH->code.size);
    noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
    noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
    noffH->initData.size = WordToHost(noffH->initData.size);
    noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
    noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
    noffH->uninitData.size = WordToHost(noffH->uninitData.size);
    noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
    noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Set up the translation from program memory to physical 
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//----------------------------------------------------------------------

AddrSpace::AddrSpace()
{
    pageTable = new TranslationEntry[NumPhysPages];
    for (unsigned int i = 0; i < NumPhysPages; i++) {
	pageTable[i].virtualPage = 0;
	pageTable[i].physicalPage = 0;
	pageTable[i].valid = FALSE;
	pageTable[i].use = FALSE;
	pageTable[i].dirty = FALSE;
	pageTable[i].readOnly = FALSE;  
    }
    
    // zero out the entire address space
//    bzero(kernel->machine->mainMemory, MemorySize);
}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.
//----------------------------------------------------------------------

AddrSpace::~AddrSpace()
{
   delete pageTable;
}


//
static 
void copyPage(OpenFile *dst, int dstPos, OpenFile *src, int srcPos, int size)
{
	char *tmpPage = new char[PageSize];
	int byteLeft = size;
    	
	DEBUG(dbgAddr, "copyPage: dst=" << dst << ", dstPos=" << dstPos << ", src =" << src << ", srcPos=" << srcPos << ", size=" << size << "\n");

	while (byteLeft >= (int)PageSize) {
		// read PageSize bytes from src file to tmpPage buffer
	    	DEBUG(dbgAddr, "byteLeft = " << byteLeft << "\n");

        	ASSERT(src->ReadAt(tmpPage, PageSize, srcPos) == PageSize);
		srcPos = srcPos + PageSize;

		// write PageSize bytes from tmpPage buffer to dest file
		ASSERT(dst->WriteAt(tmpPage, PageSize, dstPos) == PageSize);
		dstPos = dstPos + PageSize;

		byteLeft -= PageSize;
	}
	// copy the rest bytes
	if (byteLeft > 0) {
		// read byteLeft bytes from src file to tmpPage buffer
            	DEBUG(dbgAddr, "byteLeft = " << byteLeft << "\n");
		ASSERT(src->ReadAt(tmpPage, byteLeft, srcPos) == byteLeft);
		srcPos = srcPos + byteLeft;

		// write byteLeft bytes from tmpPage buffer to dest file
		ASSERT(dst->WriteAt(tmpPage, byteLeft, dstPos) == byteLeft);
		dstPos = dstPos + byteLeft;
	}	
}

static 
void clearPage(OpenFile *dst, int dstPos, int size)
{
	char *tmpPage = new char[PageSize];
	int byteLeft = size;
    	
	DEBUG(dbgAddr, "clearPage: dst=" << dst << ", dstPos=" << dstPos << ", size=" << size << "\n");

	// clear page
	for (int i=0; i< PageSize; i++) tmpPage[i] = 0;

	while (byteLeft >= (int)PageSize) {
	    	DEBUG(dbgAddr, "byteLeft = " << byteLeft << "\n");

		// write PageSize bytes from tmpPage buffer to dest file
		ASSERT(dst->WriteAt(tmpPage, PageSize, dstPos) == PageSize);
		dstPos = dstPos + PageSize;

		byteLeft -= PageSize;
	}
	// copy the rest bytes
	if (byteLeft > 0) {
            	DEBUG(dbgAddr, "byteLeft = " << byteLeft << "\n");
		// write byteLeft bytes from tmpPage buffer to dest file
		ASSERT(dst->WriteAt(tmpPage, byteLeft, dstPos) == byteLeft);
		dstPos = dstPos + byteLeft;
	}
}

//----------------------------------------------------------------------
// AddrSpace::Load
// 	Load a user program into memory from a file.
//
//	Assumes that the page table has been initialized, and that
//	the object code file is in NOFF format.
//
//	"fileName" is the file containing the object code to load into memory
//----------------------------------------------------------------------

bool 
AddrSpace::Load(char *fileName) 
{
    OpenFile *executable = kernel->fileSystem->Open(fileName);
    NoffHeader noffH;
    unsigned int size;
    char *tmpPage = new char[PageSize];

    DEBUG(dbgAddr, "fileName: " << fileName << "\n");

    if (executable == NULL) {
	cerr << "Unable to open file " << fileName << "\n";
	return FALSE;
    }
    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) && 
		(WordToHost(noffH.noffMagic) == NOFFMAGIC))
    	SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

// how big is address space?
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size 
			+ UserStackSize;	// we need to increase the size
						// to leave room for the stack
    numPages = divRoundUp(size, PageSize);
//	cout << "number of pages of " << fileName<< " is "<<numPages<<endl;
    size = numPages * PageSize;

    // assume our program size will over the NumPhysPages
    // so, let's not check it here
    // ASSERT(numPages <= NumPhysPages);		// check we're not trying
						// to run anything too big --
						// at least until we have
						// virtual memory

    DEBUG(dbgAddr, "Initializing address space (p, s): " << numPages << ", " << size);

    // let's copy the code and data segments into virtual memory space first.
    // the virtual memory space is actually a file. 
    // the file name will be the program file + "_VM"
    string vmName = (string)fileName;
    vmName += "_VM";
    if (kernel->fileSystem->Create((char *)vmName.c_str()) == TRUE) {
	DEBUG(dbgAddr, "Create file " << vmName << " successful\n");
	virSpace = kernel->fileSystem->Open((char *)vmName.c_str());
    }

    int vmPos;
    int sfPos;

    if (noffH.code.size > 0) {
        DEBUG(dbgAddr, "Initializing code segment.");

	// copy code segment
	vmPos = noffH.code.virtualAddr;
	sfPos = noffH.code.inFileAddr;
	DEBUG(dbgAddr, "noffH.code.virtualAddr = " << noffH.code.virtualAddr << ", noffH.code.size = " << noffH.code.size);
	DEBUG(dbgAddr, "vmPos = " << vmPos << ", sfPos = " << sfPos);
	copyPage(virSpace, vmPos, executable, sfPos, noffH.code.size);

	// original codes
/*
	DEBUG(dbgAddr, noffH.code.virtualAddr << ", " << noffH.code.size);
        	executable->ReadAt(
		&(kernel->machine->mainMemory[noffH.code.virtualAddr]), 
			noffH.code.size, noffH.code.inFileAddr);
*/
    }

    if (noffH.initData.size > 0) {
        DEBUG(dbgAddr, "Initializing data segment.");
	
	// copy data segment
	vmPos = noffH.initData.virtualAddr;
	sfPos = noffH.initData.inFileAddr;
	DEBUG(dbgAddr, "noffH.initData.virtualAddr = " << noffH.initData.virtualAddr << ", noffH.initData.size = " << noffH.initData.size);
	DEBUG(dbgAddr, "vmPos = " << vmPos << ", sfPos = " << sfPos);
	copyPage(virSpace, vmPos, executable, sfPos, noffH.initData.size);

	// original codes
/*
	DEBUG(dbgAddr, noffH.initData.virtualAddr << ", " << noffH.initData.size);
        executable->ReadAt(
		&(kernel->machine->mainMemory[noffH.initData.virtualAddr]),
			noffH.initData.size, noffH.initData.inFileAddr);
*/
    }

    if (noffH.uninitData.size > 0) {
        DEBUG(dbgAddr, "Un-initializing data segment.");
	
	// copy uninit data segment if there is any
	vmPos = noffH.uninitData.virtualAddr;
	sfPos = noffH.uninitData.inFileAddr;
	DEBUG(dbgAddr, "noffH.uninitData.virtualAddr = " << noffH.uninitData.virtualAddr << ", noffH.uninitData.size = " << noffH.uninitData.size);
	DEBUG(dbgAddr, "vmPos = " << vmPos << ", sfPos = " << sfPos);
	copyPage(virSpace, vmPos, executable, sfPos, noffH.uninitData.size);
    }	

    // we should reserve the stack in virtual space
    // our virtual space should be a fixed size, and the stack
    // is in at the bottom of the virtual space.

    if (UserStackSize > 0) {
        DEBUG(dbgAddr, "UserStackSize.");
	// Reserve UserStack space
	// assume our virtual space size is defined by VirtualSpaceSize
	int vmPos = VirtualSpaceSize - UserStackSize;
	
	DEBUG(dbgAddr, "vmPos = " << vmPos); 
	clearPage(virSpace, vmPos, UserStackSize);
    }	

    delete executable;			// close file
    return TRUE;			// success
}

//----------------------------------------------------------------------
// AddrSpace::Execute
// 	Run a user program.  Load the executable into memory, then
//	(for now) use our own thread to run it.
//
//	"fileName" is the file containing the object code to load into memory
//----------------------------------------------------------------------

void 
AddrSpace::Execute(char *fileName) 
{
    if (!Load(fileName)) {
	cout << "inside !Load(FileName)" << endl;
	return;				// executable not found
    }

    //kernel->currentThread->space = this;
    this->InitRegisters();		// set the initial register values
    this->RestoreState();		// load page table register

    kernel->machine->Run();		// jump to the user progam

    ASSERTNOTREACHED();			// machine->Run never returns;
					// the address space exits
					// by doing the syscall "exit"
}


//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void
AddrSpace::InitRegisters()
{
    Machine *machine = kernel->machine;
    int i;

    for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start"
    machine->WriteRegister(PCReg, 0);	

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    machine->WriteRegister(NextPCReg, 4);

   // Set the stack register to the end of the address space, where we
   // allocated the stack; but subtract off a bit, to make sure we don't
   // accidentally reference off the end!
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG(dbgAddr, "Initializing stack pointer: " << numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, don't need to save anything!
//----------------------------------------------------------------------

void AddrSpace::SaveState() 
{
        pageTable=kernel->machine->pageTable;
        numPages=kernel->machine->pageTableSize;

	// Copy all dirty pages back to virtual space,
	// then clean the dirty flag
	for (int i=0; i < NumPhysPages; i++) {
    		if (pageTable[i].dirty == TRUE) {
			unsigned int phyAddr = pageTable[i].physicalPage * PageSize;
			unsigned int virAddr = pageTable[i].virtualPage * PageSize;
			DEBUG(dbgAddr, "Save a dirty page " << pageTable[i].physicalPage << " back to virtual page " << pageTable[i].virtualPage);
			DEBUG(dbgAddr, "Copy phyAddr: " << phyAddr << " to virAddr: " << virAddr);
			virSpace->WriteAt(&(kernel->machine->mainMemory[phyAddr]), PageSize, virAddr);
			// dirty page copied, set the dirty flag to FALSE
			pageTable[i].dirty = FALSE;
		}	
	}
}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void AddrSpace::RestoreState() 
{
    kernel->machine->pageTable = pageTable;
    kernel->machine->pageTableSize = numPages;

    // Copy back all valid pages from virtual space to physical memory
    for (int i=0; i < NumPhysPages; i++) {
    	if (pageTable[i].valid == TRUE) {
		unsigned int phyAddr = pageTable[i].physicalPage * PageSize;
		unsigned int virAddr = pageTable[i].virtualPage * PageSize;
		DEBUG(dbgAddr, "restore physical page " << pageTable[i].physicalPage << " from the virtual page " << pageTable[i].virtualPage);
		DEBUG(dbgAddr, "Copy to phyAddr: " << phyAddr << " from virAddr: " << virAddr);
		virSpace->ReadAt(&(kernel->machine->mainMemory[phyAddr]), PageSize, virAddr);
	}	
    }
}

int AddrSpace::findPage2Use()
{
    static int curPageIdx = 0;
    int result = 0;
    int i = curPageIdx;

    // find an invalid page to use
    do {
	if (pageTable[i].valid == FALSE) {
		// an invalid found
		// Next time, find from the next page
		result = i++;
		curPageIdx = i % NumPhysPages;
		return (result);
	}
	i = ++i % NumPhysPages;
    } while (i != curPageIdx);

    // No invalid page. Find an unused page to replace
    do {
	if (pageTable[i].use == FALSE) {
		// an unused page found
		// Next time, find from the next page
		result = i++;
		curPageIdx = i % NumPhysPages;
		return (result);
	}
	i = ++i % NumPhysPages;
    } while (i != curPageIdx);

/*
    // check usage
    DEBUG(dbgAddr, "Dump pageTable\n");

    do {
	DEBUG(dbgAddr, i << ", virPage: " << pageTable[i].virtualPage << ", phyPage: " << pageTable[i].physicalPage << ", use: " <<pageTable[i].use << ", valid: " << pageTable[i].valid << ", dirty: " << pageTable[i].dirty << ", readOnly: " << pageTable[i].readOnly);
	i = ++i % NumPhysPages;
    } while (i != curPageIdx);
*/

    // no available page to use
    // just choose the current page to replace
    result = curPageIdx++;
    curPageIdx %= NumPhysPages;
    
    // if it is dirty, copy back to virtual space
    if (pageTable[result].dirty == TRUE) {
	unsigned int phyAddr = pageTable[result].physicalPage * PageSize;
	unsigned int virAddr = pageTable[result].virtualPage * PageSize;
	DEBUG(dbgAddr, "Get a dirty page " << result << " to use");
	DEBUG(dbgAddr, "Copy phyAddr: " << phyAddr << " to virAddr: " << virAddr);
	virSpace->WriteAt(&(kernel->machine->mainMemory[phyAddr]), PageSize, virAddr);
    }

    DEBUG(dbgAddr, "result = " << result << ", curPageIdx = " << curPageIdx);
    // return that page index
    return result;
}

// my page fault handler
void AddrSpace::pageFaultHandler()
{
    int i, virtAddr;

    DEBUG(dbgAddr, "enter my page fault handler\n");

    // The virtual address acused the page fault is saved in
    // machine's registers[BadVAddrReg]
    virtAddr = kernel->machine->ReadRegister(BadVAddrReg);
    DEBUG(dbgAddr, "virtual address: " << virtAddr);

    // 1. find a page to use
    i = findPage2Use();
    DEBUG(dbgAddr, "find page to use = " << i);

    // 2. copy virtual page to the physical memory page.
    virtAddr = virtAddr / PageSize;
    DEBUG(dbgAddr, "copy virtual page: " << virtAddr << " to phy page: " << i << " from virtual page address: " << virtAddr*PageSize << ", to mainMemory["<<i*PageSize<<"]");
    virSpace->ReadAt(&(kernel->machine->mainMemory[i*PageSize]), PageSize, virtAddr*PageSize);

/*
    if (pageTable[i].dirty == TRUE) {
	DEBUG(dbgAddr, "old content\n");
    	DEBUG(dbgAddr, "pageTable["<< i << "].virtualPage = " << pageTable[i].virtualPage << ", pageTable[" << i << "].physicalPage = " << pageTable[i].physicalPage);
	DEBUG(dbgAddr, "new content\n");
    	DEBUG(dbgAddr, "pageTable["<< i << "].virtualPage = " << virtAddr << ", pageTable[" << i << "].physicalPage = " << i);
    }
*/

    // 3.set the virtual page number and physical page number in the pageTable
    //   set valid = TRUE, use = FALSE, dirty = FALSE, readonly = ?
    pageTable[i].virtualPage = virtAddr;
    pageTable[i].physicalPage = i;
    pageTable[i].valid = TRUE;
    pageTable[i].use = FALSE;
    pageTable[i].dirty = FALSE;
}
