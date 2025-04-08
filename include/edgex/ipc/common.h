/*
 * EdgeX OS - IPC Common Definitions
 *
 * This file defines common structures, constants, and interfaces used across
 * all Inter-Process Communication (IPC) mechanisms in EdgeX OS.
 *
 * The EdgeX IPC subsystem provides a unified framework for various communication
 * mechanisms between tasks, including:
 *   - Mutexes and semaphores for synchronization
 *   - Events for notification
 *   - Message queues for data exchange
 *   - Shared memory for efficient data sharing
 *
 * This common header defines the building blocks used by all IPC mechanisms,
 * including object type definitions, statistics tracking, and system-wide
 * management functions.
 */

#ifndef EDGEX_IPC_COMMON_H
#define EDGEX_IPC_COMMON_H

#include <edgex/kernel.h>
#include <edgex/scheduler.h>

/* Maximum name length for IPC objects 
 * This defines the maximum length for names used to identify IPC objects.
 * Names are used for debugging, lookup, and human-readable identification.
 * All IPC objects (mutexes, semaphores, events, message queues, shared memory)
 * use this same maximum length for their names.
 */
#define MAX_IPC_NAME_LENGTH 64

/* IPC statistics types 
 * These constants define the types of statistics events that can be tracked
 * by the IPC subsystem. They are used with the update_ipc_stats() function
 * to record various events within the IPC mechanisms.
 *
 * IPC_STAT_OBJECT_CREATED: Increment when an IPC object is created
 * IPC_STAT_OBJECT_DESTROYED: Increment when an IPC object is destroyed
 */
#define IPC_STAT_OBJECT_CREATED   1  /* An IPC object was created */
#define IPC_STAT_OBJECT_DESTROYED 2  /* An IPC object was destroyed */

/* IPC object types 
 * This enumeration defines all the types of IPC objects supported in EdgeX OS.
 * Each type represents a different mechanism for inter-process communication
 * or synchronization.
 *
 * IPC_TYPE_MUTEX: Mutual exclusion lock for protecting access to shared resources
 * IPC_TYPE_SEMAPHORE: Counting semaphore for controlling access to limited resources
 * IPC_TYPE_EVENT: Binary event flag for signaling between tasks
 * IPC_TYPE_EVENT_SET: Group of event flags that can be waited on simultaneously
 * IPC_TYPE_MESSAGE_QUEUE: Queue for passing messages between tasks
 * IPC_TYPE_SHARED_MEMORY: Shared memory region accessible by multiple tasks
 */
typedef enum {
    IPC_TYPE_MUTEX = 1,      /* Mutual exclusion lock */
    IPC_TYPE_SEMAPHORE,      /* Counting semaphore */
    IPC_TYPE_EVENT,          /* Binary event flag */
    IPC_TYPE_EVENT_SET,      /* Group of event flags */
    IPC_TYPE_MESSAGE_QUEUE,  /* Message passing queue */
    IPC_TYPE_SHARED_MEMORY   /* Shared memory region */
} ipc_object_type_t;

/* Basic IPC object header (common to all IPC objects)
 * This structure serves as the common header for all IPC objects in EdgeX OS.
 * Every IPC object (mutex, semaphore, event, message queue, shared memory)
 * begins with this header, allowing for generic handling of different IPC types.
 *
 * The object-oriented design allows polymorphic operations on IPC objects,
 * such as reference counting, naming, and type identification.
 */
typedef struct {
    uint32_t type;                    /* IPC object type from ipc_object_type_t */
    char name[MAX_IPC_NAME_LENGTH];   /* Human-readable object name */
    uint32_t ref_count;               /* Reference count for shared objects */
    void (*destroy_fn)(void*);        /* Function pointer to destroy this object */
} ipc_object_header_t;

/* IPC statistics structure
 * This structure maintains comprehensive statistics about the IPC subsystem's
 * usage and performance. These statistics are valuable for monitoring system
 * health, detecting bottlenecks, debugging deadlocks, and optimizing resource
 * usage in EdgeX applications.
 *
 * The statistics are updated automatically by the IPC subsystem and can be
 * retrieved using the get_ipc_stats() function or viewed using print_ipc_stats().
 */
typedef struct ipc_stats {
    /* General stats - Track overall IPC object lifecycle */
    uint64_t ipc_objects_created;     /* Total number of IPC objects created */
    uint64_t ipc_objects_destroyed;   /* Total number of IPC objects destroyed */
    
    /* Object counts - Current number of each type of IPC object */
    uint32_t mutex_count;             /* Current number of mutexes */
    uint32_t semaphore_count;         /* Current number of semaphores */
    uint32_t event_count;             /* Current number of events */
    uint32_t event_set_count;         /* Current number of event sets */
    uint32_t message_queue_count;     /* Current number of message queues */
    uint32_t shared_memory_count;     /* Current number of shared memory regions */
    
    /* Operation counts - Number of operations performed on each IPC type */
    uint64_t mutex_operations;        /* Lock/unlock operations on mutexes */
    uint64_t semaphore_operations;    /* Wait/signal operations on semaphores */
    uint64_t event_operations;        /* Wait/set operations on events */
    uint64_t message_operations;      /* Send/receive operations on message queues */
    uint64_t shared_memory_operations; /* Map/unmap operations on shared memory */
    
    /* Wait statistics - Information about waiting on IPC objects */
    uint64_t total_wait_time;         /* Cumulative time tasks spent waiting (in ms) */
    uint32_t active_waiters;          /* Number of tasks currently waiting on IPC objects */
    uint32_t timeouts;                /* Number of waits that ended due to timeout */
    
    /* Error counts - Various failure conditions */
    uint32_t allocation_failures;     /* Memory allocation failures during IPC operations */
    uint32_t permission_failures;     /* Permission denied during IPC operations */
    uint32_t timeout_failures;        /* Operations that failed due to timeout */
} ipc_stats_t;

/* Functions for IPC statistics */

/**
 * Update a specific IPC statistic
 *
 * @param stat_type  Type of statistic to update (e.g., IPC_STAT_OBJECT_CREATED)
 * @param value      Value to add to the statistic (usually 1)
 *
 * This function is used internally by the IPC subsystem to update various
 * statistics counters. It should generally not be called directly by
 * application code.
 *
 * The stat_type parameter determines which statistic is updated, and the
 * value parameter specifies the amount to add to the counter (which can
 * be negative to decrement).
 */
void update_ipc_stats(int stat_type, int value);

/**
 * Get the current IPC statistics
 *
 * @param stats  Pointer to an ipc_stats_t structure to be filled with current statistics
 *
 * This function fills the provided structure with the current IPC subsystem
 * statistics. This allows applications to monitor IPC usage and detect potential
 * issues like resource leaks or performance bottlenecks.
 *
 * Example:
 *   ipc_stats_t stats;
 *   get_ipc_stats(&stats);
 *   if (stats.active_waiters > 10) {
 *       log_warning("High number of waiting tasks: %d", stats.active_waiters);
 *   }
 */
void get_ipc_stats(ipc_stats_t* stats);

/**
 * Print the current IPC statistics to the system log
 *
 * This function prints a formatted summary of all current IPC statistics
 * to the system log. It's useful for debugging and monitoring system health.
 *
 * Example:
 *   // Print IPC stats every hour
 *   void monitor_task(void) {
 *       while (1) {
 *           print_ipc_stats();
 *           sleep_ms(3600000);
 *       }
 *   }
 */
void print_ipc_stats(void);

/**
 * Reset all IPC statistics counters to zero
 *
 * This function resets all statistics counters in the IPC subsystem to zero,
 * including operation counts, wait statistics, and error counts. Object counts
 * are not reset since they reflect current system state.
 *
 * This is useful for establishing a new baseline for measurements or after
 * clearing a problematic condition.
 */
void reset_ipc_stats(void);

/* IPC subsystem initialization and management */

/**
 * Initialize all IPC subsystems
 *
 * @return true if initialization was successful, false otherwise
 *
 * This function initializes all the IPC subsystems, including mutexes, semaphores,
 * events, message queues, and shared memory. It is automatically called during
 * system startup and should not be called directly by application code.
 *
 * If initialization fails, the kernel will enter a error state as the IPC
 * subsystem is critical for system operation.
 */
bool init_ipc_subsystems(void);

/**
 * Get the last error message from an IPC operation
 *
 * @return Pointer to a string containing the last error message
 *
 * This function returns a pointer to a static string containing the description
 * of the last error that occurred during an IPC operation. This is useful for
 * debugging and error reporting.
 *
 * Example:
 *   if (mutex_lock(my_mutex) != 0) {
 *       log_error("Failed to lock mutex: %s", get_last_ipc_error());
 *   }
 */
const char* get_last_ipc_error(void);

/**
 * Check the health of the IPC subsystem
 *
 * @return true if the IPC subsystem is healthy, false if there are problems
 *
 * This function performs a comprehensive health check of the IPC subsystem,
 * verifying that all subsystems are functioning properly, resource usage is
 * within acceptable limits, and no deadlocks or resource leaks are detected.
 *
 * Example:
 *   if (!check_ipc_health()) {
 *       log_warning("IPC subsystem health check failed");
 *       print_ipc_stats();
 *       dump_ipc_objects();
 *   }
 */
bool check_ipc_health(void);

/**
 * Dump information about all IPC objects to the system log
 *
 * This function prints detailed information about all existing IPC objects
 * to the system log, including their type, name, reference count, and owner.
 * This is useful for debugging IPC-related issues, especially resource leaks
 * or permission problems.
 *
 * Example:
 *   // When debugging a resource leak or deadlock
 *   dump_ipc_objects();
 *   
 *   // Output might look like:
 *   // IPC Object: mutex "fs_access" (refs: 3, owner: pid 4)
 *   // IPC Object: message_queue "system_events" (refs: 7, owner: pid 1)
 */
void dump_ipc_objects(void);

/**
 * Register an IPC object in the system registry
 *
 * @param header  Pointer to the IPC object header
 * @param owner   PID of the task that owns the object
 * 
 * @return true if registration succeeded, false otherwise
 *
 * This function is used internally by the IPC subsystem to register newly
 * created IPC objects in the global registry. It should not be called directly
 * by application code.
 *
 * The registry allows the system to track all IPC objects, clean up resources
 * when tasks terminate, and provide debugging information.
 */
bool register_ipc_object(ipc_object_header_t* header, pid_t owner);

/**
 * Unregister an IPC object from the system registry
 *
 * @param header  Pointer to the IPC object header
 * 
 * This function is used internally by the IPC subsystem to remove objects
 * from the global registry when they are destroyed. It should not be called
 * directly by application code.
 */
void unregister_ipc_object(ipc_object_header_t* header);

#endif /* EDGEX_IPC_COMMON_H */

