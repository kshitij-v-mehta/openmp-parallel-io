#include <stdio.h>
#include <stdlib.h>
#define _LARGEFILE64_SOURCE 1
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "io_functions.h"
#include "defs.h"
#include <omp.h>

/**
 * io_preface can be called from access_work_assignment directly or from stripe_handler.
 * stripe_handler passes iovec list alongwith 1 offset.
 * io preface then calls low level io interfaces
 */

typedef struct _mmapRetVals
{
    char *mapaddr;
    size_t map_len;
    off_t write_offset;
} mmapRetVals;

static int position_offset(FS *fs, iovt *vect);
//static mmapRetVals* create_mapping(FS *fs, iovt* vect);
//static int clear_mapping(mmapRetVals*);
static int create_new_mapping(FS* fs);
static int mmap_addr_exists(FS *fs, iovt *vect);
static int clear_existing_mapping(FS *fs);
static struct aiocb* create_aiocb(int fd, void *bufptr, long buflen, off_t offset, FS *fs);

/*
 iovt contains contiguous chunk(s) of data. 
 */
int io_preface(FS * fs, iovt *vect)
{
    if(DEBUG) fprintf(stdout, "Thread %d inside io preface\n", omp_get_thread_num());
    //io preface can be called from stripe_handler as well with iovec list and 1 offset
    //call to low level io goes here

    int i=0; 
    off_t tmp_offset;
    tmp_offset = vect->offset;

    //TODO: finish up on io_preface switch(lliop)
	switch(fs->ioprms->lliop)
	{
		case prw:
			for(i=0; i<vect->listlen; i++)
			{
				if(fs->ioprms->io_type == WRITE)
					_pwrite(fs->fd[omp_get_thread_num()], vect->iovlist[i].iov_base, vect->iovlist[i].iov_len, tmp_offset, fs);
				else
					_pread(fs->fd[omp_get_thread_num()], vect->iovlist[i].iov_base, vect->iovlist[i].iov_len, tmp_offset, fs);

				tmp_offset += vect->iovlist[i].iov_len;
			}
			break;

        //rwv without striping doesnt make sense.
        //since writev does not take an offset, all buf values HAVE to be contiguous
        //without striping, we just take each contiguous work assignment and write it using writev.
		case rwv:
			position_offset(fs, vect);	//TODO: v. imp. that you lock fd if everyone uses the same fd
			
			if(fs->ioprms->io_type == WRITE)
				_writev(fs->fd[omp_get_thread_num()], vect->iovlist, vect->listlen, vect->offset, fs);
			else
				_readv(fs->fd[omp_get_thread_num()], vect->iovlist, vect->listlen, vect->offset, fs);
			break;

				case _mmap:
                ;mmap_addr_exists(fs, vect);
                
                if(fs->ioprms->io_type == WRITE)
                    mmap_write(fs->mapids[omp_get_thread_num()].mapaddr, vect->offset % fs->static_mmap_len, vect, fs);
                else
                    mmap_read(fs->mapids[omp_get_thread_num()].mapaddr, vect->offset % fs->static_mmap_len, vect, fs);

                //clear_mapping(mmapretvals);
			break;
	}

	return SUCCESS; //Todo: return error_code
}

static int position_offset(FS *fs, iovt *vect)
{
	lseek(fs->fd[omp_get_thread_num()], vect->offset, SEEK_SET);
    if(DEBUG) printf("T%d lseeking to position %ld\n",omp_get_thread_num() ,vect->offset);
	return SUCCESS; //Todo: return error_code
}

static struct aiocb* create_aiocb(int fd, void *bufptr, long buflen, off_t offset, FS *fs)
{
    struct aiocb* aiocbp = (struct aiocb*) malloc (sizeof(struct aiocb));
    aiocbp->aio_fildes = fd;
    aiocbp->aio_buf = bufptr;
    aiocbp->aio_nbytes = buflen;
    aiocbp->aio_offset = offset;

    return aiocbp;
}

static int mmap_addr_exists(FS *fs, iovt *vect)
{
    //check is mapping exists. What happens when length falls between two mappings. Re-map
    if(fs->mapids[omp_get_thread_num()].map_set == 0)
        create_new_mapping(fs);
    else 
    {
        //check if offset is outside limits
        size_t length = 0;
        int i;
        
        for(i=0; i<vect->listlen; i++)
            length += vect->iovlist[i].iov_len;

        //check here is offset is inside map region
        if((vect->offset + length) > (fs->static_mmap_len + fs->mapids[omp_get_thread_num()].map_offset))
        {
            //if(omp_get_thread_num() == MASTER) 
            clear_existing_mapping(fs);
            create_new_mapping(fs);
        }
        else
            if(DEBUG)
                fprintf(stdout, "Thread %d inside io_preface:map_addr_exists.\n", omp_get_thread_num());
            //continue;
    }
        return SUCCESS;
}

static int create_new_mapping(FS *fs)
{
    off_t new_mmap_offset;
    new_mmap_offset = fs->mapids[omp_get_thread_num()].map_offset + fs->mapids[omp_get_thread_num()].map_len;
    off_t new_file_len = new_mmap_offset + fs->static_mmap_len;

    if(lseek64(fs->fd[omp_get_thread_num()], new_file_len-1, SEEK_SET) == -1)
    {
        fprintf(stderr, "Thread %d inside io_preface:create_new_mapping, lseek failed for "
                        "new_file_len %lu\n", omp_get_thread_num(), (unsigned long)new_file_len);
    }
    
    if(write(fs->fd[omp_get_thread_num()], "", 1) == -1)
    {
        fprintf(stdout, "Thread %d in io_preface:create_mapping,"
                "write to last byte of file failed for new_file_len %lu\n", 
                omp_get_thread_num(), (unsigned long) new_file_len);
        //Todo Do something here for mmap when writing a blank char to the end of file fails
    }
    
    char *mapaddr = (char *)mmap(0, fs->static_mmap_len, PROT_READ, MAP_PRIVATE, fs->fd[omp_get_thread_num()], new_mmap_offset);
    if(mapaddr == MAP_FAILED)
    {
        fprintf(stdout, "Thread %d in io_preface:create_mapping, mmap failed," 
                "errno: %d, new_mmap_offset: %lu\n", 
                omp_get_thread_num(), errno, (unsigned long)new_mmap_offset);
        //Todo: what do u do here on mmap failed?
    }

    if(DEBUG) {
        fprintf(stdout, "Thread %d in io_preface:create_new_mapping, "
                        " new_map_offset %lu, map_len %lu, new_file_len %lu\n",
                        omp_get_thread_num(), (unsigned long)new_mmap_offset, 
                        (unsigned long) fs->static_mmap_len, (unsigned long) new_file_len);
    }

    fs->mapids[omp_get_thread_num()].mapaddr     = mapaddr;
    fs->mapids[omp_get_thread_num()].map_offset  = new_mmap_offset;
    fs->mapids[omp_get_thread_num()].map_len     = fs->static_mmap_len;
    fs->mapids[omp_get_thread_num()].map_set     = 1;

    return SUCCESS;
}

static int clear_existing_mapping(FS *fs)
{
    int retcode = SUCCESS;
    if(-1 == msync(fs->mapids[omp_get_thread_num()].mapaddr, fs->static_mmap_len, MS_ASYNC))
    {
        fprintf(stdout, "Thread %d inside io_preface:clear_mapping. msync failed.", omp_get_thread_num());
        //Todo what is mmap msync fails?
        retcode = errno;
    }

    //do you munmap immediately? If not, then this mapaddr will have to be stored in a table for unmapping later.
    if(-1 == munmap(fs->mapids[omp_get_thread_num()].mapaddr, fs->static_mmap_len))
    {
        fprintf(stdout, "Thread %d inside io_preface:clear_mapping. munmap failed.", omp_get_thread_num());
        //Todo what if munmap fails?
        retcode = errno;
    }

    fs->mapids[omp_get_thread_num()].map_set = 0;
    return retcode;
}

