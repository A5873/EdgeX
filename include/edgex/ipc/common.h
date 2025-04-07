/*
 * EdgeX OS - IPC Common Definitions
 *
 * This file defines common structures and constants used across
 * all IPC mechanisms in EdgeX OS.
 */

#ifndef EDGEX_IPC_COMMON_H
#define EDGEX_IPC_COMMON_H

#include <edgex/kernel.h>
#include <edgex/scheduler.h>

/* Maximum name length for IPC objects */
#define MAX_IPC_NAME_LENGTH 64

/* IPC statistics types */
#define IPC_STAT_OBJECT_CREATED   1
#define IPC_STAT_OBJECT_DESTROYED 2

/* IPC object types */
typedef enum {
    IPC_TYPE_MUTEX = 1,
    IPC_TYPE_SEMAPHORE,
    IPC_TYPE_EVENT,
    IPC_TYPE_EVENT_SET,
    IPC_TYPE_MESSAGE_QUEUE,
    IPC_TYPE_SHARED_MEMORY
} ipc_object_type_t;

/* Basic IPC object header (common to all IPC objects) */
typedef struct {
    uint32_t type;                /* IPC object type */
    char name[MAX_IPC_NAME_LENGTH]; /* Object name */
    uint32_t ref_count;           /* Reference count */
    void (*destroy_fn)(void*);    /* Function to destroy this object */
} ipc_object_header_t;

/* IPC statistics structure */
typedef struct ipc_stats {
    /* General stats */
    uint64_t ipc_objects_created;
    uint64_t ipc_objects_destroyed;
    
    /* Object counts */
    uint32_t mutex_count;
    uint32_t semaphore_count;
    uint32_t event_count;
    uint32_t event_set_count;
    uint32_t message_queue_count;
    uint32_t shared_memory_count;
    
    /* Operation counts */
    uint64_t mutex_operations;
    uint64_t semaphore_operations;
    uint64_t event_operations;
    uint64_t message_operations;
    uint64_t shared_memory_operations;
    
    /* Wait statistics */
    uint64_t total_wait_time;
    uint32_t active_waiters;
    uint32_t timeouts;
    
    /* Error counts */
    uint32_t allocation_failures;
    uint32_t permission_failures;
    uint32_t timeout_failures;
} ipc_stats_t;

/* Functions for IPC statistics */
void update_ipc_stats(int stat_type, int value);
void get_ipc_stats(ipc_stats_t* stats);
void print_ipc_stats(void);
void reset_ipc_stats(void);

/* IPC subsystem initialization */
bool init_ipc_subsystems(void);
const char* get_last_ipc_error(void);
bool check_ipc_health(void);
void dump_ipc_objects(void);

#endif /* EDGEX_IPC_COMMON_H */

