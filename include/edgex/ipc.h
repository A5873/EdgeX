/*
 * EdgeX OS - Inter-Process Communication
 *
 * This file defines the interface for inter-process communication,
 * including message passing, synchronization primitives, and shared memory.
 */

#ifndef EDGEX_IPC_H
#define EDGEX_IPC_H
/*
 * Synchronization Primitives
 */

/* Mutex structure (opaque) */
typedef struct mutex* mutex_t;

/* Mutex functions */
mutex_t create_mutex(const char* name);
void destroy_mutex(mutex_t mutex);
int mutex_lock(mutex_t mutex);
int mutex_trylock(mutex_t mutex);
int mutex_unlock(mutex_t mutex);

/* Semaphore structure (opaque) */
typedef struct semaphore* semaphore_t;

/* Semaphore functions */
semaphore_t create_semaphore(const char* name, uint32_t initial_value);
void destroy_semaphore(semaphore_t semaphore);
int semaphore_wait(semaphore_t semaphore);
int semaphore_trywait(semaphore_t semaphore);
int semaphore_post(semaphore_t semaphore);
int semaphore_getvalue(semaphore_t semaphore, int* value);

/*
 * Event Notification System
 */

/* Event handle (opaque) */
typedef struct event* event_t;

/* Event functions */
event_t create_event(const char* name);
void destroy_event(event_t event);
int event_wait(event_t event);
int event_timedwait(event_t event, uint64_t timeout_ms);
int event_signal(event_t event);
int event_broadcast(event_t event);
int event_reset(event_t event);

/* Event set (for waiting on multiple events) */
typedef struct event_set* event_set_t;

/* Event set functions */
event_set_t create_event_set(const char* name, uint32_t max_events);
void destroy_event_set(event_set_t event_set);
int event_set_add(event_set_t event_set, event_t event);
int event_set_remove(event_set_t event_set, event_t event);
int event_set_wait(event_set_t event_set, event_t* signaled_event);
int event_set_timedwait(event_set_t event_set, event_t* signaled_event, uint64_t timeout_ms);

/*
 * Shared Memory Management
 */

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

/*
 * IPC Initialization
 */
void init_ipc(void);

#endif /* EDGEX_IPC_H */

