#include "array.h"
#include "syscall.h"

int main()
{
    int i, j, tmp, is_swapped;

    for(i=0; i<1024; i++)
    {
        is_swapped = 0;
        for(j=0; j<1023; j++)
        {
            if(array[j] > array[j+1])
            {
                is_swapped = 1;
                array[j] = array[j]^array[j+1];
                array[j+1] = array[j]^array[j+1];
                array[j] = array[j]^array[j+1];
            }
        }

        if(!is_swapped)
            break;
    }

    for(i=907; i<912; i++)
        PrintInt(array[i]);

    Exit(0);
}
