#include "defs.h"

void work_manager_init();
int work_manager(FS *fs, void *bufptr, long buflen, off_t offset, int signal);
void reset_work_manager();
