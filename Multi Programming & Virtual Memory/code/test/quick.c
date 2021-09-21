#include "array.h"
#include "syscall.h"

void quick(int array[], int begin, int end)
{
    if(begin<end)
    {
        int pivot, i, j;
        
        pivot = array[(begin+end)/2];

        i = begin-1;
        j = end+1;

        while(1)
        {
            do
            {
                i = i+1;
            }while(array[i]<pivot);
            do
            {
                j = j-1;
            }while(array[j]>pivot);

            if(i>=j)
                break;

            array[i] = array[i]^array[j];
            array[j] = array[i]^array[j];
            array[i] = array[i]^array[j];
        }

        quick(array, begin, j);
        quick(array, j+1, end);
    }
}

int main()
{
    int i;

    quick(array, 0, 1023);

    for(i=816; i<821; i++)
        PrintInt(array[i]);

    Exit(1);
}
