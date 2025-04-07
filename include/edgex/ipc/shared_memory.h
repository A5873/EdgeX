/*
 * EdgeX OS - Shared Memory Interface
 *
 * This file defines the interface for shared memory in EdgeX OS,
 * which allows tasks to share memory regions with each other.
 */

#ifndef EDGEX_IPC_SHARED_MEMORY_H
#define EDGEX_IPC_SHARED_MEMORY_H

#include <edgex/ipc/common.h>

/* Shared memory region handle (opaque) */
typedef struct shared_memory* shared_memory_t;

/* Shared memory access permissions */
#define SHM_PERM_READ     (1 << 0)  /* Read permission */
#define SHM_PERM_WRITE    (1 << 1)  /* Write permission */
#define SHM_PERM_EXEC     (1 << 2)  /* Execute permission */

/* Shared memory creation flags */
#define SHM_FLAG_CREATE   (1 << 0)  /* Create new region */
#define SHM_FLAG_EXCL     (1 << 1)  /* Fail if region exists */
#define SHM_FLAG_PRIVATE  (1 << 2)  /* Create private (not shared) region */

/* Shared memory functions */
shared_memory_t create_shared_memory(const char* name, size_t size, uint32_t permissions, uint32_t flags);
void destroy_shared_memory(shared_memory_t shm);
void* map_shared_memory(shared_memory_t shm, void* addr_hint, uint32_t permissions);
int unmap_shared_memory(void* addr, size_t size);
int resize_shared_memory(shared_memory_t shm, size_t new_size);

/* Internal functions */
void init_shared_memory_subsystem(void);
void cleanup_task_shared_memory(pid_t pid);
void dump_all_shared_memory_regions(void);

#endif /* EDGEX_IPC_SHARED_MEMORY_H */

