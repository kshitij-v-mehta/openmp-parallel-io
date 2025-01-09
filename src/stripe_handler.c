/*
 * stripe_handler.c
 *
 *  Created on: Apr 27, 2010
 *      Author: kmehta
 */

#include <stdlib.h>
#include <sys/uio.h>
#include "io_preface.h"
#include "defs.h"

/**
 * Create any data struct you want. No need to set it in FS.
 * Create an iovt data struct and store all generated stripe data into it.
 * Either pass generated data to pread/pwrite as it is being generated .OR.
 * return to calling function .OR.
 * call io_preface at the end of the calling function.
 * Again note that iovt is not being stored in FS.
 * iovt contains 1. list of iovecs, 2. 1 single offset (for readv/write initial positioning), 3. generated listlen
 * No. of chunks to be created = initia_write + x chunks + remainder chunk
 */
int stripe_handler(FS *fs, void *bufptr, long buflen, off_t offset)
{
    int retcode = SUCCESS;
	iovt * vect;
	vect = (iovt *) malloc (sizeof(iovt));
	int i;
    long align_chunk_len;
    void *curbufptr;
    curbufptr = bufptr;
    long rem_buflen = buflen;

	if(buflen <= fs->fc.str_size)
	{
		vect->iovlist = (struct iovec *) malloc (sizeof(struct iovec));
		vect->iovlist[0].iov_base = bufptr;
		vect->iovlist[0].iov_len = buflen;
		vect->offset = offset;
		vect->listlen = 1;
	}

	else
	{
		if(offset % fs->fc.str_size !=0 )
		{
		    //calculate initial write
            vect->iovlist = (struct iovec *) malloc ((buflen/fs->fc.str_size+2) * sizeof(struct iovec));
            align_chunk_len = fs->fc.str_size - offset % fs->fc.str_size;
            vect->iovlist[vect->listlen].iov_base = bufptr;
            vect->iovlist[vect->listlen].iov_len = align_chunk_len;
            vect->listlen ++;
            *(char*)curbufptr = *(char*)bufptr+align_chunk_len;
            rem_buflen -= align_chunk_len;

            //option 1: send to io_preface now, chunk by chunk, which defeats the purpose of writev, NOT PREFERRED
            //add remaining chunks
            for(i=0; i<rem_buflen/fs->fc.str_size; i++)
            {
                vect->iovlist[vect->listlen].iov_base = curbufptr;
                vect->iovlist[vect->listlen].iov_len = fs->fc.str_size;
                *(char*)curbufptr = *(char*)curbufptr + fs->fc.str_size;
                vect->listlen++;
                rem_buflen -= fs->fc.str_size;
            }

            //add remaining buffer
            if(rem_buflen > 0)
            {
                vect->iovlist[vect->listlen].iov_base = curbufptr;
                vect->iovlist[vect->listlen].iov_len = rem_buflen;
                vect->listlen ++;

                //option: send to io_preface now, containing a bunch of chunks, so that you can writev
            }
		}
	}

    io_preface(fs, vect);
    //option 2: return to calling function or call io_preface, do ALL the io later once all iovecs have been created
    //but how do u do that? u will need a linkedlist of iovt, and iovt is not a linkedlist
    return retcode;
}

