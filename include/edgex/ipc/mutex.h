/*
 * EdgeX OS - Mutex Interface
 *
 * This file defines the interface for mutual exclusion locks (mutexes)
 * in EdgeX OS, which are used to protect shared resources from
 * concurrent access.
 */

#ifndef EDGEX_IPC_MUTEX_H
#define EDGEX_IPC_MUTEX_H

#include <edgex/ipc/common.h>

/* Mutex structure (opaque) */
typedef struct mutex* mutex_t;

/* Mutex functions */
mutex_t create_mutex(const char* name);
void destroy_mutex(void* mutex);
int mutex_lock(mutex_t mutex);
int mutex_trylock(mutex_t mutex);
int mutex_unlock(mutex_t mutex);

/* Internal functions */
void init_mutex_subsystem(void);
void cleanup_task_mutexes(pid_t pid);
void dump_all_mutexes(void);

#endif /* EDGEX_IPC_MUTEX_H */

