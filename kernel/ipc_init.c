/*
 * EdgeX OS - IPC Subsystem Initialization
 *
 * This file implements the master initialization for all IPC subsystems
 * including mutexes, semaphores, events, message queues, and shared memory.
 */

#include <edgex/kernel.h>
#include <edgex/memory.h>
#include <edgex/scheduler.h>
#include <edgex/ipc.h>

/* Forward declarations of subsystem initialization functions */
extern void init_mutex_subsystem(void);
extern void init_semaphore_subsystem(void);
extern void init_event_subsystem(void);
extern void init_message_subsystem(void);
extern void init_shared_memory_subsystem(void);

/* Forward declarations of cleanup functions */
extern void cleanup_task_mutexes(pid_t pid);
extern void cleanup_task_semaphores(pid_t pid);
extern void cleanup_task_events(pid_t pid);
extern void cleanup_task_messages(pid_t pid);
extern void cleanup_task_shared_memory(pid_t pid);

/* Forward declarations of timeout checking functions */
extern void check_event_timeouts(void);
extern void check_message_timeouts(void);

/* IPC statistics */
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

/* Global IPC statistics */
static ipc_stats_t ipc_stats;

/* IPC subsystem initialization status */
static bool mutex_initialized = false;
static bool semaphore_initialized = false;
static bool event_initialized = false;
static bool message_initialized = false;
static bool shared_memory_initialized = false;

/* Last initialization error */
static char last_error[256] = {0};

/*
 * Record an initialization error
 */
static void record_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(last_error, sizeof(last_error), format, args);
    va_end(args);
    
    kernel_printf("IPC INIT ERROR: %s\n", last_error);
}

/*
 * Initialize IPC statistics
 */
static void init_ipc_stats(void) {
    memset(&ipc_stats, 0, sizeof(ipc_stats_t));
}

/*
 * Update IPC statistics
 */
void update_ipc_stats(int stat_type, int value) {
    // In a real implementation, this would be protected with a mutex
    
    switch (stat_type) {
        case IPC_STAT_OBJECT_CREATED:
            ipc_stats.ipc_objects_created++;
            break;
        case IPC_STAT_OBJECT_DESTROYED:
            ipc_stats.ipc_objects_destroyed++;
            break;
        // Add cases for other statistics as needed
        default:
            break;
    }
}

/*
 * Clean up all IPC resources for a terminated task
 */
void cleanup_task_ipc(pid_t pid) {
    // Clean up all IPC resources for the terminated task
    // Order is important to prevent deadlocks
    
    // First clean up mutexes and semaphores to release any locks
    cleanup_task_mutexes(pid);
    cleanup_task_semaphores(pid);
    
    // Then clean up higher-level constructs
    cleanup_task_events(pid);
    cleanup_task_messages(pid);
    cleanup_task_shared_memory(pid);
    
    kernel_printf("Cleaned up IPC resources for terminated task %d\n", pid);
}

/*
 * Check all timeouts for IPC objects
 */
void check_ipc_timeouts(void) {
    // This is called periodically by the scheduler
    
    // Check timeouts for various IPC objects
    check_event_timeouts();
    check_message_timeouts();
}

/*
 * Initialize the mutex subsystem
 */
static bool initialize_mutex_subsystem(void) {
    if (mutex_initialized) {
        return true;
    }
    
    kernel_printf("Initializing mutex subsystem...\n");
    
    // Call the mutex subsystem initialization
    init_mutex_subsystem();
    
    mutex_initialized = true;
    return true;
}

/*
 * Initialize the semaphore subsystem
 */
static bool initialize_semaphore_subsystem(void) {
    if (semaphore_initialized) {
        return true;
    }
    
    if (!mutex_initialized) {
        record_error("Cannot initialize semaphores before mutexes");
        return false;
    }
    
    kernel_printf("Initializing semaphore subsystem...\n");
    
    // Call the semaphore subsystem initialization
    init_semaphore_subsystem();
    
    semaphore_initialized = true;
    return true;
}

/*
 * Initialize the event subsystem
 */
static bool initialize_event_subsystem(void) {
    if (event_initialized) {
        return true;
    }
    
    if (!mutex_initialized) {
        record_error("Cannot initialize events before mutexes");
        return false;
    }
    
    kernel_printf("Initializing event subsystem...\n");
    
    // Call the event subsystem initialization
    init_event_subsystem();
    
    event_initialized = true;
    return true;
}

/*
 * Initialize the message subsystem
 */
static bool initialize_message_subsystem(void) {
    if (message_initialized) {
        return true;
    }
    
    if (!mutex_initialized) {
        record_error("Cannot initialize messages before mutexes");
        return false;
    }
    
    kernel_printf("Initializing message subsystem...\n");
    
    // Call the message subsystem initialization
    init_message_subsystem();
    
    message_initialized = true;
    return true;
}

/*
 * Initialize the shared memory subsystem
 */
static bool initialize_shared_memory_subsystem(void) {
    if (shared_memory_initialized) {
        return true;
    }
    
    if (!mutex_initialized) {
        record_error("Cannot initialize shared memory before mutexes");
        return false;
    }
    
    kernel_printf("Initializing shared memory subsystem...\n");
    
    // Call the shared memory subsystem initialization
    init_shared_memory_subsystem();
    
    shared_memory_initialized = true;
    return true;
}

/*
 * Initialize all IPC subsystems
 */
bool init_ipc_subsystems(void) {
    kernel_printf("Initializing EdgeX IPC subsystems...\n");
    
    // Initialize statistics tracking
    init_ipc_stats();
    
    // Initialize subsystems in dependency order
    if (!initialize_mutex_subsystem()) {
        return false;
    }
    
    if (!initialize_semaphore_subsystem()) {
        return false;
    }
    
    if (!initialize_event_subsystem()) {
        return false;
    }
    
    if (!initialize_message_subsystem()) {
        return false;
    }
    
    if (!initialize_shared_memory_subsystem()) {
        return false;
    }
    
    // Register the task cleanup handler with the scheduler
    register_task_cleanup_handler(cleanup_task_ipc);
    
    // Register the timeout checker with the scheduler
    register_timeout_checker(check_ipc_timeouts);
    
    kernel_printf("IPC subsystems initialized successfully\n");
    return true;
}

/*
 * Get the current IPC statistics
 */
void get_ipc_stats(ipc_stats_t* stats) {
    if (!stats) {
        return;
    }
    
    // Copy the current statistics
    *stats = ipc_stats;
}

/*
 * Print IPC statistics
 */
void print_ipc_stats(void) {
    kernel_printf("===== IPC SUBSYSTEM STATISTICS =====\n");
    kernel_printf("Total objects created: %llu\n", ipc_stats.ipc_objects_created);
    kernel_printf("Total objects destroyed: %llu\n", ipc_stats.ipc_objects_destroyed);
    kernel_printf("Current object counts:\n");
    kernel_printf("  Mutexes: %u\n", ipc_stats.mutex_count);
    kernel_printf("  Semaphores: %u\n", ipc_stats.semaphore_count);
    kernel_printf("  Events: %u\n", ipc_stats.event_count);
    kernel_printf("  Event Sets: %u\n", ipc_stats.event_set_count);
    kernel_printf("  Message Queues: %u\n", ipc_stats.message_queue_count);
    kernel_printf("  Shared Memory Regions: %u\n", ipc_stats.shared_memory_count);
    kernel_printf("Operation counts:\n");
    kernel_printf("  Mutex operations: %llu\n", ipc_stats.mutex_operations);
    kernel_printf("  Semaphore operations: %llu\n", ipc_stats.semaphore_operations);
    kernel_printf("  Event operations: %llu\n", ipc_stats.event_operations);
    kernel_printf("  Message operations: %llu\n", ipc_stats.message_operations);
    kernel_printf("  Shared memory operations: %llu\n", ipc_stats.shared_memory_operations);
    kernel_printf("Wait statistics:\n");
    kernel_printf("  Total wait time (ms): %llu\n", ipc_stats.total_wait_time);
    kernel_printf("  Active waiters: %u\n", ipc_stats.active_waiters);
    kernel_printf("  Timeouts: %u\n", ipc_stats.timeouts);
    kernel_printf("Error counts:\n");
    kernel_printf("  Allocation failures: %u\n", ipc_stats.allocation_failures);
    kernel_printf("  Permission failures: %u\n", ipc_stats.permission_failures);
    kernel_printf("  Timeout failures: %u\n", ipc_stats.timeout_failures);
    kernel_printf("===================================\n");
}

/*
 * Dump all IPC objects for debugging
 */
void dump_ipc_objects(void) {
    kernel_printf("===== IPC OBJECT DUMP =====\n");
    
    // Dump mutexes
    kernel_printf("--- MUTEXES ---\n");
    dump_all_mutexes();
    
    // Dump semaphores
    kernel_printf("--- SEMAPHORES ---\n");
    dump_all_semaphores();
    
    // Dump events
    kernel_printf("--- EVENTS ---\n");
    dump_all_events();
    
    // Dump message queues
    kernel_printf("--- MESSAGE QUEUES ---\n");
    dump_all_message_queues();
    
    // Dump shared memory regions
    kernel_printf("--- SHARED MEMORY REGIONS ---\n");
    dump_all_shared_memory_regions();
    
    kernel_printf("==========================\n");
}

/*
 * Check the health of the IPC subsystems
 */
bool check_ipc_health(void) {
    bool healthy = true;
    
    // Verify that all expected subsystems are initialized
    if (!mutex_initialized) {
        record_error("Mutex subsystem not initialized");
        healthy = false;
    }
    
    if (!semaphore_initialized) {
        record_error("Semaphore subsystem not initialized");
        healthy = false;
    }
    
    if (!event_initialized) {
        record_error("Event subsystem not initialized");
        healthy = false;
    }
    
    if (!message_initialized) {
        record_error("Message subsystem not initialized");
        healthy = false;
    }
    
    if (!shared_memory_initialized) {
        record_error("Shared memory subsystem not initialized");
        healthy = false;
    }
    
    // Check for resource leaks (objects created vs. destroyed)
    if (ipc_stats.ipc_objects_created > ipc_stats.ipc_objects_destroyed + 100) {
        record_error("Possible IPC resource leak detected: %llu created, %llu destroyed",
                    ipc_stats.ipc_objects_created, ipc_stats.ipc_objects_destroyed);
        healthy = false;
    }
    
    // Check for excessive errors
    if (ipc_stats.allocation_failures > 10) {
        record_error("Excessive allocation failures: %u", ipc_stats.allocation_failures);
        healthy = false;
    }
    
    return healthy;
}

/*
 * Get the last initialization error
 */
const char* get_last_ipc_error(void) {
    return last_error[0] ? last_error : "No error";
}

/*
 * Reset IPC statistics
 */
void reset_ipc_stats(void) {
    // Keep count information but reset operation stats
    ipc_stats.mutex_operations = 0;
    ipc_stats.semaphore_operations = 0;
    ipc_stats.event_operations = 0;
    ipc_stats.message_operations = 0;
    ipc_stats.shared_memory_operations = 0;
    
    ipc_stats.total_wait_time = 0;
    ipc_stats.timeouts = 0;
    
    ipc_stats.allocation_failures = 0;
    ipc_stats.permission_failures = 0;
    ipc_stats.timeout_failures = 0;
    
    kernel_printf("IPC statistics reset\n");
}

