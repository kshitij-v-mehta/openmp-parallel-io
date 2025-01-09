#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "defs.h"
#include "thread_state_handler.h"
#include "access_work_assignment.h"
#include<omp.h>

/**
Lock mutex of the thread and signal on cond_var. \n
Unlock mutex and exit. \n
 */
void invoke_thread(FS *fs, int slaveid) {
    //pthread_mutex_t slave_mutex = fs->sync_objs[slaveid].mutex;
    //pthread_cond_t slave_cond = fs->sync_objs[slaveid].cond_var;

    if (DEBUG) fprintf(stdout, "Master invoking thread %d\n", slaveid);
    pthread_mutex_lock(&(fs->sync_objs[slaveid].mutex));
    if (0 != pthread_cond_signal(&(fs->sync_objs[slaveid].cond_var))) {
        fprintf(stdout, "Error: master, invoke_thread: Couldn't signal slave %d\n", slaveid);
    }
    if (DEBUG) fprintf(stdout, "Master invoked thread %d\n", slaveid);
    pthread_mutex_unlock(&(fs->sync_objs[slaveid].mutex));
}

/**
Signal every thread individually. \n
 */
void invoke_all(FS *fs) {
    if (DEBUG) fprintf(stdout, "Master entered invoke all\n");
    fs->all_quit_flag = 1;
    int i;
    for (i = 0; i < omp_get_num_threads(); i++) {
        pthread_mutex_lock(&(fs->sync_objs[i].mutex));
        if (0 != pthread_cond_signal(&fs->sync_objs[i].cond_var)) {
            fprintf(stdout, "Error: master, invoke_all: pthread_cond_signal error for thread %d\n", i);
        }
        pthread_mutex_unlock(&(fs->sync_objs[i].mutex));
    }
}

/**
 * lock.
 * while (status != QUIT)
 * if listhead == NULL, wait
 * while(listhead != null) io_preface();
 * unlock.
 */
ssize_t 
enter_wait_state(FS *fs) {
    int retcode = SUCCESS;
    ssize_t io_bytes;
    if (DEBUG) fprintf(stdout, "Thread %d entered wait state\n", omp_get_thread_num());
    int mytid = omp_get_thread_num();

    pthread_mutex_lock(&(fs->sync_objs[mytid].mutex));
    while (fs->all_quit_flag != 1) {
        if (fs->ioassignments[mytid].completed == fs->ioassignments[mytid].listlen) //Should we use same condition for listio too?
        {
            if (DEBUG) fprintf(stdout, "Thread %d waiting on cond var \n", mytid);
            pthread_cond_wait(&(fs->sync_objs[mytid].cond_var), &(fs->sync_objs[mytid].mutex));
            if (DEBUG) fprintf(stdout, "Thread %d woken from sleep\n", mytid);
        }

        while (fs->ioassignments[mytid].completed < fs->ioassignments[mytid].listlen) //doesnt look at the status=ASSIGNED flag
        {
            pthread_mutex_unlock(&(fs->sync_objs[mytid].mutex));
            if (DEBUG) fprintf(stdout, "Thread %d accessing work assignment now \n", mytid);
            io_bytes = access_work_assignment(fs);
            //delete listnode ? no. done in access_work_assignment
            pthread_mutex_lock(&(fs->sync_objs[mytid].mutex));
        }
    }
    pthread_mutex_unlock(&(fs->sync_objs[mytid].mutex));

    //quit flag set on arrival, but work was assigned to me
    if (DEBUG) fprintf(stdout, "Thread %d, iteration %d found quit flag set on arrival. "
            "Checking work assignment now\n",
            mytid, fs->debugInfo[mytid].iteration);

    while (fs->ioassignments[mytid].completed < fs->ioassignments[mytid].listlen) {
        io_bytes = access_work_assignment(fs);
    }

    fs->ioassignments[mytid].status = RESET;
    fs->ioassignments[mytid].listlen = 0;
    fs->ioassignments[mytid].completed = 0;
    fs->ioassignments[mytid].iovlisthead = NULL;
    return io_bytes;
}

