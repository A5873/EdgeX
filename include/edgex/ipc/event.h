/*
 * EdgeX OS - Event Interface
 *
 * This file defines the interface for events in EdgeX OS, which provide a mechanism
 * for task notification and condition synchronization.
 *
 * Events are synchronization primitives that allow tasks to signal conditions 
 * and wait for notifications from other tasks. Unlike semaphores, events focus on 
 * condition signaling rather than resource counting.
 *
 * EdgeX provides two types of event-based synchronization:
 *   1. Individual Events: Binary flags that are either set or reset
 *   2. Event Sets: Collections of events that allow a task to wait for any one
 *      of multiple conditions to occur
 *
 * Events are particularly useful for:
 *   - Waiting for external conditions (I/O completion, timer expiration)
 *   - Task notification (waking a task when work is available)
 *   - Implementing state machines and condition variables
 *   - Handling asynchronous operations
 */

#ifndef EDGEX_IPC_EVENT_H
#define EDGEX_IPC_EVENT_H

#include <edgex/ipc/common.h>

/* Event handle (opaque)
 * This is an opaque handle to an event object. The actual implementation
 * details are hidden to ensure type safety and proper resource management.
 * All operations on events must use this handle.
 */
typedef struct event* event_t;

/* Event functions */

/**
 * Create a new event
 *
 * @param name  Name of the event (for debugging and identification)
 * 
 * @return Handle to the new event or NULL on failure
 *
 * This function creates a new event with the specified name. The event
 * is initially in the "reset" (non-signaled) state. The event is owned
 * by the creating task but can be accessed by other tasks that have
 * a reference to it.
 *
 * Example:
 *   // Create an event for data ready notification
 *   event_t data_ready = create_event("data_ready_event");
 *   if (data_ready == NULL) {
 *       log_error("Failed to create data ready event");
 *       return ERROR;
 *   }
 */
event_t create_event(const char* name);

/**
 * Destroy an event
 *
 * @param event  Event to destroy
 *
 * This function destroys an event, freeing all associated resources.
 * If tasks are waiting on the event, they will be unblocked with
 * an appropriate error code. The event handle becomes invalid after
 * this call and should not be used again.
 *
 * Example:
 *   destroy_event(data_ready);
 *   data_ready = NULL;  // Good practice to avoid dangling pointers
 */
void destroy_event(event_t event);

/**
 * Wait for an event to be signaled
 *
 * @param event  Event to wait on
 *
 * @return 0 on success, negative error code on failure
 *
 * This function blocks the calling task until the specified event is
 * signaled by another task using event_signal() or event_broadcast().
 * If the event is already in the signaled state, the function returns
 * immediately.
 *
 * Common error codes:
 *   -EINVAL: Invalid event handle
 *   -EINTR: Wait was interrupted by a signal
 *   -EDEADLK: Would cause a deadlock (detected by system)
 *
 * Example:
 *   // Wait for data to be ready
 *   if (event_wait(data_ready) == 0) {
 *       // Data is ready, process it
 *       process_data();
 *   }
 */
int event_wait(event_t event);

/**
 * Wait for an event with a timeout
 *
 * @param event       Event to wait on
 * @param timeout_ms  Timeout in milliseconds (0 = immediate, UINT64_MAX = infinite)
 *
 * @return 0 on success, -ETIMEDOUT on timeout, other negative error code on failure
 *
 * This function is similar to event_wait() but allows specifying a maximum
 * time to wait. If the event is not signaled within the timeout period,
 * the function returns -ETIMEDOUT.
 *
 * Example:
 *   // Wait up to 1 second for data to be ready
 *   int result = event_timedwait(data_ready, 1000);
 *   if (result == 0) {
 *       // Data is ready, process it
 *       process_data();
 *   } else if (result == -ETIMEDOUT) {
 *       // Timeout occurred, handle accordingly
 *       log_warning("Timeout waiting for data");
 *       handle_timeout();
 *   } else {
 *       // Handle other errors
 *       log_error("Event wait error: %d", result);
 *   }
 */
int event_timedwait(event_t event, uint64_t timeout_ms);

/**
 * Signal an event (wake one waiting task)
 *
 * @param event  Event to signal
 *
 * @return 0 on success, negative error code on failure
 *
 * This function signals the event, waking up one task that is waiting
 * on the event (if any). If multiple tasks are waiting, only one is
 * unblocked (typically the highest priority task or the first in the queue).
 * The event remains in the signaled state after this call.
 *
 * Example:
 *   // Signal that data is ready
 *   prepare_data();
 *   event_signal(data_ready);
 */
int event_signal(event_t event);

/**
 * Broadcast an event (wake all waiting tasks)
 *
 * @param event  Event to broadcast
 *
 * @return 0 on success, negative error code on failure
 *
 * This function signals the event, waking up all tasks that are waiting
 * on the event. The event remains in the signaled state after this call.
 *
 * Example:
 *   // Signal all waiting tasks that system is shutting down
 *   event_broadcast(shutdown_event);
 */
int event_broadcast(event_t event);

/**
 * Reset an event to non-signaled state
 *
 * @param event  Event to reset
 *
 * @return 0 on success, negative error code on failure
 *
 * This function resets the event to the non-signaled state. Tasks that
 * subsequently wait on the event will block until it is signaled again.
 *
 * Example:
 *   // Reset the data ready event after processing
 *   process_data();
 *   event_reset(data_ready);  // Ready for next notification
 */
int event_reset(event_t event);

/* Event set (for waiting on multiple events)
 * Event sets allow a task to wait for any one of multiple events to be signaled.
 * This is similar to select() or poll() in traditional POSIX systems.
 */
typedef struct event_set* event_set_t;

/* Event set functions */

/**
 * Create a new event set
 *
 * @param name        Name of the event set (for debugging and identification)
 * @param max_events  Maximum number of events that can be added to the set
 * 
 * @return Handle to the new event set or NULL on failure
 *
 * This function creates a new event set that can hold up to max_events
 * individual events. The event set is owned by the creating task but
 * can be accessed by other tasks that have a reference to it.
 *
 * Example:
 *   // Create an event set for monitoring multiple I/O conditions
 *   event_set_t io_events = create_event_set("io_monitor", 5);
 *   if (io_events == NULL) {
 *       log_error("Failed to create I/O event set");
 *       return ERROR;
 *   }
 */
event_set_t create_event_set(const char* name, uint32_t max_events);

/**
 * Destroy an event set
 *
 * @param event_set  Event set to destroy
 *
 * This function destroys an event set, freeing all associated resources.
 * If tasks are waiting on the event set, they will be unblocked with
 * an appropriate error code. The event set handle becomes invalid after
 * this call and should not be used again.
 *
 * Note that the individual events added to the set are not destroyed.
 *
 * Example:
 *   destroy_event_set(io_events);
 *   io_events = NULL;  // Good practice to avoid dangling pointers
 */
void destroy_event_set(event_set_t event_set);

/**
 * Add an event to an event set
 *
 * @param event_set  Event set to add the event to
 * @param event      Event to add
 *
 * @return 0 on success, negative error code on failure
 *
 * This function adds an event to an event set. The event can still be
 * used independently, and any changes to its state will be reflected
 * in the event set.
 *
 * Common error codes:
 *   -EINVAL: Invalid parameters
 *   -ENOSPC: Event set is full (reached max_events)
 *   -EEXIST: Event is already in the set
 *
 * Example:
 *   // Add various I/O events to the set
 *   event_set_add(io_events, network_data_event);
 *   event_set_add(io_events, disk_complete_event);
 *   event_set_add(io_events, user_input_event);
 */
int event_set_add(event_set_t event_set, event_t event);

/**
 * Remove an event from an event set
 *
 * @param event_set  Event set to remove the event from
 * @param event      Event to remove
 *
 * @return 0 on success, negative error code on failure
 *
 * This function removes an event from an event set. The event itself
 * is not destroyed and can still be used independently.
 *
 * Common error codes:
 *   -EINVAL: Invalid parameters
 *   -ENOENT: Event is not in the set
 *
 * Example:
 *   // Remove an event from the set when no longer needed
 *   event_set_remove(io_events, user_input_event);
 */
int event_set_remove(event_set_t event_set, event_t event);

/**
 * Wait for any event in the set to be signaled
 *
 * @param event_set       Event set to wait on
 * @param signaled_event  Pointer to store the event that was signaled
 *
 * @return 0 on success, negative error code on failure
 *
 * This function blocks the calling task until any one of the events
 * in the set is signaled. When an event is signaled, the function
 * returns and stores the signaled event in the signaled_event parameter.
 *
 * If multiple events are already signaled, one is chosen (typically
 * based on the order they were added to the set).
 *
 * Common error codes:
 *   -EINVAL: Invalid parameters
 *   -EINTR: Wait was interrupted by a signal
 *   -EDEADLK: Would cause a deadlock (detected by system)
 *
 * Example:
 *   // Wait for any I/O event to occur
 *   event_t triggered;
 *   if (event_set_wait(io_events, &triggered) == 0) {
 *       if (triggered == network_data_event) {
 *           handle_network_data();
 *       } else if (triggered == disk_complete_event) {
 *           handle_disk_operation();
 *       }
 *   }
 */
int event_set_wait(event_set_t event_set, event_t* signaled_event);

/**
 * Wait for any event in the set with a timeout
 *
 * @param event_set       Event set to wait on
 * @param signaled_event  Pointer to store the event that was signaled
 * @param timeout_ms      Timeout in milliseconds (0 = immediate, UINT64_MAX = infinite)
 *
 * @return 0 on success, -ETIMEDOUT on timeout, other negative error code on failure
 *
 * This function is similar to event_set_wait() but allows specifying a maximum
 * time to wait. If no event is signaled within the timeout period, the function
 * returns -ETIMEDOUT and signaled_event is not modified.
 *
 * Example:
 *   // Wait up to 5 seconds for any I/O event
 *   event_t triggered;
 *   int result = event_set_timedwait(io_events, &triggered, 5000);
 *   if (result == 0) {
 *       // An event was triggered, handle it
 *       if (triggered == network_data_event) {
 *           handle_network_data();
 *       } else if (triggered == disk_complete_event) {
 *           handle_disk_operation();
 *       }
*   } else if (result == -ETIMEDOUT) {
*       // Timeout occurred, no event was triggered
*       log_info("No I/O events occurred within timeout");
*       check_system_status();
*   } else {
*     /* Internal functions */

/**
 * Initialize the event subsystem
 *
 * This function initializes the event management subsystem.
 * It is called automatically during system startup and should not be
 * called directly by application code.
 */
void init_event_subsystem(void);

/**
 * Clean up event resources for a terminated task
 *
 * @param pid  PID of the terminated task
 *
 * This function is called by the task scheduler when a task terminates.
 * It cleans up all events owned by the task and unblocks any tasks
 * waiting on those events with an appropriate error code.
 *
 * Application code should not call this function directly.
 */
void cleanup_task_events(pid_t pid);

/**
 * Check for and handle event timeouts
 *
 * This function is called periodically by the system timer to check
 * for timed wait operations that have expired, and to wake up tasks
 * that have been waiting longer than their specified timeout.
 *
 * Application code should not call this function directly.
 */
void check_event_timeouts(void);

/**
 * Dump information about all events to the system log
 *
 * This function prints detailed information about all existing events
 * to the system log, including their name, current state, waiting tasks,
 * and owner. It's useful for debugging synchronization issues.
 *
 * Example:
 *   // When debugging a synchronization problem
 *   dump_all_events();
 */
void dump_all_events(void);

/*
 * Event Usage Patterns
 * -----------------------
 *
 * 1. Task Notification
 *    Used to notify one task when another has completed an operation:
 *
 *    event_t work_complete = create_event("work_complete");
 *    
 *    // In worker task:
 *    perform_work();
 *    event_signal(work_complete);
 *    
 *    // In waiting task:
 *    event_wait(work_complete);
 *    process_results();
 *    event_reset(work_complete);  // Reset for next notification
 *
 * 2. Periodic Wake-up with Timeout
 *    Used to implement periodic operations with the ability to wake early:
 *
 *    event_t wake_event = create_event("periodic_wake");
 *    
 *    while (running) {
 *        // Wait up to 1 second, but can be woken earlier
 *        int result = event_timedwait(wake_event, 1000);
 *        
 *        if (result == 0) {
 *            // Explicit wake-up occurred
 *            handle_early_wakeup();
 *            event_reset(wake_event);
 *        } else if (result == -ETIMEDOUT) {
 *            // Normal timeout, perform periodic work
 *            perform_periodic_work();
 *        }
 *    }
 *
 * 3. State Machine Transitions
 *    Used to implement state machines with events for each state:
 *
 *    event_t state_idle = create_event("state_idle");
 *    event_t state_running = create_event("state_running");
 *    event_t state_error = create_event("state_error");
 *    
 *    event_set_t state_changes = create_event_set("state_machine", 3);
 *    event_set_add(state_changes, state_idle);
 *    event_set_add(state_changes, state_running);
 *    event_set_add(state_changes, state_error);
 *    
 *    // Signal initial state
 *    event_signal(state_idle);
 *    
 *    while (running) {
 *        event_t new_state;
 *        event_set_wait(state_changes, &new_state);
 *        
 *        if (new_state == state_idle) {
 *            handle_idle_state();
 *            event_reset(state_idle);
 *        } else if (new_state == state_running) {
 *            handle_running_state();
 *            event_reset(state_running);
 *        } else if (new_state == state_error) {
 *            handle_error_state();
 *            event_reset(state_error);
 *        }
 *    }
 *
 * 4. I/O Completion Notification
 *    Used to handle multiple I/O operations efficiently:
 *
 *    event_t network_event = create_event("network_ready");
 *    event_t disk_event = create_event("disk_ready");
 *    event_t user_event = create_event("user_input");
 *    
 *    event_set_t io_events = create_event_set("io_monitor", 3);
 *    event_set_add(io_events, network_event);
 *    event_set_add(io_events, disk_event);
 *    event_set_add(io_events, user_event);
 *    
 *    // Register events with I/O subsystems (simplified)
 *    register_network_callback(network_event);
 *    register_disk_callback(disk_event);
 *    register_input_callback(user_event);
 *    
 *    while (running) {
 *        event_t triggered;
 *        if (event_set_wait(io_events, &triggered) == 0) {
 *            if (triggered == network_event) {
 *                process_network_data();
 *                event_reset(network_event);
 *            } else if (triggered == disk_event) {
 *                process_disk_data();
 *                event_reset(disk_event);
 *            } else if (triggered == user_event) {
 *                process_user_input();
 *                event_reset(user_event);
 *            }
 *        }
 *    }
 *
 * 5. One-time Initialization
 *    Used to ensure a task-safe one-time initialization:
 *
 *    event_t init_complete = create_event("init_complete");
 *    
 *    void ensure_initialized(void) {
 *        static bool initializing = false;
 *        
 *        // Check if already initialized
 *        if (is_event_signaled(init_complete)) {
 *            return;  // Already initialized
 *        }
 *        
 *        // First task to reach this point will initialize
 *        if (!atomic_exchange(&initializing, true)) {
 *            perform_initialization();
 *            event_signal(init_complete);
 *        } else {
 *            // Other tasks wait for initialization to complete
 *            event_wait(init_complete);
 *        }
 *    }
 */

#endif /* EDGEX_IPC_EVENT_H */

