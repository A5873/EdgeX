/*
 * EdgeX OS - Semaphore Interface
 *
 * This file defines the interface for semaphores in EdgeX OS,
 * which are used for synchronization and resource counting.
 */

#ifndef EDGEX_IPC_SEMAPHORE_H
#define EDGEX_IPC_SEMAPHORE_H

#include <edgex/ipc/common.h>

/* Semaphore structure (opaque) */
typedef struct semaphore* semaphore_t;

/* Semaphore functions */
semaphore_t create_semaphore(const char* name, uint32_t initial_value);
void destroy_semaphore(void* semaphore);
int semaphore_wait(semaphore_t semaphore);
int semaphore_trywait(semaphore_t semaphore);
int semaphore_post(semaphore_t semaphore);
int semaphore_getvalue(semaphore_t semaphore, int* value);

/* Internal functions */
void init_semaphore_subsystem(void);
void cleanup_task_semaphores(pid_t pid);
void dump_all_semaphores(void);

#endif /* EDGEX_IPC_SEMAPHORE_H */

