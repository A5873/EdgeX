/*
 * EdgeX OS - Mutex Interface
 *
 * This file defines the interface for mutual exclusion locks (mutexes) in EdgeX OS,
 * which provide exclusive access to shared resources, preventing race conditions
 * and ensuring data integrity in concurrent environments.
 *
 * Mutexes are the simplest form of synchronization primitive, enforcing that only
 * one task can access a protected resource at any given time. Key characteristics
 * of EdgeX mutexes include:
 *
 *   - Ownership: Each mutex is owned by the task that successfully locks it
 *   - Recursive locking: Not supported (a task cannot lock a mutex it already owns)
 *   - Priority inheritance: Supported to prevent priority inversion problems
 *   - Deadlock detection: The system can detect and report potential deadlocks
 *
 * Proper mutex usage is critical in multitasking systems to prevent:
 *   - Race conditions: Unpredictable results from concurrent access to shared data
 *   - Deadlocks: Tasks permanently blocked waiting for resources held by each other
 *   - Priority inversion: Lower-priority tasks preventing higher-priority tasks from running
 *
 * EdgeX mutexes are designed to be efficient, with minimal overhead for
 * uncontended locks and fair scheduling when multiple tasks compete for access.
 */

#ifndef EDGEX_IPC_MUTEX_H
#define EDGEX_IPC_MUTEX_H

#include <edgex/ipc/common.h>

/* Mutex structure (opaque)
 * This is an opaque handle to a mutex object. The actual implementation
 * details are hidden to ensure type safety and proper resource management.
 * All operations on mutexes must use this handle.
 */
typedef struct mutex* mutex_t;

/* Mutex functions */

/**
 * Create a new mutex
 *
 * @param name  Name of the mutex (for debugging and identification)
 * 
 * @return Handle to the new mutex or NULL on failure
 *
 * This function creates a new mutex with the specified name. The mutex
 * is initially unlocked and owned by the creating task in the system registry.
 * Other tasks can access the mutex if they have a reference to it.
 *
 * The name parameter is primarily for debugging purposes, making it easier
 * to identify mutexes when troubleshooting synchronization issues.
 *
 * Example:
 *   // Create a mutex to protect access to a shared resource
 *   mutex_t data_lock = create_mutex("data_protection_lock");
 *   if (data_lock == NULL) {
 *       log_error("Failed to create mutex for data protection");
 *       return ERROR;
 *   }
 */
mutex_t create_mutex(const char* name);

/**
 * Destroy a mutex
 *
 * @param mutex  Mutex to destroy (void* for compatibility with destroy_fn)
 *
 * This function destroys a mutex, freeing all associated resources.
 * If the mutex is currently locked, the behavior depends on whether
 * the calling task is the owner:
 *   - If called by the owner: mutex is unlocked and destroyed
 *   - If called by another task: returns an error and doesn't destroy
 *
 * If tasks are waiting on the mutex, they will be unblocked with an
 * appropriate error code. The mutex handle becomes invalid after this
 * call and should not be used again.
 *
 * Example:
 *   destroy_mutex(data_lock);
 *   data_lock = NULL;  // Good practice to avoid dangling pointers
 */
void destroy_mutex(void* mutex);

/**
 * Lock a mutex
 *
 * @param mutex  Mutex to lock
 *
 * @return 0 on success, negative error code on failure
 *
 * This function attempts to acquire the mutex. If the mutex is currently
 * unlocked, it is locked and ownership is assigned to the calling task.
 * If the mutex is already locked by another task, the calling task blocks
 * until the mutex becomes available or an error occurs.
 *
 * The EdgeX mutex implementation includes priority inheritance, which means
 * that if a high-priority task blocks waiting for a mutex held by a lower-priority
 * task, the lower-priority task's priority is temporarily boosted to prevent
 * priority inversion.
 *
 * Common error codes:
 *   -EINVAL: Invalid mutex handle
 *   -EINTR: Wait was interrupted by a signal
 *   -EDEADLK: Would cause a deadlock (detected by system)
 *   -EOWNERDEAD: Owner task died while holding the mutex
 *
 * Example:
 *   // Protect access to shared data
 *   if (mutex_lock(data_lock) == 0) {
 *       // Critical section - only one task can execute this at a time
 *       modify_shared_data();
 *       
 *       // Always unlock when done
 *       mutex_unlock(data_lock);
 *   } else {
 *       log_error("Failed to acquire data lock");
 *   }
 */
int mutex_lock(mutex_t mutex);

/**
 * Try to lock a mutex without blocking
 *
 * @param mutex  Mutex to try to lock
 *
 * @return 0 on success, -EBUSY if the mutex is already locked, other negative error code on failure
 *
 * This function attempts to acquire the mutex without blocking. If the mutex
 * is currently unlocked, it is locked and ownership is assigned to the calling task.
 * If the mutex is already locked by another task, the function returns -EBUSY immediately
 * instead of blocking.
 *
 * This is useful for implementing non-blocking algorithms and avoiding deadlocks.
 *
 * Example:
 *   // Try to acquire the lock, but don't block if it's not available
 *   int result = mutex_trylock(data_lock);
 *   if (result == 0) {
 *       // Successfully acquired the lock
 *       process_shared_data();
 *       mutex_unlock(data_lock);
 *   } else if (result == -EBUSY) {
 *       // Mutex is currently locked by another task
 *       log_info("Data is currently being modified by another task, will try later");
 *       schedule_retry();
 *   } else {
 *       // Handle other errors
 *       log_error("Mutex error: %d", result);
 *   }
 */
int mutex_trylock(mutex_t mutex);

/**
 * Unlock a mutex
 *
 * @param mutex  Mutex to unlock
 *
 * @return 0 on success, negative error code on failure
 *
 * This function releases the mutex, allowing other tasks to acquire it.
 * The mutex must be currently locked by the calling task; attempting to
 * unlock a mutex not owned by the caller will result in an error.
 *
 * If tasks are waiting to acquire the mutex, one of them will be unblocked
 * and granted ownership. The system ensures fair scheduling of waiting tasks,
 * typically selecting the highest-priority waiting task or using FIFO order
 * for tasks of equal priority.
 *
 * Common error codes:
 *   -EINVAL: Invalid mutex handle
 *   -EPERM: Attempted to unlock a mutex not owned by the caller
 *
 * Example:
 *   // Always pair lock and unlock operations
 *   if (mutex_lock(data_lock) == 0) {
 *       // Critical section
 *       modify_shared_data();
 *       
 *       // Release the lock when done
 *       mutex_unlock(data_lock);
 *   }
 */
int mutex_unlock(mutex_t mutex);

/* Internal functions */

/**
 * Initialize the mutex subsystem
 *
 * This function initializes the mutex management subsystem.
 * It is called automatically during system startup and should not be
 * called directly by application code.
 */
void init_mutex_subsystem(void);

/**
 * Clean up mutex resources for a terminated task
 *
 * @param pid  PID of the terminated task
 *
 * This function is called by the task scheduler when a task terminates.
 * It cleans up all mutexes owned by the task and unblocks any tasks
 * waiting on those mutexes with an appropriate error code.
 *
 * Application code should not call this function directly.
 */
void cleanup_task_mutexes(pid_t pid);

/**
 * Dump information about all mutexes to the system log
 *
 * This function prints detailed information about all existing mutexes
 * to the system log, including their name, owner, and waiting tasks.
 * It's useful for debugging deadlocks and synchronization issues.
 *
 * Example:
 *   // When debugging a suspected deadlock
 *   dump_all_mutexes();
 */
void dump_all_mutexes(void);

/*
 * Mutex Usage Patterns and Best Practices
 * --------------------------------------
 *
 * 1. Basic Resource Protection
 *    The simplest and most common pattern for mutex usage:
 *
 *    mutex_t resource_lock = create_mutex("resource_lock");
 *    
 *    void access_resource(void) {
 *        mutex_lock(resource_lock);
 *        // Critical section - access protected resource
 *        modify_shared_data();
 *        mutex_unlock(resource_lock);
 *    }
 *
 * 2. Lock Guard / RAII Pattern (using goto for cleanup)
 *    Ensures mutex is always unlocked, even when errors occur:
 *
 *    int process_data(void) {
 *        mutex_lock(data_lock);
 *        
 *        if (validate_data() != SUCCESS) {
 *            goto cleanup;  // Error case
 *        }
 *        
 *        if (process_step_one() != SUCCESS) {
 *            goto cleanup;  // Error case
 *        }
 *        
 *        if (process_step_two() != SUCCESS) {
 *            goto cleanup;  // Error case
 *        }
 *        
 *        // Success case
 *        int result = SUCCESS;
 *        
 *    cleanup:
 *        mutex_unlock(data_lock);
 *        return result;
 *    }
 *
 * 3. Try-Lock Pattern for Deadlock Avoidance
 *    Useful when needing to acquire multiple locks:
 *
 *    bool transfer_data(source_t* src, dest_t* dst) {
 *        // Try to acquire first lock
 *        if (mutex_trylock(src->lock) != 0) {
 *            return false;  // Cannot get source lock, try again later
 *        }
 *        
 *        // Try to acquire second lock
 *        if (mutex_trylock(dst->lock) != 0) {
 *            // Cannot get destination lock, release first and try again later
 *            mutex_unlock(src->lock);
 *            return false;
 *        }
 *        
 *        // Have both locks, perform transfer
 *        copy_data(src, dst);
 *        
 *        // Release locks in reverse order
 *        mutex_unlock(dst->lock);
 *        mutex_unlock(src->lock);
 *        return true;
 *    }
 *
 * 4. Consistent Lock Ordering to Prevent Deadlocks
 *    Always acquire multiple locks in the same order:
 *
 *    // Bad: Task 1 gets lockA then lockB, Task 2 gets lockB then lockA
 *    // This can deadlock!
 *    
 *    // Good: Both tasks get locks in the same order (lockA then lockB)
 *    void safe_operation(void) {
 *        mutex_lock(lockA);  // Always acquire lockA first
 *        mutex_lock(lockB);  // Then acquire lockB
 *        
 *        // Critical section
 *        
 *        mutex_unlock(lockB);  // Release in reverse order
 *        mutex_unlock(lockA);
 *    }
 *
 * 5. Timeout-based Lock Acquisition
 *    Combined with events for more complex synchronization:
 *
 *    bool timed_operation(int timeout_ms) {
 *        // Try to acquire the lock without blocking
 *        if (mutex_trylock(resource_lock) == 0) {
 *            // Got the lock immediately
 *            perform_operation();
 *            mutex_unlock(resource_lock);
 *            return true;
 *        }
 *        
 *        // Create an event for notification
 *        event_t resource_available = create_event("resource_available");
 *        
 *        // Register for notification when the resource becomes available
 *        register_mutex_notification(resource_lock, resource_available);
 *        
 *        // Wait with timeout
 *        int result = event_timedwait(resource_available, timeout_ms);
 *        
 *        // Clean up event
 *        destroy_event(resource_available);
 *        
 *        if (result == 0) {
 *            // Resource might be available, try to acquire
 *            if (mutex_trylock(resource_lock) == 0) {
 *                perform_operation();
 *                mutex_unlock(resource_lock);
 *                return true;
 *            }
 *        }
 *        
 *        return false;  // Could not acquire within timeout
 *    }
 *
 * Common Pitfalls to Avoid:
 * -------------------------
 * 1. Forgetting to release a mutex
 * 2. Using mutexes in interrupt handlers (use spinlocks instead)
 * 3. Holding locks for too long, especially during I/O operations
 * 4. Recursive locking (not supported in EdgeX mutexes)
 * 5. Inconsistent lock ordering leading to deadlocks
 * 6. Releasing a mutex not owned by the current task
 */

#endif /* EDGEX_IPC_MUTEX_H */

