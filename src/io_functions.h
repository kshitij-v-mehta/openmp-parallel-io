#ifndef __IO_FUNCTIONS__
#include <fcntl.h>
#define _LARGEFILE64_SOURCE 1
#include <sys/types.h>
#ifdef __PLFS_H_
#include <plfs.h>
#endif
#include <aio.h>
#include "defs.h"

ssize_t _pwrite(int fd, void *bufptr, long buflen, off_t offset, FS *fs);
ssize_t _pread(int fd, void *bufptr, long buflen, off_t offset, FS* fs);
ssize_t _plfs_write(void* pfd, void *bufptr, long buflen, off_t offset, FS *fs);
ssize_t _plfs_read(void* pfd, void *bufptr, long buflen, off_t offset, FS* fs);
ssize_t _writev(int fd, struct iovec * iov, int iovcount, off_t offset, FS*);
ssize_t _readv(int fd, struct iovec * iov, int iovcount, off_t offset, FS*);
int _aio_write(struct aiocb*);
int mmap_write(char *mapaddr, off_t write_offset, iovt *vect, FS* fs);
int mmap_read(char *mapaddr, off_t write_offset, iovt *vect, FS* fs);
#endif
