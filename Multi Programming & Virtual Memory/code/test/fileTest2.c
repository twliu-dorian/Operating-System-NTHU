#include "syscall.h"

int main(void)
{
	char test[] = "nachosFile";
	int i, count, result;
	int size = (sizeof(test) / sizeof(test[0])) - 1;	// skip \0
	char read[100];
	
	result = Open("file1");
	if(result < 0) {
		Msg("Opening file failed!");
	}

	count = Write(test, size);
	if (count != size) Msg("Writing file failed!");

	result = Close();
	if (result != 1) Msg("Closing file failed!");


	result = Open("file1");
	if(result < 0) {
		Msg("Opening file failed!");
	}
	count = Read(read, size);
	if (count != size) Msg("Reading file failed!");
	
	result = Close();
	if (result != 1) Msg("Closing file failed!");
	
	for (i = 0; i < size; ++i) {
		if (read[i] != test[i]) Msg("Reading wrong result:'(");
	}
	Msg("=====Congratulations!!!=====");
}
