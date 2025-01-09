#include <stdio.h>
#include <stdlib.h>
#include <sys/uio.h>
#include "defs.h"
#include<omp.h>

/**
 * This function merges lists when same_params = NO. Doesnt matter at this stage whether contiguous or not.
 * Iterate through lists of each thread. Add data to merged_args.
 * merged_args_len = sum of the lengths of all lists.
 */
int merge_args_lists(FS *fs)
{
    int retcode = SUCCESS;

    if(DEBUG) 
        fprintf(stdout, "Master merging args lists\n");
	int i,j,index;
	fs->merged_args_len = 0;
	index = 0;

	for(i=0; i<omp_get_num_threads(); i++)
	{
		fs->merged_args_len += fs->tlist_args[i].list_size;
	}

	fs->merged_targs = (thread_args *) malloc (fs->merged_args_len * sizeof(thread_args));

	for(i=0; i<omp_get_num_threads(); i++)
	{
		for(j=0; j<fs->tlist_args[i].list_size; j++)
		{
			fs->merged_targs[index].bufptr = (char*) fs->tlist_args[i].iov[j].iov_base;
			fs->merged_targs[index].buflen = fs->tlist_args[i].iov[j].iov_len;
			if (fs->ioprms->merge_type == ALL)
				fs->merged_targs[index].offset = fs->tlist_args[i].offset_list[j];
            index++;
		}
	}
    if(DEBUG) fprintf(stdout, "Master done with merging args list. len = %d\n", fs->merged_args_len);
    return retcode;
}
/**
 * Create a new iovnode and set bufptr, buflen and offset.
 * create an iterator node and iterate through slave's iovlisthead.
 * attach wherever next = NULL.
 * Increment list_len for that slave.
 */
/*int add_to_assignment_list(FS *fs, void *_bufptr, long _buflen, off_t _offset)
{
    int retcode = SUCCESS;

	iovecnode * iovnode;
	iovnode = (iovecnode *) malloc (sizeof (iovecnode));
	iovnode->bufptr = _bufptr;
	iovnode->buflen = _buflen;
	iovnode->offset = _offset;
	iovnode->next = NULL;

	pthread_mutex_lock(&((fs->sync_objs[fs->nxt_slave]).mutex));

	iovecnode *iterator;
	iterator = (iovecnode *) (fs->ioassignments[fs->nxt_slave].iovlisthead);

	if(iterator == NULL)
	{
		fs->ioassignments[fs->nxt_slave].iovlisthead = iovnode;
	}
	else
	{
		while(iterator->next != NULL)
			iterator = iterator -> next;
		iterator->next = iovnode;
	}

	fs->ioassignments[fs->nxt_slave].listlen ++ ;

	pthread_mutex_unlock(&((fs->sync_objs[fs->nxt_slave]).mutex));
    return retcode;
} */

int delete_top_node(FS *fs)
{
    int retcode = SUCCESS;
	int mytid = omp_get_thread_num();

	iovecnode *tmp = fs->ioassignments[mytid].iovlisthead;

	fs->ioassignments[mytid].iovlisthead = fs->ioassignments[mytid].iovlisthead->next;
    fs->ioassignments[mytid].listlen--;

	free(tmp);

    return retcode;
}
