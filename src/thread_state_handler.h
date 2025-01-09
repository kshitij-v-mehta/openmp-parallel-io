#ifndef __THREAD_STATE_HANDLER__
#define __THREAD_STATE_HANDLER__
#include <pthread.h>

#include "defs.h"

void invoke_thread(FS *fs, int slaveid);
void invoke_all(FS *fs);
ssize_t enter_wait_state(FS *fs);

#endif
