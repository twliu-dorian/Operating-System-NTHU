#include "array.h"
#include "syscall.h"

void merge(int array[], int begin, int end, int temp[])
{
    if(end-begin == 0)
        return;
    else
    {
        int middle, i, j, k;

        middle = (begin+end)/2;

        merge(array, begin, middle, temp);
        merge(array, middle+1, end, temp);

        i = begin;
        j = middle+1;

        for(k=begin; k<=end; k++)
        {
            if(i<=middle && (j>end || array[i] <= array[j]))
            {
                temp[k] = array[i];
                i++;
            }
            else
            {
                temp[k] = array[j];
                j++;
            }
        }
        
        for(k=begin; k<=end; k++)
            array[k] = temp[k];
    }
}

int main()
{
    int temp[1023], i;

    merge(array, 0, 1023, temp);

    for(i=22; i<27; i++)
        PrintInt(array[i]);

    Exit(2);
}
