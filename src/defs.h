#ifndef __defs__
#define __defs__

#include <pthread.h>
#include <unistd.h>
#ifdef __PLFS_H_
#include <plfs.h>
#endif

#define MASTER 0
#define SLEEP -1
#define QUIT -2
#define RESET -3
#define ACTIVATED 2
#define EMPTY 0
#define ERROR -1
#define ASSIGNED 1

#define SUCCESS 0
#define WRITE_ERROR -4
#define FILE_OPEN_ERROR -5
#define FILE_CLOSE_ERROR -6

#define ALL 0
#define MEM 1
#define BUF 1
#define LIST 2

#define CONTINUE 0
#define FINISH 1
#define CONTIG_FILE_BLOCK 2
#define CONTIG_AND_FINISH 3

#define CONTIGUOUS 0
#define DISC 1
#define CONT 0

#define READ 1
#define WRITE 2

extern int DEBUG;

#define YES 1
#define NO 0

//io pattern. How do threads write data
#define INTERLEAVED 0
#define BLOCK 1
#define ALLNONCONTIG 2  //used for listio offsets

//System config options - Move these to a System.config file later on. Look at the ATF code for reading a config file.
#ifndef __system_config__
#define __system_config__

//extern int DEBUG;
#define STRIPE_DEPTH 1048576
#define STRIPE_FACTOR 22

#endif 

#ifndef __list_queue__
#define __list_queue__

typedef struct _list_queue
{
    char *bufptr;
    long buflen;
    long offset;
    int slave_id;
    struct _list_queue * next;
} list_queue, a_list;
#endif

/*#ifndef __a_list__
#define __a_list__
typedef struct _a_list
{
    char* bufptr;
    long buflen;
    long offset;
    int slave_id;
    struct _a_list* next;
} a_list;
#endif
*/

#ifndef __FILE_STRUCTURE__
#define __FILE_STRUCTURE__
#define _LARGEFILE64_SOURCE 1
typedef struct _fcache
{
	int str_size;
	int str_depth;
} fcache;

typedef struct _iovt
{
    //This struct is input to io_preface. 
	struct iovec *iovlist;
	off_t offset;
	int listlen;
} iovt;

typedef struct _thread_args
{
    char* bufptr;
    long buflen;
    off_t offset;
    //int fd;
} thread_args;

typedef struct _thread_list_args
{
    struct iovec* iov;
    off_t *offset_list;
    int list_size;
    //int fd;
} thread_list_args;

typedef struct _sync_obj
{
    pthread_mutex_t mutex;
    pthread_cond_t cond_var;
} sync_obj;

typedef struct _iovecnode
{
	void *bufptr;
	long buflen;
	off_t offset;
	struct _iovecnode *next;
} iovecnode;

typedef struct _contigFileBlock
{
    off_t offset;
    int assigned_to;
    struct iovec* iov;
	int size;
    struct _contigFileBlock* next;
} contigFileBlock;

typedef struct _listIOPool
{
    contigFileBlock* contigFileBlockList;
    int listsize;
} listIOPool;

typedef struct _workassignment
{
	//these to be used only for mem and buf
    void *bufptr;
    long buflen;
    off_t offset;
    
    //these to be used for all list interfaces
    iovecnode* iovlisthead; //This has to go and an array has to come here
    int listlen, completed;

    int status;
} workassignment;

enum lowlevelioprimitives{prw, rwv, arw, _mmap};

typedef struct _ioparams
{
    char *filename; //reqd by plfs
    int io_type;    //read/write
    int input_type; //buf/list
    int merge_type; //mem/all
    int contig;
    int same_prms;
    enum lowlevelioprimitives lliop;
    int hint;

    int MEMSTR;
    int ALLSTR;
} ioparams;

typedef struct _mmapinfo
{
    char *mapaddr;
    off_t map_len;
    off_t map_offset;
    int map_set;
} mmapinfo;

typedef struct __debuginfo
{
    int num_writes;
    int num_reads;
    int iteration;
} _debugInfo;

typedef struct _FS
{
    int status; //Not being used currently
    int *fd;
    int plfs;   //bool, set to 1 to enable plfs
#ifdef __PLFS_H_
    Plfs_fd **pfd;
#endif
    int open_flags;
    pthread_barrier_t barr;

    ioparams *ioprms;
    thread_args *targs;
    thread_list_args *tlist_args;
    sync_obj *sync_objs;
    workassignment *ioassignments;
    listIOPool listiopool;
    fcache fc;

    thread_args *merged_targs;
    int merged_args_len;
    off_t internal_offset;
    int nxt_slave;
    int active_threads;
    off_t max_write_block_size;

    mmapinfo *mapids;
    off_t static_mmap_len;

    int all_quit_flag; //global quit flag to avoid signalling every thread to quit
    int sync_flag;  //additional flag for use whenever needed

    _debugInfo* debugInfo;
} FS;

typedef struct __global_arr_elem
{
    FS *fs;
    int in_use;
} global_arr_elem;

typedef struct __global_interface_arr
{
    int size;
    global_arr_elem *elem;
} global_interface_arr;

#endif
#endif
