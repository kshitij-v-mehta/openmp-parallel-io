#include <stdio.h>
#include <stdlib.h>
#include <sys/uio.h>
#include "defs.h"

/**
 * Taken from an online source.
 * Status: Complete.
 * Sorts acc. to offset
 */
void heap_sort(thread_args *targs, int array_size)
{
    if(DEBUG) fprintf(stdout, "Master heap sorting input array of size %d\n", array_size);
    if(array_size == 1)
        return;

    unsigned int n = array_size, i = n/2, parent, child;
    char* t; long l; off_t of;

    for (;;) { /* Loops until arr is sorted */
        if (i > 0) { /* First stage - Sorting the heap */
            i--;           /* Save its index to i */
            t = targs[i].bufptr;    /* Save parent value to t */
            l = targs[i].buflen;
            of = targs[i].offset;
        } else {     /* Second stage - Extracting elements in-place */
            n--;           /* Make the new heap smaller */
            if (n == 0) return; /* When the heap is empty, we are done */
            t = targs[n].bufptr;    /* Save last value (it will be overwritten) */
            l = targs[n].buflen;
            of = targs[n].offset;
            targs[n].bufptr = targs[0].bufptr; /* Save largest value at the end of arr */
            targs[n].buflen = targs[0].buflen;
            targs[n].offset = targs[0].offset;
        }

        parent = i; /* We will start pushing down t from parent */
        child = i*2 + 1; /* parent's left child */

        /* Sift operation - pushing the value of t down the heap */
        while (child < n) {
            if (child + 1 < n  &&  targs[child + 1].offset > targs[child].offset) {
                child++; /* Choose the largest child */
            }
            if (targs[child].offset > of) { /* If any child is bigger than the parent */
                targs[parent].bufptr = targs[child].bufptr; /* Move the largest child up */
                targs[parent].buflen = targs[child].buflen;
                targs[parent].offset = targs[child].offset;
                parent = child; /* Move parent pointer to this child */
                child = parent*2 + 1; /* Find the next child */
            } else {
                break; /* t's place is found */
            }
        }
        targs[parent].bufptr = t; /* We save t in the heap */
        targs[parent].buflen = l;
        targs[parent].offset = of;
    }
}

