#include <stdio.h>
#include <stdlib.h>
#include "defs.h"
#include "fs_table.h"
#include<omp.h>

int* get_num_io(int fd) {
    int i;
    int *num_io = (int *) malloc (omp_get_num_threads() * sizeof(int));
    FS* fs = get_fs_from_table(fd);
    if(fs->ioprms->io_type == WRITE) {
        for(i=0; i<omp_get_num_threads(); i++) {
            num_io[i] = fs->debugInfo[i].num_writes;
        }
    }
    else {
        for(i=0; i<omp_get_num_threads(); i++) {
            num_io[i] = fs->debugInfo[i].num_reads;
        }
    }

    return num_io;
}

int omp_print_debug_info(int fd)
{
    int *num_io = get_num_io(fd);
    int i;
    for(i=0; i<omp_get_num_threads(); i++)
        printf("Num io for t%d: %d\n", i, num_io[i]);
    
    return 0;
}

