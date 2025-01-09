#include <stdio.h>
#include <stdlib.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __PLFS_H_
#include <plfs.h>
#endif
#include "defs.h"
#include "list_utils.h"
#include "thread_state_handler.h"
#include<omp.h>

static int incr_slave(FS *);
//static void set_nxt_slave(FS *);
static int reset_nxt_slave(FS *);
static int add_to_listiopool(FS* fs, void* bufptr, long buflen, off_t offset, int signal);
static contigFileBlock* create_listio_fileBlock(FS* fs);

/**
 * get internal offset first using lseek.
 * The bufptr, buflen are added to the list of task assignments for the next slave.
 * Common: set slave status to ASSIGNED. wake slave. If FINISH, wake all.

 * 05.20.11
 Will have to change this so that it handles the listio case as well. 

*/
int 
work_manager(FS *fs, 
            void *bufptr, 
            long buflen, 
            off_t _offset, 
            int signal) 
{
    int retcode = SUCCESS;
    off_t offset = _offset;

    //Set fs->internal_offset
    if (fs->ioprms->merge_type == MEM) {
        if (fs->internal_offset == -1) { //TODO: This is a very sensitive statement 
        //if (_offset == -1) {
            if (fs->ioprms->io_type == READ) {
                fs->internal_offset = 0;
            }
            else {
                if(fs->plfs)
                {
#ifdef __PLFS_H_
                    struct stat filestat;
                    if(0 > plfs_getattr(fs->pfd[MASTER], fs->ioprms->filename, &filestat, 1))
                    {
                        perror("Could not plfs_getattr in WM");
                        exit(-1);
                    }
                    fs->internal_offset = filestat.st_size;
                    fs->internal_offset = 0;
#endif
                }else{
                    fs->internal_offset = lseek(fs->fd[MASTER], 0, SEEK_CUR);
                }
            }
            offset = fs->internal_offset;
            fs->internal_offset += buflen;  //now on use offset only, not fs->internal_offset
        }
        else {
            offset = fs->internal_offset;
            fs->internal_offset += buflen;
        }
    }

    //What happens when different merge_type interfaces are called for the same FS? fs->internal_offset must have the highest offset value
    /*else {
      if ((offset + buflen) > fs->internal_offset)
      fs->internal_offset = offset + buflen;
      }*/

    //check if listio or regulario
    /*if (fs->ioprms->input_type != LIST) {
      fs->ioassignments[fs->nxt_slave].bufptr = bufptr;
      fs->ioassignments[fs->nxt_slave].buflen = buflen;
      fs->ioassignments[fs->nxt_slave].offset = offset;
      fs->ioassignments[fs->nxt_slave].listlen ++ ;
      fs->ioassignments[fs->nxt_slave].status = ASSIGNED;

      invoke_thread(fs, fs->nxt_slave);
      incr_slave(fs);

      if(signal == FINISH || signal == CONTIG_AND_FINISH)
      {
      invoke_all(fs);
      reset_nxt_slave(fs);
      }
      } else //ListIO 
      { */
    /* Algo:
       if(signal == CONTIG_FILE_BLOCK)
       add_to_listiopool(bufptr, buflen, offset, signal);
       else if (signal == CONTIG_AND_FINISH)
       add_to_listiopool(bufptr, buflen, offset, signal);
       else if (signal == CONTINUE)	//last contig buf of the block
       {
    //This is the last buf of the block. Set slave status and wake the bastard.
    add_to_listiopool(bufptr, buflen, offset, signal);			
    fs->ioassignments[fs->nxt_slave].status = ASSIGNED;
    fs->ioassignments[fs->nxt_slave].listlen ++ ;
    incr_slave(fs);
    }
    else //(signal == FINISH)
    {
    add_to_listiopool(bufptr, buflen, offset, signal);
    fs->ioassignments[fs->nxt_slave].status = ASSIGNED;
    fs->ioassignments[fs->nxt_slave].listlen ++ ;
    invoke_all(fs);
    reset_nxt_slave(fs);
    }

    if((signal == FINISH) || (signal == CONTIG_AND_FINISH))
    {
    invoke_all(fs);
    reset_nxt_slave(fs);
    }*/

    add_to_listiopool(fs, bufptr, buflen, offset, signal);

    if(fs->ioprms->input_type == LIST)
    {
        if (signal != CONTIG_FILE_BLOCK) //CONTINUE/CONTIG_AND_FINISH/FINISH - then this is the last contig buf of the block
        {
            fs->ioassignments[fs->nxt_slave].status = ASSIGNED;
            fs->ioassignments[fs->nxt_slave].listlen++;
            invoke_thread(fs, fs->nxt_slave);
            incr_slave(fs);
        }
    }
    else //BUF
    {
        fs->ioassignments[fs->nxt_slave].status = ASSIGNED;
        fs->ioassignments[fs->nxt_slave].listlen++;
        invoke_thread(fs, fs->nxt_slave);
        incr_slave(fs);
    }

    if ((signal == CONTIG_AND_FINISH) || (signal == FINISH)) {
        invoke_all(fs);
        reset_nxt_slave(fs);
    }
    //}
    return retcode;
}

/*int work_manager(FS *fs, void *bufptr, long buflen, off_t offset, int signal)
  {
  int retcode = SUCCESS;
  if(DEBUG) fprintf(stdout, "Master inside work manager\n");
  if(fs->internal_offset == -1)   //TODO: This is a very sensitive statement
  fs->internal_offset = lseek(fs->fd[MASTER], 0, SEEK_CUR);

//set_nxt_slave(fs);

if(fs->ioprms->merge_type == MEM)
{
if(DEBUG) 
fprintf(stdout, "Master setting assignment for thread %d\n", fs->nxt_slave);
add_to_assignment_list(fs, bufptr, buflen, fs->internal_offset);
fs->internal_offset += buflen;
}
else
add_to_assignment_list(fs, bufptr, buflen, offset);

fs->ioassignments[fs->nxt_slave].status = ASSIGNED;
//All data set for slave 1 for listIO. Dont wake up slaves.
if(fs->ioprms->input_type != LIST)
{
invoke_thread(fs, fs->nxt_slave);
incr_slave(fs);
}
if(signal == FINISH)
{
invoke_all(fs);
reset_nxt_slave(fs);
}
return retcode;
}*/

/*static void set_nxt_slave(FS *fs) {
//this function could be used later to set the slave in order to maintain locality of buf pointers.
//Currently leaving it empty.
}*/

static int incr_slave(FS *fs) {
    int retcode = SUCCESS;
    if(fs->nxt_slave == fs->active_threads-1)
        fs->nxt_slave = MASTER;
    else 
        fs->nxt_slave++;

    return retcode;
}

static int reset_nxt_slave(FS *fs) {
    fs->nxt_slave = 0;
    if (omp_get_num_threads() > 1)
        if(fs->active_threads>1)
            fs->nxt_slave = 1;

    return SUCCESS;
}

/*
 * This piece of shit has to be remodelled
 */
static int add_to_listiopool(FS* fs, void* bufptr, long buflen, off_t offset, int signal) {
    int retcode = SUCCESS;
    /*//Algo:
      if(signal == CONTIG_FILE_BLOCK)
    //create new block is pool is empty or add to prev block if slave not set or create new block
    else if (signal == CONTINUE)
    //create new block if pool is empty and set slave or add to prev block and set slave
    else if (signal == CONTIG_AND_FINISH)
    //create new block if pool is empty and set slave or add to prev block and set slave
    else if (signal == FINISH)
    //create new block is pool is empty and set slave or add to prev block and set slave */

    contigFileBlock * iterator;

    if (signal == CONTIG_FILE_BLOCK) {
        //Is this new block?
        if(fs->listiopool.listsize == 0) {
            contigFileBlock* cfb = create_listio_fileBlock(fs);

            cfb->iov[0].iov_base = bufptr;
            cfb->iov[0].iov_len = buflen;
            cfb->offset = offset;
            cfb->size++;

            fs->listiopool.contigFileBlockList = cfb;
            fs->listiopool.listsize++;
        }
        else {
            //first locate iterator
            iterator = fs->listiopool.contigFileBlockList;
            while (iterator->next != NULL)
                iterator = iterator->next;
            //Not a new block
            if (iterator->assigned_to == -1) {
                //Add to the iov array
                iterator->iov[iterator->size].iov_base = bufptr;
                iterator->iov[iterator->size].iov_len = buflen;
                iterator->size++;
            }
            //New block
            else {
                //create_new_block
                contigFileBlock* cfb = create_listio_fileBlock(fs);
                cfb->offset = offset;
                cfb->iov[cfb->size].iov_base = bufptr;
                cfb->iov[cfb->size].iov_len = buflen;
                cfb->size++;

                iterator->next = cfb;
                fs->listiopool.listsize++;
            }
        }
    }
    else {
        if(fs->listiopool.listsize == 0) {
            contigFileBlock* cfb = create_listio_fileBlock(fs);

            cfb->iov[0].iov_base = bufptr;
            cfb->iov[0].iov_len = buflen;
            cfb->offset = offset;
            cfb->size++;

            fs->listiopool.contigFileBlockList = cfb;
            fs->listiopool.listsize++;

            cfb->assigned_to = fs->nxt_slave;
        }
        else {
            iterator = fs->listiopool.contigFileBlockList;
            while (iterator->next != NULL)
                iterator = iterator->next;

            contigFileBlock* cfb = create_listio_fileBlock(fs);
            cfb->offset = offset;
            cfb->iov[cfb->size].iov_base = bufptr;
            cfb->iov[cfb->size].iov_len = buflen;
            cfb->size++;
            cfb->assigned_to = fs->nxt_slave;

            iterator->next = cfb;
            fs->listiopool.listsize++;
        }
    }

    return retcode;
}

static contigFileBlock* create_listio_fileBlock(FS* fs) {
    contigFileBlock* cfb = (contigFileBlock*) malloc(sizeof (contigFileBlock));
    struct iovec *iov = (struct iovec*) malloc(fs->merged_args_len * sizeof (struct iovec));

    cfb->iov = iov;
    cfb->assigned_to = -1;
    cfb->size = 0;
    cfb->next = NULL;

    return cfb;
}
