#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "defs.h"
#include <omp.h>

pthread_barrier_t barr;

void ompc_barrier_init(int num_threads)
{
    if(num_threads > 0)
        pthread_barrier_init(&barr, NULL, num_threads);
    else
	pthread_barrier_init(&barr, NULL, atoi(getenv("OMP_NUM_THREADS")));
}
#pragma weak ompc_barrier_init_ = Ompc_barrier_init
#pragma weak ompc_barrier_init__ = Ompc_barrier_init
#pragma weak OMPC_BARRIER_INIT = Ompc_barrier_init

void Ompc_barrier_init(int *num_threads)
{
    ompc_barrier_init(*num_threads);
}

void ompc_barrier_destroy()
{
	pthread_barrier_destroy(&barr);
}

void __ompc_barrier()
{
	pthread_barrier_wait(&barr);
}

