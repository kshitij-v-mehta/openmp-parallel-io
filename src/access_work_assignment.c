#include <stdio.h>
#include <stdlib.h>
#include <sys/uio.h>
#include <sys/types.h>
#include "io_functions.h"
#include "thread_state_handler.h"
#include "stripe_handler.h"
#include "io_preface.h"
#include "list_utils.h"
#ifdef __PLFS_H_
#include <plfs.h>
#endif
#include "defs.h"
#include<omp.h>

//static iovt * create_iovt(void *bufptr, long buflen, off_t offset);
//static iovt* create_iovec_arr_listio(FS *fs);
//static void print_iovec_list(iovt* vect);

int 
access_work_assignment(FS *fs) 
{
    int retcode = SUCCESS;
    ssize_t io_bytes;

    //get from global pool?
    contigFileBlock *iterator;
    iterator = fs->listiopool.contigFileBlockList;

    //If omp_file_open_all was called outside par region, 
    //all fds/pfds are gonna be -1/NULL, 
    //in this case, assign them to the MASTER's
    //
    //This actually wont work since if it was called outside the par region, 
    //fs->fd[tid] does not exist at all, and will segfault.
    if(fs->plfs)
    {
#ifdef __PLFS_H_
        if(fs->pfd[omp_get_thread_num()] == NULL)
            fs->pfd[omp_get_thread_num()] = fs->pfd[MASTER];
#endif
    } 
    else 
    {
        if(fs->fd[omp_get_thread_num()] == -1)
            fs->fd[omp_get_thread_num()] = fs->fd[MASTER];
    }

    while (fs->ioassignments[omp_get_thread_num()].completed < fs->ioassignments[omp_get_thread_num()].listlen) {
        if (iterator->assigned_to == omp_get_thread_num()) {
            if (fs->ioprms->io_type == WRITE)
            {
                if(fs->plfs) 
                {
#ifdef __PLFS_H_
                    io_bytes = _plfs_write(fs->pfd[omp_get_thread_num()], iterator->iov->iov_base, iterator->iov->iov_len, iterator->offset, fs);
#endif
                }
                else
                {
                    if (fs->ioprms->input_type == LIST)
                        io_bytes = _writev(fs->fd[omp_get_thread_num()], iterator->iov, iterator->size, iterator->offset, fs);
                    else if (fs->ioprms->input_type == BUF)
                        io_bytes = _pwrite(fs->fd[omp_get_thread_num()], iterator->iov->iov_base, iterator->iov->iov_len, iterator->offset, fs);
                } 
            }
            else if (fs->ioprms->io_type == READ) 
            {
                if(fs->plfs) {
#ifdef __PLFS_H_
                    io_bytes = _plfs_read(fs->pfd[omp_get_thread_num()], iterator->iov->iov_base, iterator->iov->iov_len, iterator->offset, fs);
#endif
                }
                else
                {
                    if (fs->ioprms->input_type == LIST)
                        io_bytes = _readv(fs->fd[omp_get_thread_num()], iterator->iov, iterator->size, iterator->offset, fs);
                    else if (fs->ioprms->input_type == BUF)
                        io_bytes = _pread(fs->fd[omp_get_thread_num()], iterator->iov->iov_base, iterator->iov->iov_len, iterator->offset, fs);
                } 
            }
            iterator->assigned_to = -1;
            fs->ioassignments[omp_get_thread_num()].completed++;
        }
        if (iterator->next != NULL)
            iterator = iterator->next;
    }
    //At this point, completed should be = listlen. Not checking though. 

	//io_bytes is actually an error code ERROR/SUCCESS. TODO: Change var name to retval or something
    return io_bytes;
}

/*int access_work_assignment(FS *fs) {
    int retcode = SUCCESS;
    /*
     * case BUF:
     *          check ioassignment , get the bufptr, len and straight pass it to the low level io functions. bypass io_preface
     * case LIST:
     *          check pool, parse it till completed = listlen, set to offset, pass iovt array to io_function, bypass preface
     * case MEM:
     *          doesnt matter if mem or all. offset set in teh ioassignments will take care of it.
     * case ALL:
     */ /*
    if (fs->ioprms->input_type == LIST) {

        //get from global pool?
        contigFileBlock *iterator;
        iterator = fs->listiopool.contigFileBlockList;
        while (fs->ioassignments[omp_get_thread_num()].completed < fs->ioassignments[omp_get_thread_num()].listlen) {
            if (iterator->assigned_to == omp_get_thread_num()) {
                if (fs->ioprms->io_type == WRITE)
                    _writev(fs->fd[omp_get_thread_num()], iterator->iov, iterator->size, iterator->offset, fs);
                else if (fs->ioprms->io_type == READ)
                    _readv(fs->fd[omp_get_thread_num()], iterator->iov, iterator->size, iterator->offset, fs);

                fs->ioassignments[omp_get_thread_num()].completed++;
            }
            if (iterator->next != NULL)
                iterator = iterator->next;
        }
        //At this point, completed should be = listlen. Not checking though. 
    } else {// BUF:
#ifdef __PLFS_H_
        while (fs->ioassignments[omp_get_thread_num()].completed < fs->ioassignments[omp_get_thread_num()].listlen) {
            if (fs->ioprms->io_type == WRITE)
                _plfs_write(fs->pfd[omp_get_thread_num()], fs->ioassignments[omp_get_thread_num()].bufptr, fs->ioassignments[omp_get_thread_num()].buflen, fs->ioassignments[omp_get_thread_num()].offset, fs);
            else
                _plfs_read(fs->pfd[omp_get_thread_num()], fs->ioassignments[omp_get_thread_num()].bufptr, fs->ioassignments[omp_get_thread_num()].buflen, fs->ioassignments[omp_get_thread_num()].offset, fs);

            fs->ioassignments[omp_get_thread_num()].completed++;
        }
#else
        while (fs->ioassignments[omp_get_thread_num()].completed < fs->ioassignments[omp_get_thread_num()].listlen) {
            if (fs->ioprms->io_type == WRITE)
                _pwrite(fs->fd[omp_get_thread_num()], fs->ioassignments[omp_get_thread_num()].bufptr, fs->ioassignments[omp_get_thread_num()].buflen, fs->ioassignments[omp_get_thread_num()].offset, fs);
            else
                _pread(fs->fd[omp_get_thread_num()], fs->ioassignments[omp_get_thread_num()].bufptr, fs->ioassignments[omp_get_thread_num()].buflen, fs->ioassignments[omp_get_thread_num()].offset, fs);

            fs->ioassignments[omp_get_thread_num()].completed++;
        }
#endif
    }
    return retcode;
} */

/**
 * Check work assignment. Check stripe options.
 * If stripe, then go to stripe handler.
 * else create iovct
 * delete this assignment node, which was just processed. lock before deleting.
 */

/*
   int access_work_assignment(FS *fs)
   {
   if(DEBUG) fprintf(stdout, "Thread %d inside access work assignment\n", omp_get_thread_num());
   int mytid = omp_get_thread_num();

//		Another f***ing temporary fix. 
//		In ListIO, no work is assigned to you (unless your id is 1). Everyone 'shares' work amongst themselves (another temp. fix)
//		So, skip the ioassignments[myid].completed?? check and proceed to dividing the work

if(fs->ioprms->input_type == LIST)
{
if(fs->all_quit_flag == 1)
{					
//create array of iovecs
iovt* vect = create_iovec_arr_listio(fs);
if(DEBUG) print_iovec_list(vect);
io_preface(fs, vect);

fs->ioassignments[omp_get_thread_num()].completed = 0;
fs->ioassignments[omp_get_thread_num()].listlen = 0;
}
}

else //if not list interface
{
while(fs->ioassignments[mytid].completed < fs->ioassignments[mytid].listlen)
{
if(fs->ioprms->merge_type == MEM)
{
if(fs->ioprms->MEMSTR == YES)
{
stripe_handler(fs, fs->ioassignments[mytid].iovlisthead->bufptr, fs->ioassignments[mytid].iovlisthead->buflen, fs->ioassignments[mytid].iovlisthead->offset);
}
else
{
if(DEBUG) fprintf(stdout, "Thread %d listlen=%d\n", mytid, fs->ioassignments[mytid].listlen);
if(fs->ioprms->input_type != LIST)
{
iovt *vect = create_iovt(fs->ioassignments[mytid].iovlisthead->bufptr, fs->ioassignments[mytid].iovlisthead->buflen, fs->ioassignments[mytid].iovlisthead->offset);
io_preface(fs, vect);
}
else //if(fs->ioprms->input_type == LIST). This will never be executed. 
{
if(fs->all_quit_flag == 1)
{					
//create array of iovecs
iovt* vect = create_iovec_arr_listio(fs);
if(DEBUG) print_iovec_list(vect);
io_preface(fs, vect);
}
}	
}
}
else //All Merge
{
if(fs->ioprms->ALLSTR == YES)
{
if(DEBUG) fprintf(stdout, "Thread %d inside access_work_assignment for all merge. Now calling stripe handler.\n", omp_get_thread_num());
stripe_handler(fs, fs->ioassignments[mytid].iovlisthead->bufptr, fs->ioassignments[mytid].iovlisthead->buflen, fs->ioassignments[mytid].iovlisthead->offset);
}
else
{
if(DEBUG) fprintf(stdout, "Thread %d inside access_work_assignment for all merge. Striping switched off. Creating iovt and calling preface.\n", omp_get_thread_num());
iovt *vect = create_iovt(fs->ioassignments[mytid].iovlisthead->bufptr, fs->ioassignments[mytid].iovlisthead->buflen, fs->ioassignments[mytid].iovlisthead->offset);
io_preface(fs, vect);
}
}
fs->ioassignments[mytid].status = RESET;
fs->ioassignments[mytid].listlen = 0;
fs->ioassignments[mytid].completed = 0;
fs->ioassignments[mytid].iovlisthead = NULL;
//delete_top_node(fs);
if(DEBUG)
    fprintf(stdout, "Thread %d inside access_work_assignment. Deleted top node. Now iovlisthead==NULL = %d, listlen=%d\n",mytid, (fs->ioassignments[mytid].iovlisthead == NULL), fs->ioassignments[mytid].listlen);
    }
}

//Todo: return error_code, not SUCCESS
return SUCCESS;
}
*/
//To be removed, no longer being used
/*static iovt * create_iovt(void *bufptr, long buflen, off_t offset) {
  iovt *vect;
  vect = (iovt *) malloc(sizeof (iovt));

  vect->iovlist = (struct iovec *) malloc(sizeof (struct iovec));
  vect->iovlist->iov_base = bufptr;
  vect->iovlist->iov_len = buflen;
  vect->offset = offset;
  vect->listlen++;

  return vect;
  } */


//This has to go.
//To be removed, no longer being used
/*static iovt* create_iovec_arr_listio(FS *fs) {
//	Get the no. of elements that you will write
//	Get the index in the list from where you should write
//	Traverse to that index
//	Create iovec list from this position
//
//get arr size. for e.g. if listlen=27, n_threads=8, arr_size=27/8 = 3, plus threads 0,1,2 get one more element
int i;
int arr_size = fs->ioassignments[1].listlen / omp_get_num_threads();
if (omp_get_thread_num() < fs->ioassignments[1].listlen % omp_get_num_threads())
arr_size++;

int arrIndex = arr_size * omp_get_thread_num();
arrIndex += (fs->ioassignments[1].listlen % omp_get_num_threads()) % (omp_get_thread_num() + 1);
if (DEBUG) {
printf("T%d in create_iovec_arr_listio, arr_size: %d, arrIndex: %d\n", omp_get_thread_num(), arr_size, arrIndex);
}

iovt* vect = (iovt*) malloc(arr_size * sizeof (iovt));
vect->iovlist = (struct iovec*) malloc(arr_size * sizeof (struct iovec));

//traverse list now to get to arrIndex. TODO: Should change list to array later on. 
iovecnode *iterator;
iterator = (iovecnode *) (fs->ioassignments[1].iovlisthead); //TODO: Remove hardcoded value 1
int listIndex = 0;
//traverse to element arrIndex of the list
while (listIndex < arrIndex) {
iterator = iterator->next;
listIndex++;
}
//add elements from this position to the iovec list
i = 0;
vect->listlen = arr_size;
vect->offset = iterator->offset;
while (i < arr_size) {
vect->iovlist[i].iov_base = iterator->bufptr;
vect->iovlist[i].iov_len = iterator->buflen;
iterator = iterator->next;
i++;
}

return vect;
}*/

//To be removed, no longer being used
/*static void print_iovec_list(iovt* vect) {
  int i;
  printf("******Thread %d printing iovec list******\n", omp_get_thread_num());
  for (i = 0; i < vect->listlen; i++) {
  printf("T%d, index: %d, iov_base: %p, iov_len: %ld\n", omp_get_thread_num(), i, vect->iovlist[i].iov_base, vect->iovlist[i].iov_len);
  }
  printf("******Thread %d done printing iovec list******\n", omp_get_thread_num());
  }*/
