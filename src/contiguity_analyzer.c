#include <stdio.h>
#include <stdlib.h>
#include <sys/uio.h>
#include "work_manager.h"
#include "defs.h"
#include "thread_state_handler.h"
#include <omp.h>

//#include buf_store
//#include pthread_utils
//#include work_manager

//check if struct iovec* buf_store works

//fs->merged_args_len is the fs->merged_args_len of the buf_store
//This function cannot handle an empty buf_store, error is not captured.
//This sends a FINISH signal to the work_manager once it has finished processing all elements of buf_store

static void buffer_contiguity_analyzer(FS* fs, int bufStartIndex, int bufEndIndex, int signal);
static void file_offset_contiguity_analyzer(FS* fs, int startIndex, int endIndex);

/*
static int check_if_contiguous(FS *fs, int startIndex, int curIndex);

int contiguity_analyzer(FS *fs) 
{
    if(DEBUG) {
        fprintf(stdout, "Master Inside Contiguity Analyzer\n" );
    }

    // 1. Create variables
    int startIndex = 0, curIndex = 0;
    off_t bufLen = 0;
    int done = 0, cont = 1, contiguous = 0, empty_values = 1;

    while(!done)
    {
        cont = 1;
        empty_values = 1;

        // 2. Check if the sorted buf_store[] array has any empty elements. If yes, then go to the first non-empty element
        while(empty_values && !done)
        {
            curIndex = startIndex;
            if(startIndex > fs->merged_args_len-1)
            {
                done = 1;
                invoke_all(fs);
            }
            else if (fs->merged_targs[startIndex].bufptr == EMPTY)
                startIndex++;	   		
            else
                empty_values = 0;
        }

        // This means that all elements in the buf_store are empty. Practically not possible, but good to have a check here
        if(done)
            break;   //this break statement SHOULD be for the while(!done) main loop only

        // 3. Step 3
        else
        {	    
            bufLen = fs->merged_targs[startIndex].buflen;
            curIndex ++;
            while(cont)
            {
                // 3.1 If there is only one non-empty element, then assign a write and quit
                if(curIndex > fs->merged_args_len-1)
                {
                    if(DEBUG) printf("Master analyzed all elements for contiguity start %d, end %d. Calling work manager.\n", startIndex, curIndex-1);
                    work_manager(fs, fs->merged_targs[startIndex].bufptr, bufLen, fs->merged_targs[startIndex].offset, FINISH);
                    cont = 0;
                    done = 1;
                }
                else 
                {
                    // 3.2 Check for contiguity between startIndex and curIndex
                    contiguous = check_if_contiguous(fs, curIndex-1, curIndex);
                    if(contiguous)
                    {
                        // 3.2.1 Increment curIndex. In the next step, check for contiguity between curIndex and curIndex - 1
                        if(DEBUG) printf("Contiguity found\n");
                        bufLen += fs->merged_targs[curIndex].buflen;
                        curIndex++;
                    }
                    else
                    {
                        // 3.2.2 If discontiguous, assign write and reset startIndex and curIndex
                        //if(DEBUG) printf("\nDiscontiguity found at %x and %x\n", fs->merged_targs[startIndex].bufptr, fs->merged_targs[curIndex].bufptr);
                        if(DEBUG) fprintf(stdout, "Discontiguity found\n");
                        work_manager(fs, fs->merged_targs[startIndex].bufptr, bufLen, fs->merged_targs[startIndex].offset, CONTINUE);
                        cont = 0;
                        startIndex = curIndex;
                    }
                }
            }
        }
    }

    //TODO: return error_code
    return SUCCESS;
}

static int check_if_contiguous(FS *fs, int startIndex, int curIndex)
{
    if((fs->merged_targs[startIndex].bufptr + fs->merged_targs[startIndex].buflen) != fs->merged_targs[curIndex].bufptr)
        return 0;
    if(fs->ioprms->merge_type == ALL)
    {
        if(DEBUG) printf("Checking for contiguity in all merge type, offsets: %lu %lu\n", (unsigned long) fs->merged_targs[startIndex].offset, (unsigned long) fs->merged_targs[curIndex].offset);
        if((fs->merged_targs[startIndex].offset + fs->merged_targs[startIndex].buflen) != fs->merged_targs[curIndex].offset)
            return 0;
    }
    return 1;
} */

int contiguity_analyzer(FS *fs)
{
    int retcode = SUCCESS;
    
    if(DEBUG) {
        fprintf(stdout, "Master Inside Contiguity Analyzer\n" );
    }

    // 1. Create variables
    int startIndex = 0, curIndex = 0;//, done = 0;
    off_t bufLen = 0;

    //Boundary conditions check: 0 elements, all empty elements
    /*  
     */  

    bufLen = fs->merged_targs[startIndex].buflen;
    curIndex ++;
    
    //If there is only one non-empty element, then assign an io request and quit
    if(curIndex > fs->merged_args_len-1)
    {
        if(DEBUG) printf("Master analyzed all elements for contiguity start %d, end %d. Calling work manager.\n", startIndex, curIndex-1);
        work_manager(fs, fs->merged_targs[startIndex].bufptr, bufLen, fs->merged_targs[startIndex].offset, FINISH);
        //done = 1;
    }

    //more than 1 non-empty element in the list
    else 
    {
        if(fs->ioprms->merge_type == ALL) //explicit offsets
        {
            //input list should be sorted in order of file offsets
            file_offset_contiguity_analyzer(fs, startIndex, fs->merged_args_len-1);
        }

        else //implicit offsets
        {
            buffer_contiguity_analyzer(fs, startIndex, fs->merged_args_len-1, FINISH);
        }
    }
    
    return retcode;
} 

static void buffer_contiguity_analyzer(FS* fs, int bufStartIndex, int bufEndIndex, int signal)
{
    //Analyze buf contiguity between the given indices
    //Call WM for each contig block found. See signal to know when FINISHed
    
    int i,j, restart = 1;
    long buflen = 0;

    int intermediate_signal = CONTIG_FILE_BLOCK;
    if(fs->ioprms->input_type == BUF)
        intermediate_signal = CONTINUE;

    for(i=bufStartIndex,j=bufStartIndex; j<bufEndIndex; j++)
    {
        if((fs->merged_targs[j].bufptr + fs->merged_targs[j].buflen) != fs->merged_targs[j+1].bufptr)
        {
            work_manager(fs, fs->merged_targs[i].bufptr, buflen + fs->merged_targs[j].buflen, fs->merged_targs[i].offset, intermediate_signal);
            i = j+1;
            buflen = 0;
			restart = 1;
        }
        else
		{
            buflen += fs->merged_targs[i].buflen;
			restart = 0;
		}
    }
   
	if(signal == FINISH)
	{
		//lone element? If you just send finish, WM wont know if this element is CONTIG or not
		if(restart == 1)
			work_manager(fs, fs->merged_targs[i].bufptr, buflen + fs->merged_targs[bufEndIndex].buflen, fs->merged_targs[i].offset, FINISH);
		else	//group of elements 
			work_manager(fs, fs->merged_targs[i].bufptr, buflen + fs->merged_targs[bufEndIndex].buflen, fs->merged_targs[i].offset, CONTIG_AND_FINISH);
	}
	else
		work_manager(fs, fs->merged_targs[i].bufptr, buflen + fs->merged_targs[bufEndIndex].buflen, fs->merged_targs[i].offset, signal); 
}

static void file_offset_contiguity_analyzer(FS* fs, int startIndex, int endIndex)
{
    //Analyze contiguity in file. Index will probably be the first and the last one in the array
    //For every contiguous block , pass it on to the buffer_contiguity_analyzer
    int i,j;

    for(i=startIndex,j=startIndex; j<endIndex; j++)
    {
        if((fs->merged_targs[j].offset + fs->merged_targs[j].buflen) != fs->merged_targs[j+1].offset)
        {
            buffer_contiguity_analyzer(fs, i, j, CONTINUE);
            i = j+1;
        }
    }

    buffer_contiguity_analyzer(fs, i, endIndex, FINISH);
}

