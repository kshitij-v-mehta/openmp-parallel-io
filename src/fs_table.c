#include <stdio.h>
#include "defs.h"
#include<omp.h>

extern int DEBUG;

FS *fs_table[1000]; //TODO: for now. Make this dynamic later on. 
int fs_table_size = 0;

int insert_into_fs_table(FS* fs)
{
    fs_table[fs_table_size] = fs;
    fs_table_size++;

    return 0;
}

FS* get_fs_from_table(int fd)
{
    int i, j;
    for (i=0; i<fs_table_size; i++)
    {
        FS* temp_fs = fs_table[i];
        for(j=0; j<omp_get_num_threads(); j++)
        {
            if(temp_fs->fd[j] == fd) {
                if(DEBUG)
                    fprintf(stdout, "Thread %d inside get_fs_from_table. fs found.\n", omp_get_thread_num());
                return temp_fs;
            }
        }
    }

    return NULL;
}

int remove_fs_from_table(int fd) {
    fs_table_size--;
    return SUCCESS;
}
