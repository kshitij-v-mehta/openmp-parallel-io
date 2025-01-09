/*
Communication between C and Fortran program segments:

1. Strings:
Fortran strings are not terminated by the end-of-line \0 character like C strings. Secondly, fortran strings are declared to be of constant size, and the non-used trailing spaces are filled with blanks ' '. Note that you cannot have dynamically created strings in fortran like char* a = "abc".

2. Opening files:
One way is to open a file in the fortan code and convert the int associated with the file to a valid fd using FNUM(thatint). 
Second way is to open the file in the C code and store the fd somewhere, or return the fd to the fortran code in a pointer. 
The second way is more appropriate since opening a file in your library actually creates a FS* fs, which cannot be returned to the fortran code. All fds opened during this time are stored in fs, which has to be stored somewhere. 

Now, there might be many FSes open in the library at the same time for different sets of threads. When each set opens a file, all its threads get a common int f, which is actually the array index in the global array where their FS is stored. So the library maintains a global array of a struct {FS, ..}. Hence, in a collective call, the library first has to fetch the fs associated with the calling set of threads by doing a lookup of their f into the global array.  
 */

//TODO: The library does not make use of the 'hint' paramter. Needs to be included in the library code once approved.

#include "defs.h"
#include "fs_table.h"
#include "pthread_coll_io_base.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/syscall.h>
#ifdef __PLFS_H_
#include <plfs.h>
#endif
#include "ompc_barrier.h"
#include <omp.h>

int DEBUG = 0;
#ifdef __PLFS_H_
volatile Plfs_fd* temp_plfs_fd_table[65536]; // = {NULL};
#endif
volatile int temp_fds[128] = {0}; //TODO: temporary. to be set dynamically.
int sync_var = -1; //used for synchronization. This function on moving to the compiler should use a barrier variable.

static int fortran_string_f2c(char *fstr, int *flen, char **cstr) {
    char *end;
    int i;
    int len = *flen;

    /* Leading and trailing blanks are discarded. */

    end = fstr + len - 1;

    for (i = 0; (i < len) && (' ' == *fstr); ++i, ++fstr) {
        continue;
    }

    if (i >= len) {
        len = 0;
    } else {
        for (; (end > fstr) && (' ' == *end); --end) {
            continue;
        }

        len = end - fstr + 1;
    }

    /* Allocate space for the C string. */

    if (NULL == (*cstr = malloc(len + 1))) {
        return -1;
    }

    /* Copy F77 string into C string and NULL terminate it. */

    if (len > 0) {
        strncpy(*cstr, fstr, len);
    }
    (*cstr)[len] = '\0';

    *flen = len;
    return 0;
}

int omp_file_fsync(int fd) {    
    if(omp_get_thread_num() == MASTER) {
        FS* fs = get_fs_from_table(fd);
        int i;

        for (i=0; i<omp_get_num_threads(); i++) {
            if(fs->plfs) {
#ifdef __PLFS_H_
                if(0!=plfs_sync(fs->pfd[i]))
                {
                    perror("Error on plfs_sync");
                    errno = 0;
                }
#endif
            }else{
                fsync(fs->fd[i]);
            }
        }
    }

    return SUCCESS;
}

static int read_fs_config(FS* fs) {
    int MAXLINE = 100;
    int MAXKEYLEN = 100;
    int i, flag;//, flag2;
    char keyword[MAXKEYLEN];
    char temp[MAXKEYLEN];
    char buffer[MAXLINE];
    char *ptr;
    FILE *Conf_File;
    memset(buffer, 0, MAXLINE);
    memset(keyword, 0, MAXKEYLEN);
    ptr = NULL;
    flag = 0;
    //flag2 = 0;
    memset(buffer, 0, MAXLINE);
    memset(keyword, 0, MAXKEYLEN);

    if ((Conf_File = fopen("fs.config", "r")) == NULL) {

        fprintf(stdout, "%d @ %s, open file error !\n", __LINE__, __FILE__);
        return -1;
    }

    while (fgets(buffer, MAXLINE, Conf_File) != NULL) {
        /* check blank line or # comment */
        if (buffer[0] != '#') {

            /* Parse one single line! */
            i = 0;
            flag = 0;
            while (i < strlen(buffer)) {
                temp[i] = buffer[i];
                if (buffer[i] == ':') {
                    strncpy(keyword, temp, i);
                    i++;
                    flag = 1; /* this means this line has keywords; */
                    ptr = &buffer[i];
                    break;
                }
                i++;
            }
    
            int active_threads, plfs_switch;
            off_t max_write_block_size;
            /*  If this is a keyworkd...... */
            if (flag == 1) {
                if (strncmp(keyword, "active_threads", strlen("active_threads")) == 0) {
                    sscanf(ptr, "%d", &active_threads);
                    fs->active_threads = active_threads;
                    ptr = NULL;
                }

                if (strncmp(keyword, "max_write_block_size", strlen("max_write_block_size")) == 0) {
                    sscanf(ptr, "%ld", &max_write_block_size);
                    fs->max_write_block_size = max_write_block_size;
                    ptr = NULL;
                }
                
                if (strncmp(keyword, "plfs", strlen("plfs")) == 0) {
                    sscanf(ptr, "%d", &plfs_switch);
                    fs->plfs = plfs_switch;
                    ptr = NULL;
                }
            }
        }
    }
    //puts(filename);
    fclose(Conf_File);
    return SUCCESS;
}

static int __file_open(char*filename, int oflag, FS* fs) {
    int my_fd;

    // If not master, and O_TRUNC is specified, then remove it. Only master should do it 
    if(omp_get_thread_num() != MASTER) {
        if(oflag & O_TRUNC) {
            oflag = (oflag) ^ O_TRUNC ;
        }
    }

    fs->open_flags = oflag;
    if(fs->plfs) {
#ifdef __PLFS_H_
        Plfs_fd *my_pfd = NULL;
        pthread_mutex_lock(&(fs->sync_objs[MASTER].mutex));
        if(0 > plfs_open(&my_pfd, filename, oflag, syscall(SYS_gettid), 0644, NULL))
        {
            perror("Error with plfs file open");
            errno = 0;
            return -1;
        }
        pthread_mutex_unlock(&(fs->sync_objs[MASTER].mutex));

        // Get a valuable stat attribute like inode_no to assign to my_fd
        struct stat filestat;
        if(0 > plfs_getattr(my_pfd, filename, &filestat, 0)) {
            perror("Could not stat plfs file");
            exit(-1);
        }
        my_fd = filestat.st_ino;

        fs->pfd[omp_get_thread_num()] = my_pfd;
#endif
    }
    else
    {
        if (-1 == (my_fd = open(filename, oflag, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH))) 
        {
            perror("Error opening file");
            //fs->status = ERROR;    
            return -1;
        }
    }
    fs->fd[omp_get_thread_num()] = my_fd; 

    return  my_fd;
}

//int pthread_coll_file_open(FS *fs, char * filename, enum lowlevelioprimitives ioprim)
int omp_file_open_all(int *fd, char * filename, int oflag, ...) {
    int my_fd, i;
    FS * fs = NULL;
    if (omp_get_thread_num() == MASTER) {
        fs = (FS*) malloc(sizeof (FS));
        insert_into_fs_table(fs);

        // Read fs_config 
        read_fs_config(fs);
        if(fs->active_threads > omp_get_num_threads())
            fs->active_threads = omp_get_num_threads();

        fs->targs = (thread_args*) malloc(omp_get_num_threads() * sizeof (thread_args));
        fs->tlist_args = (thread_list_args*) malloc(omp_get_num_threads() * sizeof (thread_list_args));
        fs->fd = (int *) malloc(omp_get_num_threads() * sizeof (int));
#ifdef __PLFS_H_
        if(fs->plfs)
            fs->pfd = (Plfs_fd **) malloc (omp_get_num_threads() * sizeof(Plfs_fd *));
#endif
        fs->sync_objs = (sync_obj *) malloc(omp_get_num_threads() * sizeof (sync_obj));
        fs->ioassignments = (workassignment *) malloc(omp_get_num_threads() * sizeof (workassignment));
        fs->ioprms = (ioparams *) malloc(sizeof (ioparams));
        fs->ioprms->filename = filename;
        fs->mapids = (mmapinfo *) malloc(omp_get_num_threads() * sizeof (mmapinfo));
        fs->static_mmap_len = 128 * 1048576;

        fs->listiopool.contigFileBlockList = NULL;
        fs->listiopool.listsize = 0;
        fs->tlist_args->iov = NULL;
        fs->tlist_args->offset_list = NULL;
        //fs->merged_targs = NULL;

        pthread_barrier_init(&(fs->barr), NULL, fs->active_threads);
        fs->debugInfo = (_debugInfo *) malloc(omp_get_num_threads() * sizeof (_debugInfo));

        for (i = 0; i < omp_get_num_threads(); i++) {
            fs->targs[i].offset = -1;
            fs->tlist_args[i].list_size = 0;
            fs->ioassignments[i].offset = -1;
            fs->ioassignments[i].iovlisthead = NULL;
            fs->ioassignments[i].listlen = 0;
            fs->ioassignments[i].completed = 0;
            fs->ioassignments[i].status = SLEEP;
            pthread_mutex_init(&(fs->sync_objs[i].mutex), NULL);
            pthread_cond_init(&(fs->sync_objs[i].cond_var), NULL);

            fs->mapids[i].map_set = 0;
            fs->mapids[i].map_offset = 0;
            fs->mapids[i].map_len = 0;

            fs->debugInfo[i].num_writes = 0;
            fs->debugInfo[i].num_reads = 0;
            fs->debugInfo[i].iteration = 0;

            if(fs->plfs)
            {
#ifdef __PLFS_H_
                fs->pfd[omp_get_thread_num()] = NULL;
#endif
            }
            else
            {
                fs->fd[omp_get_thread_num()] = -1;
            }
        }

        fs->sync_flag = 2;
        fs->nxt_slave = 0;
        if (omp_get_num_threads() > 1)
        {
            if(fs->active_threads>1)
                fs->nxt_slave = 1;
        }

        //These could be set from someplace else
        fs->fc.str_size = 1048576;
        fs->fc.str_depth = 22;

        fs->internal_offset = -1;
        fs->all_quit_flag = 0;
        //fs->ioprms->lliop = prw; //TODO: Read this from somewhere
        
        my_fd = __file_open(filename, oflag, fs);
        *fd = my_fd;
    } // if(omp_get_thread_num() == MASTER)

    __ompc_barrier();

    if(omp_get_thread_num() != MASTER) {
        fs = get_fs_from_table(*fd);
        my_fd = __file_open(filename, oflag, fs);
    }

    if(!fs->plfs)
    {
        //After opening the file, set the permissions correctly
        mode_t mode = 0;
        if (oflag & O_CREAT) {
            va_list arg;
            va_start(arg, oflag);
            mode = va_arg(arg, int);
            va_end(arg);
            //fchmod(my_fd,mode);
        }
    }
    __ompc_barrier();
    if (DEBUG) fprintf(stdout, "Thread %d inside file open, filename: %s, fd: %d\n", omp_get_thread_num(), filename, my_fd);
    return SUCCESS;
}

void Omp_File_open_all(int* fd, char* ffilename, int* mode) {
    if (DEBUG) printf("T%d inside Omp_File_open_all. filename: %s\n", omp_get_thread_num(), ffilename);
    char* cfilename = "";
    int fstrlen = strlen(ffilename);
    fortran_string_f2c(ffilename, &fstrlen, &cfilename);
    if (DEBUG) printf("T%d converted fstring to cstring. fstr: %s, cstr: %s\n", omp_get_thread_num(), ffilename, cfilename);
    omp_file_open_all(fd, cfilename, *mode);
}
#pragma weak omp_file_open_all_ = Omp_File_open_all
#pragma weak omp_file_open_all__ = Omp_File_open_all
#pragma weak OMP_FILE_OPEN_ALL = Omp_File_open_all

void omp_file_close_all(int fd) {
    int i;
    __ompc_barrier();
    FS* fs = get_fs_from_table(fd);
    if(fs->plfs) 
    {
#ifdef __PLFS_H_
        if(0 != (plfs_close(fs->pfd[omp_get_thread_num()], syscall(SYS_gettid), 0, fs->open_flags, NULL)))
        {
            perror("Error during plfs_close");
            errno = 0;
        }
#endif
    }
    else
    {
        close(fs->fd[omp_get_thread_num()]);    //Should be before the barrier
    }
    __ompc_barrier();

    if(omp_get_thread_num() == MASTER)
    {
        remove_fs_from_table(fd);           //Very imp. that we get fs first, then remove
        free(fs->fd);
#ifdef __PLFS_H_
        if(fs->plfs)
            free(fs->pfd);
#endif
        free(fs->ioprms);
        free(fs->targs);
        /*if(fs->tlist_args)
        {
            if(fs->tlist_args->iov)
                free(fs->tlist_args->iov);
            if(fs->tlist_args->offset_list)
                free(fs->tlist_args->offset_list);
        }*/
        free(fs->tlist_args);
        free(fs->ioassignments);
        //if(fs->merged_targs)
            //free(fs->merged_targs);
        free(fs->mapids);
        free(fs->debugInfo);

        for(i=0; i<omp_get_num_threads(); i++)
        {
            pthread_mutex_destroy(&(fs->sync_objs[i].mutex));
            pthread_cond_destroy(&(fs->sync_objs[i].cond_var));
        }

        free(fs->sync_objs);
        free(fs);
    }
}

void Omp_File_close_all(int *fd) {
    omp_file_close_all(*fd);
}
#pragma weak omp_file_close_all_ = Omp_File_close_all
#pragma weak omp_file_close_all__ = Omp_File_close_all
#pragma weak OMP_FILE_CLOSE_ALL = Omp_File_close_all

//Returns error code

ssize_t
omp_file_read_all(void* bufptr, size_t length, int fd, int hint) {
    int retcode = SUCCESS;
	ssize_t io_bytes;
    FS* fs = get_fs_from_table(fd); //TODO: No error checking here if fs==NULL ?
    if (omp_get_thread_num() == MASTER) {
        fs->ioprms->io_type = READ;
        fs->ioprms->input_type = BUF;
        fs->ioprms->merge_type = MEM;
        fs->ioprms->contig = NO;
        fs->ioprms->same_prms = NO;

        fs->ioprms->hint = hint;

        fs->ioprms->MEMSTR = NO;
        fs->ioprms->ALLSTR = YES;
    }

    fs->targs[omp_get_thread_num()].bufptr = (char*) bufptr;
    fs->targs[omp_get_thread_num()].buflen = length;

    io_bytes = pthread_coll_io_base(fs);
    return io_bytes;
}

void Omp_File_read_all(void *buffer, long* length, int* fd, int* hint) {
    omp_file_read_all(buffer, *length, *fd, *hint);
}
#pragma weak omp_file_read_all_ = Omp_File_read_all
#pragma weak omp_file_read_all__ = Omp_File_read_all
#pragma weak OMP_FILE_READ_ALL = Omp_File_read_all

ssize_t
omp_file_read_at_all(void* bufptr, size_t length, off_t offset, int fd, int hint) {
    int retcode = SUCCESS;
	ssize_t io_bytes;
    FS* fs = get_fs_from_table(fd);
    if (omp_get_thread_num() == MASTER) {
        fs->ioprms->io_type = READ;
        fs->ioprms->input_type = BUF;
        fs->ioprms->merge_type = MEM;
        fs->ioprms->contig = NO;
        fs->ioprms->same_prms = NO;

        fs->ioprms->hint = hint;

        fs->ioprms->MEMSTR = NO;
        fs->ioprms->ALLSTR = YES;
    }

    fs->targs[omp_get_thread_num()].bufptr = (char*) bufptr;
    fs->targs[omp_get_thread_num()].buflen = length;
    fs->targs[omp_get_thread_num()].offset = offset;

    io_bytes = pthread_coll_io_base(fs);
    return io_bytes;
}

void Omp_File_read_at_all(void *buffer, long* length, long* _offset, int* fd, int* hint) {
    off_t offset = (off_t) (*_offset);
    omp_file_read_at_all(buffer, *length, offset, *fd, *hint); //TODO: offset typecasted to off_t. Do this everywhere.
}
#pragma weak omp_file_read_at_all_ = Omp_File_read_at_all
#pragma weak omp_file_read_at_all__ = Omp_File_read_at_all
#pragma weak OMP_FILE_READ_AT_ALL = Omp_File_read_at_all

ssize_t
omp_file_write_all(void* bufptr, size_t length, int fd, int hint) {
    int retcode = SUCCESS;
	ssize_t io_bytes;
    FS* fs = get_fs_from_table(fd);
    if (omp_get_thread_num() == MASTER) {
        fs->ioprms->io_type = WRITE;
        fs->ioprms->input_type = BUF;
        fs->ioprms->merge_type = MEM;
        fs->ioprms->contig = NO;
        fs->ioprms->same_prms = NO;

        fs->ioprms->MEMSTR = NO;
        fs->ioprms->ALLSTR = YES;

        fs->ioprms->hint = hint;

        //fs->ioprms->lliop = arw; 
    }

    fs->targs[omp_get_thread_num()].bufptr = (char*) bufptr;
    fs->targs[omp_get_thread_num()].buflen = length;

    io_bytes = pthread_coll_io_base(fs);
    return io_bytes;
}

void Omp_File_write_all(void *buffer, long* length, int* fd, int* hint) {
    if (DEBUG) printf("T%d in Omp_File_write_all, fd:%d, length:%ld\n", omp_get_thread_num(), *fd, *length);
    omp_file_write_all(buffer, *length, *fd, *hint);
}
#pragma weak omp_file_write_all_ = Omp_File_write_all
#pragma weak omp_file_write_all__ = Omp_File_write_all
#pragma weak OMP_FILE_WRITE_ALL = Omp_File_write_all

ssize_t
omp_file_write_at_all(void* bufptr, size_t length, off_t offset, int fd, int hint) {
    int retcode = SUCCESS;
	ssize_t io_bytes;
    FS* fs = get_fs_from_table(fd);
    if (omp_get_thread_num() == MASTER) {
        fs->ioprms->io_type = WRITE;
        fs->ioprms->input_type = BUF;
        fs->ioprms->merge_type = ALL;
        fs->ioprms->contig = NO;
        fs->ioprms->same_prms = NO;

        fs->ioprms->hint = hint;

        fs->ioprms->MEMSTR = NO;
        fs->ioprms->ALLSTR = NO;
    }

    fs->targs[omp_get_thread_num()].bufptr = (char*) bufptr;
    fs->targs[omp_get_thread_num()].buflen = length;
    fs->targs[omp_get_thread_num()].offset = offset;

    io_bytes = pthread_coll_io_base(fs);
    return io_bytes;
}

void Omp_File_write_at_all(void *buffer, long* length, long* _offset, int* fd, int* hint) {
    omp_file_write_at_all(buffer, *length, (off_t) * _offset, *fd, *hint);
}
#pragma weak omp_file_write_at_all_ = Omp_File_write_at_all
#pragma weak omp_file_write_at_all__ = Omp_File_write_at_all
#pragma weak OMP_FILE_WRITE_AT_ALL = Omp_File_write_at_all

//-------------------------------LIST INTERFACES----------------------------------//

ssize_t
omp_file_read_list_all(struct iovec* buflist, int size, int fd, int hint) {
    int retcode = SUCCESS;
	ssize_t io_bytes;
    FS *fs = get_fs_from_table(fd);
    if (omp_get_thread_num() == MASTER) {
        fs->ioprms->io_type = READ;
        fs->ioprms->input_type = LIST;
        fs->ioprms->merge_type = MEM;
        fs->ioprms->contig = NO;
        fs->ioprms->same_prms = NO;

        fs->ioprms->hint = hint;

        fs->ioprms->MEMSTR = NO;
        fs->ioprms->ALLSTR = NO;
    }

    fs->tlist_args[omp_get_thread_num()].iov = buflist;
    fs->tlist_args[omp_get_thread_num()].list_size = size;
    io_bytes = pthread_coll_io_base(fs);

    return io_bytes;
}

//TODO: Fortran cannot pass 'struct iovec *'. How do you handle this? Ask EG.

void Omp_File_read_list_all(struct iovec *buffer, int* size, int* fd, int* hint) {
    omp_file_read_list_all(buffer, *size, *fd, *hint);
}
#pragma weak omp_file_read_list_all_ = Omp_File_read_list_all
#pragma weak omp_file_read_list_all__ = Omp_File_read_list_all
#pragma weak OMP_FILE_READ_LIST_ALL = Omp_File_read_list_all

ssize_t
omp_file_read_list_at_all(void* buflist, off_t* offsets, int size, int fd, int hint) {
    int retcode = SUCCESS;
	ssize_t io_bytes;
    FS *fs = get_fs_from_table(fd);
    if (omp_get_thread_num() == MASTER) {
        fs->ioprms->io_type = READ;
        fs->ioprms->input_type = LIST;
        fs->ioprms->merge_type = ALL;
        fs->ioprms->contig = NO;
        fs->ioprms->same_prms = NO;

        fs->ioprms->hint = hint;

        fs->ioprms->MEMSTR = NO;
        fs->ioprms->ALLSTR = YES;
    }

    fs->tlist_args[omp_get_thread_num()].iov = buflist;
    fs->tlist_args[omp_get_thread_num()].offset_list = offsets;
    fs->tlist_args[omp_get_thread_num()].list_size = size;
    io_bytes = pthread_coll_io_base(fs);

    return io_bytes;
}

void Omp_File_read_list_at_all(struct iovec* buffer, long* offsets, int* listsize, int* fd, int* hint) {
    omp_file_read_list_at_all(buffer, (off_t *) offsets, *listsize, *fd, *hint);
}
#pragma weak omp_file_read_list_at_all_ = Omp_File_read_list_at_all
#pragma weak omp_file_read_list_at_all__ = Omp_File_read_list_at_all
#pragma weak OMP_FILE_READ_LIST_AT_ALL = Omp_File_read_list_at_all

ssize_t
omp_file_write_list_all(void* buflist, int size, int fd, int hint) {
    int retcode = SUCCESS;
	ssize_t io_bytes;
    FS *fs = get_fs_from_table(fd);
    if (omp_get_thread_num() == MASTER) {
        fs->ioprms->io_type = WRITE;
        fs->ioprms->input_type = LIST;
        fs->ioprms->merge_type = MEM;
        fs->ioprms->contig = NO;
        fs->ioprms->same_prms = NO;

        fs->ioprms->hint = hint;

        fs->ioprms->MEMSTR = NO;
        fs->ioprms->ALLSTR = NO;

        fs->ioprms->lliop = rwv; //TODO: Set lliop in all interfaces
    }

    fs->tlist_args[omp_get_thread_num()].iov = buflist;
    fs->tlist_args[omp_get_thread_num()].list_size = size;
    io_bytes = pthread_coll_io_base(fs);

    return io_bytes;
}

void Omp_File_write_list_all(struct iovec* buffer, int *listsize, int* fd, int* hint) {
    omp_file_write_list_all(buffer, *listsize, *fd, *hint);
}
#pragma weak omp_file_write_list_all_ = Omp_File_write_list_all
#pragma weak omp_file_write_list_all__ = Omp_File_write_list_all
#pragma weak OMP_FILE_WRITE_LIST_ALL = Omp_File_write_list_all

ssize_t
omp_file_write_list_at_all(void* buflist, off_t* offsets, int size, int fd, int hint) {
    int retcode = SUCCESS;
	ssize_t io_bytes;
    FS *fs = get_fs_from_table(fd);
    if (omp_get_thread_num() == MASTER) {
        fs->ioprms->io_type = WRITE;
        fs->ioprms->input_type = LIST;
        fs->ioprms->merge_type = ALL;
        fs->ioprms->contig = NO;
        fs->ioprms->same_prms = NO;

        fs->ioprms->hint = hint;

        fs->ioprms->MEMSTR = NO;
        fs->ioprms->ALLSTR = YES;
    }

    fs->ioprms->lliop = rwv; //TODO: Set lliop in all interfaces
    fs->tlist_args[omp_get_thread_num()].iov = buflist;
    fs->tlist_args[omp_get_thread_num()].offset_list = offsets;
    fs->tlist_args[omp_get_thread_num()].list_size = size;
    io_bytes = pthread_coll_io_base(fs);

    return io_bytes;
}

void Omp_File_write_list_at_all(struct iovec* buffer, long* offsets, int* size, int* fd, int* hint) {
    omp_file_write_list_at_all(buffer, (off_t *) offsets, *size, *fd, *hint);
}
#pragma weak omp_file_write_list_at_all_ = Omp_File_write_list_at_all
#pragma weak omp_file_write_list_at_all__ = Omp_File_write_list_at_all
#pragma weak OMP_FILE_WRITE_LIST_AT_ALL = Omp_File_write_list_at_all



//-----------------------COMMON ARGS INTERFACES-----------------------------------------------------------//

ssize_t
omp_file_read_com_all(void *bufptr, size_t length, int fd, int hint) {
    int retcode = SUCCESS;
	ssize_t io_bytes;
    FS* fs = get_fs_from_table(fd); //TODO: No error checking here if fs==NULL ?
    if (omp_get_thread_num() == MASTER) {
        fs->ioprms->io_type = READ;
        fs->ioprms->input_type = BUF;
        fs->ioprms->merge_type = MEM;
        fs->ioprms->contig = YES;
        fs->ioprms->same_prms = YES;

        fs->ioprms->hint = hint;

        fs->ioprms->MEMSTR = NO;
        fs->ioprms->ALLSTR = YES;
    }

    fs->targs[omp_get_thread_num()].bufptr = (char*) bufptr;
    fs->targs[omp_get_thread_num()].buflen = length;

    io_bytes = pthread_coll_io_base(fs);
    return io_bytes;
}

void Omp_File_read_com_all(void* buffer, long* length, int* fd, int* hint) {
    omp_file_read_com_all(buffer, *length, *fd, *hint);
}
#pragma weak omp_file_read_com_all__ = Omp_File_read_com_all
#pragma weak omp_file_read_com_all_  = Omp_File_read_com_all
#pragma weak OMP_FILE_READ_COM_ALL   = Omp_File_read_com_all

ssize_t
omp_file_read_com_at_all(void* bufptr, size_t length, off_t offset, int fd, int hint) {
    int retcode = SUCCESS;
	ssize_t io_bytes;
    FS* fs = get_fs_from_table(fd); //TODO: No error checking here if fs==NULL ?
    if (omp_get_thread_num() == MASTER) {
        fs->ioprms->io_type = READ;
        fs->ioprms->input_type = BUF;
        fs->ioprms->merge_type = ALL;
        fs->ioprms->contig = YES;
        fs->ioprms->same_prms = YES;

        fs->ioprms->hint = hint;

        fs->ioprms->MEMSTR = NO;
        fs->ioprms->ALLSTR = YES;
    }

    fs->targs[omp_get_thread_num()].bufptr = (char*) bufptr;
    fs->targs[omp_get_thread_num()].buflen = length;
    fs->targs[omp_get_thread_num()].offset = offset;

    io_bytes = pthread_coll_io_base(fs);
    return io_bytes;
}

void Omp_File_read_com_at_all(void * buffer, long* length, long* offset, int* fd, int* hint) {
    omp_file_read_com_at_all(buffer, *length, (off_t) * offset, *fd, *hint);
}
#pragma weak omp_file_read_com_at_all__ = Omp_File_read_com_at_all
#pragma weak omp_file_read_com_at_all_  = Omp_File_read_com_at_all
#pragma weak OMP_FILE_READ_COM_AT_ALL   = Omp_File_read_com_at_all

ssize_t
omp_file_write_com_all(void* bufptr, size_t length, int fd, int hint) {
    int retcode = SUCCESS;
	ssize_t io_bytes;
    FS* fs = get_fs_from_table(fd); //TODO: No error checking here if fs==NULL ?
    if (omp_get_thread_num() == MASTER) {
        fs->ioprms->io_type = WRITE;
        fs->ioprms->input_type = BUF;
        fs->ioprms->merge_type = MEM;
        fs->ioprms->contig = YES;
        fs->ioprms->same_prms = YES;

        fs->ioprms->hint = hint;

        fs->ioprms->MEMSTR = NO;
        fs->ioprms->ALLSTR = YES;
    }

    fs->targs[omp_get_thread_num()].bufptr = (char*) bufptr;
    fs->targs[omp_get_thread_num()].buflen = length;

    io_bytes = pthread_coll_io_base(fs);
    return io_bytes;
}

void Omp_File_write_com_all(void* buffer, long* length, int* fd, int* hint) {
    omp_file_write_com_all(buffer, *length, *fd, *hint);
}
#pragma weak omp_file_write_com_all__ = Omp_File_write_com_all
#pragma weak omp_file_write_com_all_  = Omp_File_write_com_all
#pragma weak OMP_FILE_WRITE_COM_ALL   = Omp_File_write_com_all

ssize_t
omp_file_write_com_at_all(void* bufptr, size_t length, off_t offset, int fd, int hint) {
    int retcode = SUCCESS;
	ssize_t io_bytes;
    FS* fs = get_fs_from_table(fd); //TODO: No error checking here if fs==NULL ?
    if (omp_get_thread_num() == MASTER) {
        fs->ioprms->io_type = WRITE;
        fs->ioprms->input_type = BUF;
        fs->ioprms->merge_type = ALL;
        fs->ioprms->contig = YES;
        fs->ioprms->same_prms = YES;

        fs->ioprms->hint = hint;

        fs->ioprms->MEMSTR = NO;
        fs->ioprms->ALLSTR = YES;
    }

    fs->targs[omp_get_thread_num()].bufptr = (char*) bufptr;
    fs->targs[omp_get_thread_num()].buflen = length;
    fs->targs[omp_get_thread_num()].offset = offset;

    io_bytes = pthread_coll_io_base(fs);
    return io_bytes;
}

void Omp_File_write_com_at_all(void *buffer, long* length, long* offset, int* fd, int* hint) {
    omp_file_write_com_at_all(buffer, *length, (off_t) * offset, *fd, *hint);
}
#pragma weak omp_file_write_com_at_all__ = Omp_File_write_com_at_all
#pragma weak omp_file_write_com_at_all_  = Omp_File_write_com_at_all
#pragma weak OMP_FILE_WRITE_COM_AT_ALL   = Omp_File_write_com_at_all

ssize_t
omp_file_read_com_list_all(void* buflist, int size, int fd, int hint) {
    int retcode = SUCCESS;
	ssize_t io_bytes;
    FS *fs = get_fs_from_table(fd);
    if (omp_get_thread_num() == MASTER) {
        fs->ioprms->io_type = READ;
        fs->ioprms->input_type = LIST;
        fs->ioprms->merge_type = MEM;
        fs->ioprms->contig = NO;
        fs->ioprms->same_prms = YES;

        fs->ioprms->hint = hint;

        fs->ioprms->MEMSTR = NO;
        fs->ioprms->ALLSTR = YES;
    }

    fs->tlist_args[omp_get_thread_num()].iov = buflist;
    fs->tlist_args[omp_get_thread_num()].list_size = size;
    io_bytes = pthread_coll_io_base(fs);

    return io_bytes;
}

void Omp_File_read_com_list_all(void *buffer, int* size, int* fd, int* hint) {
    omp_file_read_com_list_all(buffer, *size, *fd, *hint);
}
#pragma weak omp_file_read_com_list_all__ = Omp_File_read_com_list_all
#pragma weak omp_file_read_com_list_all_  = Omp_File_read_com_list_all
#pragma weak OMP_FILE_READ_COM_LIST_ALL   = Omp_File_read_com_list_all

ssize_t
omp_file_read_com_list_at_all(void* buflist, off_t* offsets, int size, int fd, int hint) {
    int retcode = SUCCESS;
	ssize_t io_bytes;
    FS *fs = get_fs_from_table(fd);
    if (omp_get_thread_num() == MASTER) {
        fs->ioprms->io_type = READ;
        fs->ioprms->input_type = LIST;
        fs->ioprms->merge_type = ALL;
        fs->ioprms->contig = NO;
        fs->ioprms->same_prms = YES;

        fs->ioprms->hint = hint;

        fs->ioprms->MEMSTR = NO;
        fs->ioprms->ALLSTR = YES;
    }

    fs->tlist_args[omp_get_thread_num()].iov = buflist;
    fs->tlist_args[omp_get_thread_num()].offset_list = offsets;
    fs->tlist_args[omp_get_thread_num()].list_size = size;
    io_bytes = pthread_coll_io_base(fs);

    return io_bytes;
}

void Omp_File_read_com_list_at_all(void *buffer, long* offsets, int* size, int* fd, int* hint) {
    omp_file_read_com_list_at_all(buffer, (off_t*) offsets, *size, *fd, *hint);
}
#pragma weak omp_file_read_com_list_at_all__ = Omp_File_read_com_list_at_all
#pragma weak omp_file_read_com_list_at_all_  = Omp_File_read_com_list_at_all
#pragma weak OMP_FILE_READ_COM_LIST_AT_ALL   = Omp_File_read_com_list_at_all

ssize_t
omp_file_write_com_list_all(void* buflist, int size, int fd, int hint) {
    int retcode = SUCCESS;
	ssize_t io_bytes;
    FS *fs = get_fs_from_table(fd);
    if (omp_get_thread_num() == MASTER) {
        fs->ioprms->io_type = WRITE;
        fs->ioprms->input_type = LIST;
        fs->ioprms->merge_type = MEM;
        fs->ioprms->contig = NO;
        fs->ioprms->same_prms = YES;

        fs->ioprms->hint = hint;

        fs->ioprms->MEMSTR = NO;
        fs->ioprms->ALLSTR = YES;
    }

    fs->tlist_args[omp_get_thread_num()].iov = buflist;
    fs->tlist_args[omp_get_thread_num()].list_size = size;
    io_bytes = pthread_coll_io_base(fs);

    return io_bytes;
}

void Omp_File_write_com_list_all(void *buffer, int* size, int* fd, int* hint) {
    omp_file_write_com_list_all(buffer, *size, *fd, *hint);
}
#pragma weak omp_file_write_com_list_all__ = Omp_File_write_com_list_all
#pragma weak omp_file_write_com_list_all_  = Omp_File_write_com_list_all
#pragma weak OMP_FILE_WRITE_COM_LIST_ALL   = Omp_File_write_com_list_all

ssize_t
omp_file_write_com_list_at_all(void* buflist, off_t* offsets, int size, int fd, int hint) {
    int retcode = SUCCESS;
	ssize_t io_bytes;
    FS *fs = get_fs_from_table(fd);
    if (omp_get_thread_num() == MASTER) {
        fs->ioprms->io_type = WRITE;
        fs->ioprms->input_type = LIST;
        fs->ioprms->merge_type = ALL;
        fs->ioprms->contig = NO;
        fs->ioprms->same_prms = YES;

        fs->ioprms->hint = hint;

        fs->ioprms->MEMSTR = NO;
        fs->ioprms->ALLSTR = YES;
    }

    fs->tlist_args[omp_get_thread_num()].iov = buflist;
    fs->tlist_args[omp_get_thread_num()].offset_list = offsets;
    fs->tlist_args[omp_get_thread_num()].list_size = size;
    io_bytes = pthread_coll_io_base(fs);

    return io_bytes;
}

void
Omp_File_write_com_list_at_all(void* buffer, long* offsets, int* size, int* fd, int* hint) {
    omp_file_write_com_list_at_all(buffer, (off_t *) offsets, *size, *fd, *hint);
}
#pragma weak omp_file_write_com_list_at_all_ = Omp_File_write_com_list_at_all
#pragma weak omp_file_write_com_list_at_all__ = Omp_File_write_com_list_at_all
#pragma weak OMP_FILE_WRITE_COM_LIST_AT_ALL = Omp_File_write_com_list_at_all

