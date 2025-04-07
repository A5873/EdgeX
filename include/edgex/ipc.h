/*
 * EdgeX OS - Inter-Process Communication (IPC)
 *
 * This file defines the interface for the Inter-Process Communication
 * subsystem of EdgeX OS, including message passing, synchronization
 * primitives, event notification, and shared memory management.
 */

#ifndef EDGEX_IPC_H
#define EDGEX_IPC_H

#include <edgex/kernel.h>
#include <edgex/scheduler.h>

/*
 * Message Passing System
 */

/* Maximum size of a message in bytes */
#define MAX_MESSAGE_SIZE    1024

/* Message priorities */
typedef enum {
    MESSAGE_PRIORITY_LOW = 0,
    MESSAGE_PRIORITY_NORMAL = 1,
    MESSAGE_PRIORITY_HIGH = 2,
    MESSAGE_PRIORITY_URGENT = 3
} message_priority_t;

/* Message types */
typedef enum {
    MESSAGE_TYPE_NORMAL = 0,    /* Normal user-defined message */
    MESSAGE_TYPE_SIGNAL = 1,    /* Signal message (predefined meaning) */
    MESSAGE_TYPE_RESPONSE = 2,  /* Response to a previous message */
    MESSAGE_TYPE_SYSTEM = 3     /* System message from kernel */
} message_type_t;

/* Message header */
typedef struct {
    pid_t sender;               /* PID of sending task */
    pid_t receiver;             /* PID of receiving task */
    uint32_t message_id;        /* Unique message ID */
    message_type_t type;        /* Message type */
    message_priority_t priority;/* Message priority */
    uint32_t size;              /* Size of the message payload */
    uint32_t flags;             /* Message flags */
    uint64_t timestamp;         /* Timestamp when message was sent */
} message_header_t;

/* Message structure */
typedef struct {
    message_header_t header;    /* Message header */
    uint8_t payload[MAX_MESSAGE_SIZE]; /* Message payload */
} message_t;

/* Message queue handle */
typedef struct message_queue* message_queue_t;

/* Message sending flags */
#define MESSAGE_FLAG_BLOCKING    (1 << 0)  /* Block if queue is full */
#define MESSAGE_FLAG_WAIT_REPLY  (1 << 1)  /* Wait for a reply */
#define MESSAGE_FLAG_URGENT      (1 << 2)  /* Urgent message, goes to front of queue */

/* Message queue functions */
message_queue_t create_message_queue(const char* name, uint32_t max_messages);
void destroy_message_queue(message_queue_t queue);
int send_message(message_queue_t queue, message_t* message, uint32_t flags);
int receive_message(message_queue_t queue, message_t* message, uint32_t flags);
int reply_to_message(message_t* original, message_t* reply, uint32_t flags);

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

