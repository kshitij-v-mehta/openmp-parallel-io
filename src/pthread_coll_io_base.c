#include <stdio.h>
#include <stdlib.h>
#include <sys/uio.h>
#include "work_manager.h"
#include "defs.h"
#include "access_work_assignment.h"
#include "thread_state_handler.h"
#include "list_utils.h"
#include "heap_sort.h"
#include "contiguity_analyzer.h"
#include<omp.h>

ssize_t _base(FS* fs);
void divide_tasks_sameprms(FS* fs, void* bufptr, long buflen, off_t offset, int signal);

ssize_t 
pthread_coll_io_base(FS *fs)
{
    __ompc_barrier();   //sync here so that the MASTER proceeds only after everyone has provided their data
    if(omp_get_thread_num() < fs->active_threads)
        return (_base(fs));

    return SUCCESS;
}

/**
 * slaves enter wait state.
 * master:
 * if buf and same prms,then merged_targs = master args. len = 1. call work manager.
 * if buf and disc, merged_targs = fs->targs. len = num_threads. call contiguity_analyzer.
 *
 * everybody: access work assignment
 */
ssize_t 
_base(FS *fs) {
    int i, retcode = SUCCESS;
    ssize_t io_bytes;
    if (DEBUG) fprintf(stdout, "Thread %d entered pthread_coll_io_base\n", omp_get_thread_num());

    if (omp_get_thread_num() == MASTER)
        fs->all_quit_flag = 0;
    //fs->sync_flag = 1;
    pthread_barrier_wait(&(fs->barr));  //only active threads sync 

    if (omp_get_thread_num() != MASTER)
        io_bytes = enter_wait_state(fs);

    else //if master
    {
        if ((fs->ioprms->input_type == LIST) && (fs->ioprms->same_prms == YES)) {
            //merged_args = list of MASTER
            fs->merged_targs = (thread_args *) malloc(fs->tlist_args[MASTER].list_size * sizeof (thread_args));
            int i;
            for (i = 0; i < fs->tlist_args[MASTER].list_size; i++) {
                fs->merged_targs[i].bufptr = (char*) fs->tlist_args[MASTER].iov[i].iov_base;
                fs->merged_targs[i].buflen = fs->tlist_args[MASTER].iov[i].iov_len;
                if(fs->ioprms->merge_type == ALL)
			fs->merged_targs[i].offset = fs->tlist_args[MASTER].offset_list[i];
            }
            fs->merged_args_len = fs->tlist_args[MASTER].list_size;
        } else if ((fs->ioprms->input_type == LIST) && (fs->ioprms->same_prms == NO) /*&& (fs->ioprms->contig == YES)*/) {
            merge_args_lists(fs);
        } else if ((fs->ioprms->input_type == BUF) && (fs->ioprms->same_prms == YES)) {
            fs->merged_targs = &(fs->targs[MASTER]);
            fs->merged_args_len = 1;
        } else if ((fs->ioprms->input_type == BUF) && (fs->ioprms->same_prms == NO)) {
            fs->merged_targs = fs->targs;
            fs->merged_args_len = omp_get_num_threads();
        }

        //Heap sort if explicit offsets specified
        if (fs->ioprms->merge_type == ALL)
            heap_sort(fs->merged_targs, fs->merged_args_len);

        //
        if (fs->ioprms->input_type == BUF && fs->ioprms->same_prms == YES) {
            divide_tasks_sameprms(fs, fs->merged_targs[0].bufptr, fs->merged_targs[0].buflen, fs->merged_targs[0].offset, FINISH);
        } else if (fs->ioprms->contig == YES || fs->merged_args_len == 1)
            work_manager(fs, fs->merged_targs[0].bufptr, fs->merged_targs[0].buflen, fs->merged_targs[0].offset, FINISH);
        else
        {
            if(fs->ioprms->hint <= 0)
                contiguity_analyzer(fs);
            else
            {
                //calculate total buflen
                size_t total_buflen = 0;
                for(i=0; i<fs->merged_args_len; i++)
                    total_buflen += fs->merged_targs[i].buflen;
                work_manager(fs, fs->merged_targs[0].bufptr, total_buflen, fs->merged_targs[0].offset, FINISH);
            }
        }

        io_bytes = access_work_assignment(fs);
		
        //reset and cleanup steps
        fs->ioassignments[MASTER].status = RESET;
        fs->ioassignments[MASTER].listlen = 0;
        fs->ioassignments[MASTER].completed = 0;
    }
	
    //wait for everyone to be done, then free listiopool elements
	//__ompc_barrier();
    pthread_barrier_wait(&(fs->barr));
    if (omp_get_thread_num() == MASTER) {
        contigFileBlock* iterator = fs->listiopool.contigFileBlockList;
        contigFileBlock* cur_tmp_node = iterator;
        while (iterator != NULL) {
            cur_tmp_node = iterator;
            iterator = iterator->next;
            free(cur_tmp_node->iov);
            free(cur_tmp_node);
        }
        fs->listiopool.listsize = 0;

        if (fs->ioprms->input_type == LIST)
            free(fs->merged_targs);	
    }
	
    //__ompc_barrier();
    pthread_barrier_wait(&(fs->barr));
    return io_bytes; 
}

/*
 * This will divide the io assignment equally between all threads (last thread might do more)
 */
void divide_tasks_sameprms(FS* fs, void* bufptr, long buflen, off_t offset, int signal) {
    int i, boundary;
    long chunk, bytes_remaining=buflen, bytes_done=0;
    signal=CONTINUE;

    if (buflen > fs->max_write_block_size)
    {
        if (buflen/fs->active_threads > fs->max_write_block_size) {
            chunk    = buflen/fs->active_threads;
            boundary = fs->active_threads-1;
        }
        else {
            chunk    = fs->max_write_block_size;
            boundary = buflen/fs->max_write_block_size;
        }

        for (i=0; i<boundary; i++)
        {
            bytes_remaining -= chunk;
            if (!bytes_remaining) signal = FINISH;
            work_manager (fs, bufptr+bytes_done, chunk, offset+bytes_done, signal);
            bytes_done += chunk;
        }
        if(bytes_remaining)
            work_manager (fs, bufptr+bytes_done, bytes_remaining, offset+bytes_done, FINISH);
    }
    else
        work_manager (fs, bufptr, buflen, offset, FINISH);
}

