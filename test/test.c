#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "omp_io_interfaces.h"

#define NUM_THREADS 8
int fd;

int main() {
    int retval;
    int buf = 0;

    //If the parallel IO library is NOT part of a compiler, initialize barrier explicitly
    ompc_barrier_init(NUM_THREADS);

#pragma omp parallel private(retval,buf) shared(fd) num_threads(NUM_THREADS)
    {
        buf = omp_get_thread_num();

        retval = omp_file_open_all(&fd, "test.out", O_CREAT|O_RDWR);
        if(retval != 0 || fd <= 0) {
            perror("Error during open. Aborting..");
            exit(-1);
        }

        omp_file_write_all(&buf, sizeof(int), fd, -1);

        omp_file_close_all(fd);
    }

    return 0;
}

