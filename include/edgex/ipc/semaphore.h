/*
 * EdgeX OS - Semaphore Interface
 *
 * This file defines the interface for semaphores in EdgeX OS, which are used for
 * synchronization, resource counting, and coordination between tasks.
 *
 * Semaphores are synchronization primitives that maintain a count representing
 * the number of available resources or slots. They support two fundamental operations:
 *   - wait (decrement): Attempts to acquire a resource, blocking if none are available
 *   - post (increment): Releases a resource, potentially unblocking waiting tasks
 *
 * EdgeX semaphores can be used for various synchronization patterns:
 *   - Limiting access to a fixed number of resources (e.g., I/O channels)
 *   - Controlling task execution order (signaling)
 *   - Implementing producer-consumer coordination
 *   - Protecting critical sections with mutual exclusion (binary semaphores)
 *
 * Semaphores complement mutexes but provide more flexibility as they can count
 * beyond 1 and can be signaled from a different task than the one that will wait.
 */

#ifndef EDGEX_IPC_SEMAPHORE_H
#define EDGEX_IPC_SEMAPHORE_H

#include <edgex/ipc/common.h>

/* Semaphore structure (opaque)
 * This is an opaque handle to a semaphore object. The actual implementation
 * details are hidden to ensure type safety and proper resource management.
 * All operations on semaphores must use this handle.
 */
typedef struct semaphore* semaphore_t;

/* Semaphore functions */

/**
 * Create a new semaphore
 *
 * @param name           Name of the semaphore (for debugging and identification)
 * @param initial_value  Initial count of the semaphore (available resources)
 * 
 * @return Handle to the new semaphore or NULL on failure
 *
 * This function creates a new semaphore with the specified name and initial count.
 * The semaphore is owned by the creating task but can be accessed by other tasks
 * that have a reference to it.
 *
 * Common initial values:
 *   - 1: Binary semaphore (for mutual exclusion)
 *   - 0: Signaling semaphore (for task synchronization)
 *   - N: Resource-counting semaphore (for N available resources)
 *
 * Example (resource pool):
 *   // Create a semaphore to manage a pool of 5 database connections
 *   semaphore_t db_conn_sem = create_semaphore("db_connections", 5);
 *   if (db_conn_sem == NULL) {
 *       log_error("Failed to create database connection semaphore");
 *       return ERROR;
 *   }
 *
 * Example (binary semaphore):
 *   // Create a binary semaphore for mutual exclusion
 *   semaphore_t mutex_sem = create_semaphore("file_access", 1);
 */
semaphore_t create_semaphore(const char* name, uint32_t initial_value);

/**
 * Destroy a semaphore
 *
 * @param semaphore  Semaphore to destroy (void* for compatibility with destroy_fn)
 *
 * This function destroys a semaphore, freeing all associated resources.
 * If tasks are waiting on the semaphore, they will be unblocked with an error.
 * The semaphore handle becomes invalid after this call and should not be used.
 *
 * This function is normally called automatically through the IPC object system
 * when the last reference to a semaphore is removed, but can be called directly
 * when needed.
 *
 * Example:
 *   destroy_semaphore(my_semaphore);
 *   my_semaphore = NULL;  // Good practice to avoid dangling pointers
 */
void destroy_semaphore(void* semaphore);

/**
 * Wait on a semaphore (decrement count)
 *
 * @param semaphore  Semaphore to wait on
 *
 * @return 0 on success, negative error code on failure
 *
 * This function attempts to decrement the semaphore's count. If the count is
 * greater than zero, it decrements the count and returns immediately. If the
 * count is zero, the task blocks until the semaphore is posted or an error occurs.
 *
 * Common error codes:
 *   -EINVAL: Invalid semaphore handle
 *   -EINTR: Wait was interrupted by a signal
 *   -EDEADLK: Would cause a deadlock (detected by system)
 *   -EOWNERDEAD: Owner task died while holding the semaphore
 *
 * Example (resource allocation):
 *   // Acquire a database connection from the pool
 *   if (semaphore_wait(db_conn_sem) == 0) {
 *       // Successfully acquired a connection
 *       // Use the database...
 *       
 *       // Return the connection to the pool when done
 *       semaphore_post(db_conn_sem);
 *   }
 */
int semaphore_wait(semaphore_t semaphore);

/**
 * Try to wait on a semaphore without blocking
 *
 * @param semaphore  Semaphore to try to wait on
 *
 * @return 0 on success, -EAGAIN if would block, other negative error code on failure
 *
 * This function attempts to decrement the semaphore's count without blocking.
 * If the count is greater than zero, it decrements the count and returns success.
 * If the count is zero, it returns -EAGAIN immediately instead of blocking.
 *
 * This is useful for implementing non-blocking algorithms and avoiding deadlocks.
 *
 * Example (non-blocking resource acquisition):
 *   // Try to acquire a database connection, but don't block if none are available
 *   int result = semaphore_trywait(db_conn_sem);
 *   if (result == 0) {
 *       // Successfully acquired a connection
 *       process_database_query();
 *       semaphore_post(db_conn_sem);
 *   } else if (result == -EAGAIN) {
 *       // No connections available, handle gracefully
 *       log_info("No database connections available, trying later");
 *       queue_task_for_later();
 *   } else {
 *       // Handle other errors
 *       log_error("Semaphore error: %d", result);
 *   }
 */
int semaphore_trywait(semaphore_t semaphore);

/**
 * Post to a semaphore (increment count)
 *
 * @param semaphore  Semaphore to post to
 *
 * @return 0 on success, negative error code on failure
 *
 * This function increments the semaphore's count. If tasks are blocked waiting
 * on the semaphore, one of them will be unblocked and allowed to proceed.
 *
 * The system ensures fair scheduling of waiting tasks, typically using a FIFO order.
 *
 * Example (producer-consumer):
 *   // In producer task - Signal that a new item is available
 *   add_item_to_queue(item);
 *   semaphore_post(items_available_sem);
 *
 *   // In consumer task - Wait for an item to become available
 *   semaphore_wait(items_available_sem);
 *   item_t item = get_item_from_queue();
 */
int semaphore_post(semaphore_t semaphore);

/**
 * Get the current value of a semaphore
 *
 * @param semaphore  Semaphore to query
 * @param value      Pointer to store the current count
 *
 * @return 0 on success, negative error code on failure
 *
 * This function retrieves the current count of the semaphore without modifying it.
 * Note that the value may change immediately after this call returns due to
 * concurrent operations by other tasks.
 *
 * This is primarily useful for debugging and monitoring, not for synchronization logic.
 *
 * Example (monitoring):
 *   int available_connections;
 *   if (semaphore_getvalue(db_conn_sem, &available_connections) == 0) {
 *       log_debug("Database connections available: %d", available_connections);
 *       if (available_connections < 2) {
 *           log_warning("Database connection pool running low");
 *       }
 *   }
 */
int semaphore_getvalue(semaphore_t semaphore, int* value);

/* Internal functions */

/**
 * Initialize the semaphore subsystem
 *
 * This function initializes the semaphore management subsystem.
 * It is called automatically during system startup and should not be
 * called directly by application code.
 */
void init_semaphore_subsystem(void);

/**
 * Clean up semaphore resources for a terminated task
 *
 * @param pid  PID of the terminated task
 *
 * This function is called by the task scheduler when a task terminates.
 * It cleans up all semaphores owned by the task and unblocks any tasks
 * waiting on those semaphores with an appropriate error code.
 *
 * Application code should not call this function directly.
 */
void cleanup_task_semaphores(pid_t pid);

/**
 * Dump information about all semaphores to the system log
 *
 * This function prints detailed information about all existing semaphores
 * to the system log, including their name, current value, waiting tasks,
 * and owner. It's useful for debugging deadlocks and resource leaks.
 *
 * Example:
 *   // When debugging a potential deadlock
 *   dump_all_semaphores();
 */
void dump_all_semaphores(void);

/*
 * Semaphore Usage Patterns
 * -----------------------
 *
 * 1. Binary Semaphore (Mutex-like)
 *    Used for mutual exclusion to protect critical sections:
 *
 *    semaphore_t lock = create_semaphore("critical_section", 1);
 *    
 *    // To enter critical section:
 *    semaphore_wait(lock);
 *    // ... critical section code ...
 *    semaphore_post(lock);
 *
 * 2. Signaling (Task Synchronization)
 *    Used to signal from one task to another:
 *
 *    semaphore_t ready = create_semaphore("ready_signal", 0);
 *    
 *    // In task 1:
 *    prepare_data();
 *    semaphore_post(ready);
 *    
 *    // In task 2:
 *    semaphore_wait(ready);
 *    process_data();
 *
 * 3. Resource Counting
 *    Used to manage a fixed pool of resources:
 *
 *    semaphore_t pool = create_semaphore("resource_pool", MAX_RESOURCES);
 *    
 *    // To use a resource:
 *    semaphore_wait(pool);
 *    resource_t* res = allocate_resource();
 *    use_resource(res);
 *    free_resource(res);
 *    semaphore_post(pool);
 *
 * 4. Producer-Consumer Queue
 *    Used to synchronize producers and consumers:
 *
 *    semaphore_t items = create_semaphore("items_count", 0);
 *    semaphore_t slots = create_semaphore("free_slots", QUEUE_SIZE);
 *    semaphore_t mutex = create_semaphore("queue_mutex", 1);
 *    
 *    // Producer:
 *    semaphore_wait(slots);     // Wait for a free slot
 *    semaphore_wait(mutex);     // Lock the queue
 *    add_to_queue(item);
 *    semaphore_post(mutex);     // Unlock the queue
 *    semaphore_post(items);     // Signal an item is available
 *    
 *    // Consumer:
 *    semaphore_wait(items);     // Wait for an available item
 *    semaphore_wait(mutex);     // Lock the queue
 *    item = remove_from_queue();
 *    semaphore_post(mutex);     // Unlock the queue
 *    semaphore_post(slots);     // Signal a slot is free
 */

#endif /* EDGEX_IPC_SEMAPHORE_H */

