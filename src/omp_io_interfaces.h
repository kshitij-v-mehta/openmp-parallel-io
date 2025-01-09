#ifndef __OMP_IO_INTERFACES_
#define __OMP_IO_INTERFACES_

void ompc_barrier_init(int num_threads);

ssize_t omp_file_read_all (void* bufptr, long length, int fd, int hint);
ssize_t omp_file_read_at_all (void* bufptr, long length, off_t offset, int fd, int hint);
ssize_t omp_file_write_all (void* bufptr, long length, int fd, int hint);
ssize_t omp_file_write_at_all (void* bufptr, long length, off_t offset,  int fd, int hint);
ssize_t omp_file_open_all (int *fd, char * filename, int oflag, ...);
ssize_t omp_file_close_all (int fd);
ssize_t omp_file_fsync(int fd);

void Omp_File_read_all (void *buffer, long* length, int* fd, int* hint);
void Omp_File_read_at_all (void *buffer, long* length, long* _offset, int* fd, int* hint);
void Omp_File_write_all (void *buffer, long* length, int* fd, int* hint);
void Omp_File_write_at_all (void *buffer, long* length, long* _offset, int* fd, int* hint);
//-------------------------------LIST INTERFACES----------------------------------//

ssize_t omp_file_read_list_all (void* buflist, int size,  int fd, int hint);
ssize_t omp_file_read_list_at_all (void* buflist, off_t* offsets, int size, int fd, int hint);
ssize_t omp_file_write_list_all (void* buflist, int size, int fd, int hint);
ssize_t omp_file_write_list_at_all (void* buflist, off_t* offsets, int size, int fd, int hint);
void Omp_File_read_list_all (void *buffer, int* size, int* fd, int* hint);
void Omp_File_read_list_at_all (void* buffer, long* offsets, int* listsize, int* fd, int* hint);
void Omp_File_write_list_all (void* buffer, int *listsize, int* fd, int* hint);
void Omp_File_write_list_at_all (void* buffer, long* offsets, int* size, int* fd, int* hint);

//-----------------------COMMON ARGS INTERFACES-----------------------------------------------------------//
ssize_t omp_file_read_com_all (void *bufptr, long length, int fd, int hint);
ssize_t omp_file_read_com_at_all (void* bufptr, long length, off_t offset, int fd, int hint);
ssize_t omp_file_write_com_all (void* bufptr, long length, int fd, int hint);
ssize_t omp_file_write_com_at_all (void* bufptr, long length, off_t offset, int fd, int hint);
ssize_t omp_file_read_com_list_all (void* buflist, int size, int fd, int hint);
ssize_t omp_file_read_com_list_at_all (void* buflist, off_t* offsets, int size, int fd, int hint);
ssize_t omp_file_write_com_list_all (void* buflist, int size, int fd, int hint);
ssize_t omp_file_write_com_list_at_all (void* buflist, off_t* offsets, int size, int fd, int hint);
void Omp_File_read_com_all (void* buffer, long* length, int* fd, int* hint);
void Omp_File_read_com_at_all (void * buffer, long* length, long* offset, int* fd, int* hint);
void Omp_File_write_com_all (void* buffer, long* length, int* fd, int* hint);
void Omp_File_write_com_at_all (void *buffer, long* length, long* offset, int* fd, int* hint);
void Omp_File_read_com_list_all (void *buffer, int* size, int* fd, int* hint);
void Omp_File_read_com_list_at_all (void *buffer, long* offsets, int* size, int* fd, int* hint);
void Omp_File_write_com_list_all (void *buffer, int* size, int* fd, int* hint);
void Omp_File_write_com_list_at_all (void* buffer, long* offsets, int* size, int* fd, int* hint);

#endif
