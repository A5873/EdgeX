/*
 * EdgeX OS - Event Notification System
 *
 * This file implements the event notification system for EdgeX OS,
 * including events and event sets.
 */

#include <edgex/kernel.h>
#include <edgex/memory.h>
#include <edgex/scheduler.h>
#include <edgex/ipc.h>

/* Maximum number of events in the system */
#define MAX_EVENTS 64

/* Maximum number of event sets in the system */
#define MAX_EVENT_SETS 32

/* Maximum number of events in a single event set */
#define MAX_EVENTS_PER_SET 16

/* Event flags */
#define EVENT_FLAG_MANUAL_RESET  0x01  /* Event doesn't auto-reset after signal */
#define EVENT_FLAG_SIGNALED      0x02  /* Event is currently signaled */

/* Event state */
typedef enum {
    EVENT_STATE_NONSIGNALED = 0,
    EVENT_STATE_SIGNALED = 1
} event_state_t;

/* Event waiter structure */
typedef struct event_waiter {
    pid_t pid;                       /* PID of waiting task */
    uint64_t wait_start_time;        /* When the wait started */
    uint64_t timeout;                /* Timeout in ticks (0 = forever) */
    struct event_waiter* next;       /* Next waiter in list */
} event_waiter_t;

/* Event structure */
struct event {
    ipc_object_header_t header;      /* Common IPC header */
    uint32_t flags;                  /* Event flags */
    event_state_t state;             /* Current state */
    mutex_t mutex;                   /* Synchronization mutex */
    event_waiter_t* waiters;         /* List of waiting tasks */
    uint32_t waiter_count;           /* Number of waiting tasks */
};

/* Event set structure */
struct event_set {
    ipc_object_header_t header;      /* Common IPC header */
    mutex_t mutex;                   /* Synchronization mutex */
    uint32_t event_count;            /* Number of events in the set */
    event_t events[MAX_EVENTS_PER_SET]; /* Array of events */
    event_waiter_t* waiters;         /* List of waiting tasks */
    uint32_t waiter_count;           /* Number of waiting tasks */
};

/* Pool of all events */
static struct event event_pool[MAX_EVENTS];
static uint32_t event_count = 0;

/* Pool of all event sets */
static struct event_set event_set_pool[MAX_EVENT_SETS];
static uint32_t event_set_count = 0;

/* Forward declarations */
static void destroy_event(void* event_ptr);
static void destroy_event_set(void* event_set_ptr);
static void add_event_waiter(event_t event, pid_t pid, uint64_t timeout);
static void remove_event_waiter(event_t event, pid_t pid);
static void wake_event_waiters(event_t event, uint32_t count);
static void add_event_set_waiter(event_set_t event_set, pid_t pid, uint64_t timeout);
static void remove_event_set_waiter(event_set_t event_set, pid_t pid);
static void wake_event_set_waiters(event_set_t event_set, event_t signaled_event, uint32_t count);

/*
 * Find a free event slot
 */
static event_t find_free_event(void) {
    for (uint32_t i = 0; i < MAX_EVENTS; i++) {
        if (event_pool[i].header.type == 0) {
            return &event_pool[i];
        }
    }
    return NULL;
}

/*
 * Find a free event set slot
 */
static event_set_t find_free_event_set(void) {
    for (uint32_t i = 0; i < MAX_EVENT_SETS; i++) {
        if (event_set_pool[i].header.type == 0) {
            return &event_set_pool[i];
        }
    }
    return NULL;
}

/*
 * Create an event
 */
event_t create_event(const char* name) {
    event_t event = find_free_event();
    if (!event) {
        kernel_printf("Failed to allocate event: no free slots\n");
        return NULL;
    }
    
    // Initialize event
    event->header.type = IPC_TYPE_EVENT;
    strncpy(event->header.name, name, MAX_IPC_NAME_LENGTH - 1);
    event->header.name[MAX_IPC_NAME_LENGTH - 1] = '\0';
    event->header.ref_count = 1;
    event->header.destroy_fn = destroy_event;
    
    // Create synchronization mutex
    char mutex_name[MAX_IPC_NAME_LENGTH];
    snprintf(mutex_name, MAX_IPC_NAME_LENGTH, "%s_mutex", name);
    event->mutex = create_mutex(mutex_name);
    
    if (!event->mutex) {
        memset(event, 0, sizeof(struct event));
        return NULL;
    }
    
    event->flags = 0;  // Auto-reset by default
    event->state = EVENT_STATE_NONSIGNALED;
    event->waiters = NULL;
    event->waiter_count = 0;
    
    event_count++;
    return event;
}

/*
 * Destroy an event
 */
static void destroy_event(void* event_ptr) {
    event_t event = (event_t)event_ptr;
    
    if (!event || event->header.type != IPC_TYPE_EVENT) {
        return;
    }
    
    // Lock the event
    mutex_lock(event->mutex);
    
    // Wake up all waiters
    wake_event_waiters(event, 0);
    
    // Unlock and destroy mutex
    mutex_unlock(event->mutex);
    destroy_mutex(event->mutex);
    
    // Clear the event
    memset(event, 0, sizeof(struct event));
    
    event_count--;
}

/*
 * Destroy an event (public API)
 */
void destroy_event(event_t event) {
    if (!event || event->header.type != IPC_TYPE_EVENT) {
        return;
    }
    
    destroy_event((void*)event);
}

/*
 * Add a waiter to an event
 */
static void add_event_waiter(event_t event, pid_t pid, uint64_t timeout) {
    event_waiter_t* waiter = (event_waiter_t*)kmalloc(sizeof(event_waiter_t));
    if (!waiter) {
        kernel_printf("Failed to allocate event waiter\n");
        return;
    }
    
    // Initialize waiter
    waiter->pid = pid;
    waiter->wait_start_time = get_tick_count();
    waiter->timeout = timeout;
    
    // Add to head of list for simplicity
    waiter->next = event->waiters;
    event->waiters = waiter;
    event->waiter_count++;
}

/*
 * Remove a waiter from an event
 */
static void remove_event_waiter(event_t event, pid_t pid) {
    event_waiter_t* prev = NULL;
    event_waiter_t* current = event->waiters;
    
    while (current) {
        if (current->pid == pid) {
            // Remove from list
            if (prev) {
                prev->next = current->next;
            } else {
                event->waiters = current->next;
            }
            
            // Free the waiter
            kfree(current);
            event->waiter_count--;
            return;
        }
        
        prev = current;
        current = current->next;
    }
}

/*
 * Wake up waiters on an event
 */
static void wake_event_waiters(event_t event, uint32_t count) {
    uint32_t woken = 0;
    event_waiter_t* current = event->waiters;
    event_waiter_t* next;
    
    while (current && (count == 0 || woken < count)) {
        next = current->next;
        
        // Unblock the task
        unblock_task(current->pid);
        
        // Free the waiter
        kfree(current);
        woken++;
        
        current = next;
    }
    
    // Update waiters list and count
    event->waiters = current;
    event->waiter_count -= woken;
}

/*
 * Wait for an event to be signaled
 */
int event_wait(event_t event) {
    return event_timedwait(event, 0);  // Wait forever
}

/*
 * Wait for an event with timeout
 */
int event_timedwait(event_t event, uint64_t timeout_ms) {
    if (!event || event->header.type != IPC_TYPE_EVENT) {
        return -1;
    }
    
    pid_t current_pid = get_current_pid();
    int result = 0;
    
    // Convert ms to ticks if timeout is not 0
    uint64_t timeout_ticks = 0;
    if (timeout_ms > 0) {
        timeout_ticks = (timeout_ms * 1000) / get_tick_interval_us();
        if (timeout_ticks == 0) {
            timeout_ticks = 1;  // Minimum 1 tick
        }
    }
    
    // Lock the event
    mutex_lock(event->mutex);
    
    // If event is already signaled, handle it
    if (event->state == EVENT_STATE_SIGNALED) {
        // For auto-reset events, reset the state and only let one task through
        if (!(event->flags & EVENT_FLAG_MANUAL_RESET)) {
            event->state = EVENT_STATE_NONSIGNALED;
        }
        
        mutex_unlock(event->mutex);
        return 0;  // Success, event was signaled
    }
    
    // Event is not signaled, need to wait
    add_event_waiter(event, current_pid, timeout_ticks);
    
    // Unlock the event before blocking
    mutex_unlock(event->mutex);
    
    // Block the task
    block_task(current_pid);
    
    // When we wake up, check if the event is signaled
    mutex_lock(event->mutex);
    
    // If we timed out, the event might not be signaled
    if (event->state != EVENT_STATE_SIGNALED) {
        // We timed out, remove ourselves from the waiters list
        remove_event_waiter(event, current_pid);
        result = -1;  // Timeout
    } else if (!(event->flags & EVENT_FLAG_MANUAL_RESET)) {
        // Auto-reset: reset the event for the next waiter
        event->state = EVENT_STATE_NONSIGNALED;
    }
    
    mutex_unlock(event->mutex);
    return result;
}

/*
 * Signal an event, waking waiters
 */
int event_signal(event_t event) {
    if (!event || event->header.type != IPC_TYPE_EVENT) {
        return -1;
    }
    
    // Lock the event
    mutex_lock(event->mutex);
    
    // Mark as signaled
    event->state = EVENT_STATE_SIGNALED;
    
    // For auto-reset events, only wake one waiter
    if (!(event->flags & EVENT_FLAG_MANUAL_RESET)) {
        if (event->waiter_count > 0) {
            wake_event_waiters(event, 1);
            event->state = EVENT_STATE_NONSIGNALED;
        }
    } else {
        // For manual-reset events, wake all waiters
        wake_event_waiters(event, 0);
    }
    
    mutex_unlock(event->mutex);
    return 0;
}

/*
 * Broadcast to all waiters (same as signal for manual-reset events)
 */
int event_broadcast(event_t event) {
    if (!event || event->header.type != IPC_TYPE_EVENT) {
        return -1;
    }
    
    // Lock the event
    mutex_lock(event->mutex);
    
    // Mark as signaled
    event->state = EVENT_STATE_SIGNALED;
    
    // Wake all waiters
    wake_event_waiters(event, 0);
    
    mutex_unlock(event->mutex);
    return 0;
}

/*
 * Reset an event to non-signaled state
 */
int event_reset(event_t event) {
    if (!event || event->header.type != IPC_TYPE_EVENT) {
        return -1;
    }
    
    // Lock the event
    mutex_lock(event->mutex);
    
    // Reset to non-signaled
    event->state = EVENT_STATE_NONSIGNALED;
    
    mutex_unlock(event->mutex);
    return 0;
}

/*
 * Create an event set
 */
event_set_t create_event_set(const char* name, uint32_t max_events) {
    if (max_events == 0 || max_events > MAX_EVENTS_PER_SET) {
        return NULL;
    }
    
    event_set_t event_set = find_free_event_set();
    if (!event_set) {
        kernel_printf("Failed to allocate event set: no free slots\n");
        return NULL;
    }
    
    // Initialize event set
    event_set->header.type = IPC_TYPE_EVENT_SET;
    strncpy(event_set->header.name, name, MAX_IPC_NAME_LENGTH - 1);
    event_set->header.name[MAX_IPC_NAME_LENGTH - 1] = '\0';
    event_set->header.ref_count = 1;
    event_set->header.destroy_fn = destroy_event_set;
    
    // Create synchronization mutex
    char mutex_name[MAX_IPC_NAME_LENGTH];
    snprintf(mutex_name, MAX_IPC_NAME_LENGTH, "%s_mutex", name);
    event_set->mutex = create_mutex(mutex_name);
    
    if (!event_set->mutex) {
        memset(event_set, 0, sizeof(struct event_set));
        return NULL;
    }
    
    event_set->event_count = 0;
    memset(event_set->events, 0, sizeof(event_set->events));
    event_set->waiters = NULL;
    event_set->waiter_count = 0;
    
    event_set_count++;
    return event_set;
}

/*
 * Destroy an event set
 */
static void destroy_event_set(void* event_set_ptr) {
    event_set_t event_set = (event_set_t)event_set_ptr;
    
    if (!event_set || event_set->header.type != IPC_TYPE_EVENT_SET) {
        return;
    }
    
    // Lock the event set
    mutex_lock(event_set->mutex);
    
    // Wake up all waiters
    event_waiter_t* current = event_set->waiters;
    event_waiter_t* next;
    
    while (current) {
        next = current->next;
        
        // Unblock the task
        unblock_task(current->pid);
        
        // Free the waiter
        kfree(current);
        
        current = next;
    }
    
    event_set->waiters = NULL;
    event_set->waiter_count = 0;
    
    // Unlock and destroy mutex
    mutex_unlock(event_set->mutex);
    destroy_mutex(event_set->mutex);
    
    // Clear the event set
    memset(event_set, 0, sizeof(struct event_set));
    
    event_set_count--;
}

/*
 * Destroy an event set (public API)
 */
void destroy_event_set(event_set_t event_set) {
    if (!event_set || event_set->header.type != IPC_TYPE_EVENT_SET) {
        return;
    }
    
    destroy_event_set((void*)event_set);
}

/*
 * Add an event to an event set
 */
int event_set_add(event_set_t event_set, event_t event) {
    if (!event_set || event_set->header.type != IPC_TYPE_EVENT_SET ||
        !event || event->header.type != IPC_TYPE_EVENT) {
        return -1;
    }
    
    // Lock the event set
    mutex_lock(event_set->mutex);
    
    // Check if event is already in the set
    for (uint32_t i = 0; i < event_set->event_count; i++) {
        if (event_set->events[i] == event) {
            mutex_unlock(event_set->mutex);
            return 0;  // Already in set
        }
    }
    
    // Check if set is full
    if (event_set->event_count >= MAX_EVENTS_PER_SET) {
        mutex_unlock(event_set->mutex);
        return -1;
    }
    
    // Add the event to the set
    event_set->events[event_set->event_count++] = event;
    
    // Increment reference count on the event
    event->header.ref_count++;
    
    mutex_unlock(event_set->mutex);
    return 0;
}

/*
 * Remove an event from an event set
 */
int event_set_remove(event_set_t event_set, event_t event) {
    if (!event_set || event_set->header.type != IPC_TYPE_EVENT_SET ||
        !event || event->header.type != IPC_TYPE_EVENT) {
        return -1;
    }
    
    // Lock the event set
    mutex_lock(event_set->mutex);
    
    // Find the event in the set
    uint32_t i;
    for (i = 0; i < event_set->event_count; i++) {
        if (event_set->events[i] == event) {
            break;
        }
    }
    
    // If not found
    if (i >= event_set->event_count) {
        mutex_unlock(event_set->mutex);
        return -1;
    }
    
    // Remove the event (shift remaining events down)
    for (; i < event_set->event_count - 1; i++) {
        event_set->events[i] = event_set->events[i + 1];
    }
    
    event_set->event_count--;
    
    // Decrement reference count on the event
    event->header.ref_count--;
    
    mutex_unlock(event_set->mutex);
    return 0;
}

/*
 * Add a waiter to an event set
 */
static void add_event_set_waiter(event_set_t event_set, pid_t pid, uint64_t timeout) {
    event_waiter_t* waiter = (event_waiter_t*)kmalloc(sizeof(event_waiter_t));
    if (!waiter) {
        kernel_printf("Failed to allocate event set waiter\n");
        return;
    }
    
    // Initialize waiter
    waiter->pid = pid;
    waiter->wait_start_time = get_tick_count();
    waiter->timeout = timeout;
    
    // Add to head of list for simplicity
    waiter->next = event_set->waiters;
    event_set->waiters = waiter;
    event_set->waiter_count++;
}

/*
 * Remove a waiter from an event set
 */
static void remove_event_set_waiter(event_set_t event_set, pid_t pid) {
    event_waiter_t* prev = NULL;
    event_waiter_t* current = event_set->waiters;
    
    while (current) {
        if (current->pid == pid) {
            // Remove from list
            if (prev) {
                prev->next = current->next;
            } else {
                event_set->waiters = current->next;
            }
            
            // Free the waiter
            kfree(current);
            event_set->waiter_count--;
            return;
        }
        
        prev = current;
        current = current->next;
    }
}

/*
 * Wake up waiters on an event set
 */
static void wake_event_set_waiters(event_set_t event_set, event_t signaled_event, uint32_t count) {
    uint32_t woken = 0;
    event_waiter_t* current = event_set->waiters;
    event_waiter_t* next;
    
    while (current && (count == 0 || woken < count)) {
        next = current->next;
        
        // Unblock the task
        unblock_task(current->pid);
        
        // Free the waiter
        kfree(current);
        woken++;
        
        current = next;
    }
    
    // Update waiters list and count
    event_set->waiters = current;
    event_set->waiter_count -= woken;
}

/*
 * Wait for any event in the set to be signaled
 */
int event_set_wait(event_set_t event_set, event_t* signaled_event) {
    return event_set_timedwait(event_set, signaled_event, 0);  // Wait forever
}

/*
 * Wait for any event in the set with timeout
 */
int event_set_timedwait(event_set_t event_set, event_t* signaled_event, uint64_t timeout_ms) {
    if (!event_set || event_set->header.type != IPC_TYPE_EVENT_SET) {
        return -1;
    }
    
    pid_t current_pid = get_current_pid();
    int result = -1;
    
    // Convert ms to ticks if timeout is not 0
    uint64_t timeout_ticks = 0;
    if (timeout_ms > 0) {
        timeout_ticks = (timeout_ms * 1000) / get_tick_interval_us();
        if (timeout_ticks == 0) {
            timeout_ticks = 1;  // Minimum 1 tick
        }
    }
    
    // Lock the event set
    mutex_lock(event_set->mutex);
    
    // Check if any event is already signaled
    for (uint32_t i = 0; i < event_set->event_count; i++) {
        event_t event = event_set->events[i];
        
        // Lock the event
        mutex_lock(event->mutex);
        
        if (event->state == EVENT_STATE_SIGNALED) {
            // Found a signaled event
            
            // For auto-reset events, reset the state
            if (!(event->flags & EVENT_FLAG_MANUAL_RESET)) {
                event->state = EVENT_STATE_NONSIGNALED;
            }
            
            // Set the result
            if (signaled_event) {
                *signaled_event = event;
            }
            
            mutex_unlock(event->mutex);
            mutex_unlock(event_set->mutex);
            return 0;  // Success
        }
        
        mutex_unlock(event->mutex);
    }
    
    // No event is signaled, need to wait
    add_event_set_waiter(event_set, current_pid, timeout_ticks);
    
    // Unlock the event set before blocking
    mutex_unlock(event_set->mutex);
    
    // Block the task
    block_task(current_pid);
    
    // When we wake up, check which event was signaled
    mutex_lock(event_set->mutex);
    
    // TODO: We need a better way to know which event was signaled
    // For now, just check all events
    for (uint32_t i = 0; i < event_set->event_count; i++) {
        event_t event = event_set->events[i];
        
        // Lock the event
        mutex_lock(event->mutex);
        
        if (event->state == EVENT_STATE_SIGNALED) {
            // This event is signaled
            
            // For auto-reset events, reset the state
            if (!(event->flags & EVENT_FLAG_MANUAL_RESET)) {
                event->state = EVENT_STATE_NONSIGNALED;
            }
            
            // Set the result
            if (signaled_event) {
                *signaled_event = event;
            }
            
            result = 0;  // Success
            mutex_unlock(event->mutex);
            break;
        }
        
        mutex_unlock(event->mutex);
    }
    
    // Clean up our waiter if we're still in the list
    remove_event_set_waiter(event_set, current_pid);
    
    mutex_unlock(event_set->mutex);
    return result;
}

/*
 * Check for timed-out event waiters
 */
static void check_event_timeouts(void) {
    uint64_t current_tick = get_tick_count();
    
    // Check all events
    for (uint32_t i = 0; i < MAX_EVENTS; i++) {
        event_t event = &event_pool[i];
        
        if (event->header.type != IPC_TYPE_EVENT) {
            continue;
        }
        
        // Lock the event
        mutex_lock(event->mutex);
        
        // Check for timed-out waiters
        event_waiter_t* prev = NULL;
        event_waiter_t* current = event->waiters;
        
        while (current) {
            event_waiter_t* next = current->next;
            
            // Check if the waiter has timed out
            if (current->timeout > 0 && 
                (current_tick - current->wait_start_time) >= current->timeout) {
                
                // Unblock the task
                unblock_task(current->pid);
                
                // Remove from list
                if (prev) {
                    prev->next = next;
                } else {
                    event->waiters = next;
                }
                
                // Free the waiter
                kfree(current);
                event->waiter_count--;
                
                current = next;
            } else {
                prev = current;
                current = next;
            }
        }
        
        mutex_unlock(event->mutex);
    }
    
    // Check all event sets
    for (uint32_t i = 0; i < MAX_EVENT_SETS; i++) {
        event_set_t event_set = &event_set_pool[i];
        
        if (event_set->header.type != IPC_TYPE_EVENT_SET) {
            continue;
        }
        
        // Lock the event set
        mutex_lock(event_set->mutex);
        
        // Check for timed-out waiters
        event_waiter_t* prev = NULL;
        event_waiter_t* current = event_set->waiters;
        
        while (current) {
            event_waiter_t* next = current->next;
            
            // Check if the waiter has timed out
            if (current->timeout > 0 && 
                (current_tick - current->wait_start_time) >= current->timeout) {
                
                // Unblock the task
                unblock_task(current->pid);
                
                // Remove from list
                if (prev) {
                    prev->next = next;
                } else {
                    event_set->waiters = next;
                }
                
                // Free the waiter
                kfree(current);
                event_set->waiter_count--;
                
                current = next;
            } else {
                prev = current;
                current = next;
            }
        }
        
        mutex_unlock(event_set->mutex);
    }
}

/*
 * Get the tick interval in microseconds
 * This is a helper function for timeout calculations
 */
static uint64_t get_tick_interval_us(void) {
    // For now, we assume a fixed 1ms tick interval
    // This should be replaced with the actual tick interval from the timer subsystem
    return 1000;  // 1000 microseconds = 1ms
}

/*
 * Clean up events for a terminated task
 */
void cleanup_task_events(pid_t pid) {
    // Check all events for waiters from this task
    for (uint32_t i = 0; i < MAX_EVENTS; i++) {
        event_t event = &event_pool[i];
        
        if (event->header.type != IPC_TYPE_EVENT) {
            continue;
        }
        
        // Lock the event
        mutex_lock(event->mutex);
        
        // Remove any waiters for this task
        remove_event_waiter(event, pid);
        
        mutex_unlock(event->mutex);
    }
    
    // Check all event sets for waiters from this task
    for (uint32_t i = 0; i < MAX_EVENT_SETS; i++) {
        event_set_t event_set = &event_set_pool[i];
        
        if (event_set->header.type != IPC_TYPE_EVENT_SET) {
            continue;
        }
        
        // Lock the event set
        mutex_lock(event_set->mutex);
        
        // Remove any waiters for this task
        remove_event_set_waiter(event_set, pid);
        
        mutex_unlock(event_set->mutex);
    }
}

/*
 * Initialize the event system
 */
void init_event_system(void) {
    kernel_printf("Initializing event notification system...\n");
    
    // Initialize event pool
    memset(event_pool, 0, sizeof(event_pool));
    event_count = 0;
    
    // Initialize event set pool
    memset(event_set_pool, 0, sizeof(event_set_pool));
    event_set_count = 0;
    
    // Create initial kernel events
    event_t kernel_event = create_event("kernel_event");
    if (!kernel_event) {
        kernel_panic("Failed to create kernel event");
    }
    
    // Enable manual reset for kernel event
    kernel_event->flags |= EVENT_FLAG_MANUAL_RESET;
    
    kernel_printf("Event system initialized: %d events, %d event sets\n", 
                 event_count, event_set_count);
}

/*
 * Initialize the event system and register with scheduler
 * Called during kernel startup
 */
void init_event_subsystem(void) {
    // Initialize the event system
    init_event_system();
    
    // Register timeout checker with the scheduler
    // This would be called periodically by the timer interrupt handler
    // register_timeout_checker(check_event_timeouts);
}
