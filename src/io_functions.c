#include <stdio.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#ifdef __PLFS_H_
#include <plfs.h>
#endif
#include <aio.h>
#include "defs.h"
#include<omp.h>

//extern long* num_writes;

//long** get_num_writes();

ssize_t 
_pwrite(int fd, void *bufptr, long buflen, off_t offset, FS *fs) {
    if (DEBUG) {
        fprintf(stdout, "Thread %d inside _pwrite, offset: %lu, buflen: %ld, fd: %d\n", omp_get_thread_num(), (unsigned long) offset, buflen, fd); 
        fs->debugInfo[omp_get_thread_num()].num_writes++;
    }
	
	ssize_t bytes_written = 0;
	size_t bytes_remaining = buflen;
	while(bytes_remaining > 0)
	{
		bytes_written = pwrite(fd, bufptr+bytes_written, bytes_remaining, offset);
		if(bytes_written == -1)
		{
			perror("Error during pwrite");
			return ERROR;
		}
		offset += bytes_written;
		bytes_remaining -= bytes_written;
	}
	return SUCCESS;
}

ssize_t _pread(int fd, void *bufptr, long buflen, off_t offset, FS* fs) {
	if (DEBUG) {
		fprintf(stdout, "Thread %d inside _pread, offset: %lu, buflen: %ld, fd: %d\n", omp_get_thread_num(), (unsigned long) offset, buflen, fd); 
		fs->debugInfo[omp_get_thread_num()].num_reads++;
	}
	
	ssize_t bytes_read = 0;
    ssize_t bytes_remaining = buflen;
    bytes_read = pread(fd, bufptr+bytes_read, bytes_remaining, offset);
    if(bytes_read == -1)
    {
        perror("Error during pread");
        return ERROR;
    }
    offset += bytes_read;
    bytes_remaining -= bytes_read;

    return SUCCESS;
}

ssize_t 
_plfs_write(void* _fd, void* bufptr, long buflen, off_t offset, FS *fs) {
#ifdef __PLFS_H_
    Plfs_fd *pfd = (Plfs_fd*)_fd;
    if (DEBUG) {
        fprintf(stdout, "Thread %d inside _plfs_write, offset: %lu, buflen: %ld\n", omp_get_thread_num(), (unsigned long) offset, buflen); 
        fs->debugInfo[omp_get_thread_num()].num_writes++;
    }

    ssize_t bytes_written = 0;
    size_t bytes_remaining = buflen;
    while(bytes_remaining > 0)
    {
        bytes_written = plfs_write(pfd, bufptr+bytes_written, bytes_remaining, offset, syscall(SYS_gettid));
        if(bytes_written == -1)
        {
            perror("Error during plfs write");
            return ERROR;
        }
        offset += bytes_written;
        bytes_remaining -= bytes_written;
    }
#endif
    return SUCCESS;
}

ssize_t  
_plfs_read(void* _fd, void* bufptr, long buflen, off_t offset, FS *fs) {
#ifdef __PLFS_H_
    Plfs_fd* pfd = (Plfs_fd*) _fd;
    if (DEBUG) {
        fprintf(stdout, "Thread %d inside _plfs_read, offset: %lu, buflen: %ld\n", omp_get_thread_num(), (unsigned long) offset, buflen); 
        fs->debugInfo[omp_get_thread_num()].num_writes++;
    }

    ssize_t bytes_read = 0;
    size_t bytes_remaining = buflen;
    bytes_read = plfs_read(pfd, bufptr+bytes_read, bytes_remaining, offset);
    if(bytes_read == -1)
    {
        perror("Error during plfs read");
        return ERROR;
    }
    offset += bytes_read;
    bytes_remaining -= bytes_read;
#endif
    return SUCCESS;
}

ssize_t 
_writev(int fd, struct iovec * iov, int iovcount, off_t offset, FS* fs) {
    if (DEBUG) {
        fprintf(stdout, "Thread %d inside writev. bufcount: %d, offset: %ld\n", omp_get_thread_num(), iovcount, offset);}
    fs->debugInfo[omp_get_thread_num()].num_writes += iovcount;

    //you dont need to lseek here coz u already did that in io_preface
    //need to lseek if io_preface bypassed, which is currently the case
    lseek(fd, offset, SEEK_SET);

    if (-1 == writev(fd, iov, iovcount)) {
        fprintf(stdout, "------Thread %d encountered error during writev------\n", omp_get_thread_num());
        perror("Error");
        return ERROR;
    }

    return SUCCESS; //Todo return error_code
}

ssize_t 
_readv(int fd, struct iovec * iov, int iovcount, off_t offset, FS* fs) {
    if (DEBUG) {
        fprintf(stdout, "Thread %d inside readv. bufcount: %d, offset: %ld\n", omp_get_thread_num(), iovcount, offset); }
    fs->debugInfo[omp_get_thread_num()].num_reads += iovcount;

    //you dont need to lseek here coz u already did that in io_preface
    //need to lseek if io_preface bypassed, which is currently the case
    lseek(fd, offset, SEEK_SET);

    if (-1 == readv(fd, iov, iovcount)) {
        fprintf(stdout, "------Thread %d encountered error during readv------\n", omp_get_thread_num());
        perror("Error");
        return ERROR;
    }

    return SUCCESS; //Todo return error_code
}

/*int _aio_write(struct aiocb* aiocbp) {
  if (DEBUG) {
  fprintf(stdout, "Thread %d inside _aio_write\n", omp_get_thread_num());
//fs->debugInfo[omp_get_thread_num()].num_writes++;
}

if (-1 == aio_write(aiocbp))
fprintf(stdout, "Error during aio_write\n");

return SUCCESS;
}*/

int mmap_write(char *mapaddr, off_t write_offset, iovt* vect, FS* fs) {
    int retcode = SUCCESS;
    if (DEBUG) {
        fprintf(stdout, "Thread %d inside low level mmap_write. mapaddr: %p, write_offset: %lu\n",
                omp_get_thread_num(), mapaddr, (unsigned long) write_offset);

        fs->debugInfo[omp_get_thread_num()].num_writes++;
    }

    int i;
    off_t tmpmapindex = write_offset;
    for (i = 0; i < vect->listlen; i++) {
        memcpy(&mapaddr[tmpmapindex], vect->iovlist[i].iov_base, vect->iovlist[i].iov_len);
        tmpmapindex += vect->iovlist[i].iov_len;
    }

    /*int j;
      for(i=0; i<vect->listlen; i++)
      {
      for(j=0; j<vect->iovlist[i].iov_len; j++)
      mapaddr[tmpmapindex++] = ((char *)vect->iovlist[i].iov_base)[j];
      }*/

    return retcode;
}

int mmap_read(char *mapaddr, off_t write_offset, iovt* vect, FS* fs) {
    if (DEBUG) {
        fprintf(stdout, "Thread %d inside low level mmap_write. mapaddr: %x\n", omp_get_thread_num(), *mapaddr);
        //fs->debugInfo[omp_get_thread_num()].num_writes++;
    }

    int i, tmpmapindex = write_offset;
    for (i = 0; i < vect->listlen; i++) {
        memcpy(vect->iovlist[i].iov_base, &mapaddr[tmpmapindex], vect->iovlist[i].iov_len);
        tmpmapindex += vect->iovlist[i].iov_len;
    }

    return SUCCESS;
}

/*void write_data(void *_bufptr, long buflen, long offset, int fd)
  {
  if(DEBUG) {
  puts("Inside low level io interface. Printing input parameters");
  printf("\n T%d bufptr: %x, buflen: %ld, offset: %ld, fd: %d\n", omp_get_thread_num(), _bufptr, buflen, offset, fd);	
  } 
  int check;
  char * bufptr;
  bufptr = (char *) _bufptr;
  check = pwrite(fd, _bufptr, buflen, offset);
//writes[omp_get_thread_num()]++;
if(check == -1)
{
puts("\nWrite error !\n");
printf("\nerrno: %d , fd : %d\n", errno, fd);
//print_error(errno);
}
num_writes[omp_get_thread_num()]++;
if(DEBUG) printf("\nNum_writes %ld for thread %d\n", num_writes[omp_get_thread_num()], omp_get_thread_num());
}

void read_data(void *_bufptr, long buflen, off_t offset, int fd)
{
char *bufptr;
bufptr = (char *) _bufptr;
char tempvar;
int tid = omp_get_thread_num();
int check;
check = pread(fd, bufptr, buflen, offset);
if(bufptr[0] == 'a')
tempvar = bufptr[0];
if(check == -1)
printf("\nWrite error !!\n");
}

long* get_num_writes()
{
return (num_writes);
}

void print_num_writes()
{
printf("\nNum threads %ld %ld\n", num_writes[0], num_writes[1]);
}
*/
//Implement readv and writev functions for the list I/O interface
