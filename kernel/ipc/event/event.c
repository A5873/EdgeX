/*
 * EdgeX OS - Event Subsystem Implementation
 *
 * This file implements the event notification system for EdgeX OS,
 * providing synchronization mechanisms between tasks through events
 * and event sets.
 */

#include <edgex/kernel.h>
#include <edgex/memory.h>
#include <edgex/scheduler.h>
#include <edgex/ipc/common.h>
#include <edgex/ipc/mutex.h>
#include <edgex/ipc/event.h>

/* Maximum number of events in the system */
#define MAX_EVENTS 256

/* Maximum number of event sets in the system */
#define MAX_EVENT_SETS 64

/* Maximum number of events per event set */
#define MAX_EVENTS_PER_SET 32

/* Event flags */
#define EVENT_FLAG_MANUAL_RESET   (1 << 0)  /* Event stays signaled until reset */
#define EVENT_FLAG_INITIALLY_SET  (1 << 1)  /* Event starts in signaled state */

/* Wait queue flags */
#define WAIT_QUEUE_EVENT     (1 << 2)  /* Queue is for an event */
#define WAIT_QUEUE_EVENT_SET (1 << 3)  /* Queue is for an event set */

/* Wait queue status */
#define WAIT_STATUS_WAITING    0  /* Task is waiting */
#define WAIT_STATUS_SIGNALED   1  /* Task was signaled */
#define WAIT_STATUS_TIMEOUT    2  /* Task timed out */

/* Event states */
typedef enum {
    EVENT_STATE_NONSIGNALED = 0,
    EVENT_STATE_SIGNALED = 1
} event_state_t;

/* Waiter structure - represents a task waiting on a resource */
typedef struct waiter {
    pid_t pid;                    /* PID of waiting task */
    uint64_t wait_start_time;     /* When the wait started */
    uint64_t timeout;             /* Timeout in milliseconds (0 = forever) */
    uint32_t status;              /* Wait status (WAIT_STATUS_*) */
    void* user_data;              /* User data for the waiter (e.g., event that was signaled) */
    struct waiter* next;          /* Next waiter in queue */
} waiter_t;

/* Wait queue - list of tasks waiting for a resource */
typedef struct {
    char name[MAX_IPC_NAME_LENGTH]; /* Queue name */
    uint32_t flags;               /* Queue flags */
    waiter_t* waiter = queue->head;
    while (waiter) {
        if (waiter->pid == pid) {
            mutex_unlock(queue->lock);
            return waiter;
        }
        waiter = waiter->next;
    }
    
    mutex_unlock(queue->lock);
    return NULL;
}
/*
 * Initialize the event subsystem
 */
void init_event_subsystem(void) {
    kernel_printf("Initializing event subsystem...\n");
    
    // Initialize event pool
    memset(event_pool, 0, sizeof(event_pool));
    event_count = 0;
    
    // Initialize event set pool
    memset(event_set_pool, 0, sizeof(event_set_pool));
    event_set_count = 0;
    
    // Create mutexes for pool access
    event_pool_mutex = create_mutex("event_pool_mutex");
    if (!event_pool_mutex) {
        kernel_panic("Failed to create event pool mutex");
    }
    
    event_set_pool_mutex = create_mutex("event_set_pool_mutex");
    if (!event_set_pool_mutex) {
        kernel_panic("Failed to create event set pool mutex");
    }
    
    event_subsystem_initialized = true;
    
    kernel_printf("Event subsystem initialized: capacity for %d events, %d event sets\n", 
                while (prev && prev->next != current) {
                    prev = prev->next;
                }
                
                if (prev) {
                    prev->next = next;
                }
            }
            
            // Update tail if needed
            if (queue->tail == current) {
                queue->tail = prev;
            }
            
            // Free the waiter
            kfree(current);
            woken++;
        }
        
        current = next;
    }
    
    queue->waiter_count -= woken;
    
    mutex_unlock(queue->lock);
}
                unblock_task(current->pid);
                
                // Remove from list
                if (prev) {
                    prev->next = next;
                } else {
                    queue->head = next;
                }
                
                // Update tail if needed
                if (queue->tail == current) {
                    queue->tail = prev;
                }
                
                // Free the waiter
                kfree(current);
                queue->waiter_count--;
                
                // Update stats
                update_ipc_stats(IPC_STAT_TIMEOUT_FAILURES, 1);
            } else {
                prev = current;
            }
        } else {
            prev = current;
        }
        
        current = next;
    }
    
    mutex_unlock(queue->lock);
    return NULL;
}

/*
 * Find a free event set slot
 */
static event_set_t find_free_event_set(void) {
    if (!event_subsystem_initialized) {
        kernel_printf("Event subsystem not initialized\n");
        return NULL;
    }
    
    mutex_lock(event_set_pool_mutex);
    
    for (uint32_t i = 0; i < MAX_EVENT_SETS; i++) {
        if (event_set_pool[i].header.type == 0) {
            mutex_unlock(event_set_pool_mutex);
            return &event_set_pool[i];
        }
    }
    
    mutex_unlock(event_set_pool_mutex);
    return NULL;
}

/*
 * Initialize a wait queue
 */
static void init_wait_queue(wait_queue_t* queue, const char* name, uint32_t flags, void* owner) {
    strncpy(queue->name, name, MAX_IPC_NAME_LENGTH - 1);
    queue->name[MAX_IPC_NAME_LENGTH - 1] = '\0';
    queue->flags = flags;
    queue->head = NULL;
    queue->tail = NULL;
    queue->waiter_count = 0;
    queue->owner = owner;
    
    /* Create a mutex for this queue */
    char mutex_name[MAX_IPC_NAME_LENGTH];
    snprintf(mutex_name, MAX_IPC_NAME_LENGTH, "%s_mutex", name);
    queue->lock = create_mutex(mutex_name);
}

/*
 * Add a waiter to a wait queue
 */
static void add_waiter(wait_queue_t* queue, pid_t pid, uint64_t timeout, void* user_data) {
    mutex_lock(queue->lock);
    
    waiter_t* waiter = (waiter_t*)kmalloc(sizeof(waiter_t));
    if (!waiter) {
        kernel_printf("Failed to allocate waiter structure\n");
        mutex_unlock(queue->lock);
        return;
    }
    
    // Initialize waiter
    waiter->pid = pid;
    waiter->wait_start_time = get_tick_count();
    waiter->timeout = timeout;
    waiter->status = WAIT_STATUS_WAITING;
    waiter->user_data = user_data;
    waiter->next = NULL;
    
    // Add to end of queue
    if (queue->tail) {
        queue->tail->next = waiter;
        queue->tail = waiter;
    } else {
        queue->head = queue->tail = waiter;
    }
    
    queue->waiter_count++;
    
    mutex_unlock(queue->lock);
}

/*
 * Remove a waiter from a wait queue
 */
static void remove_waiter(wait_queue_t* queue, pid_t pid) {
    mutex_lock(queue->lock);
    
    waiter_t* prev = NULL;
    waiter_t* current = queue->head;
    
    while (current) {
        if (current->pid == pid) {
                while (prev && prev->next != current) {
                    prev = prev->next;
                }
                
                if (prev) {
                    prev->next = next;
                }
            }
            
            // Update tail if needed
            if (queue->tail == current) {
                queue->tail = prev;
            }
            
            // Free the waiter
            kfree(current);
            woken++;
        }
        
        current = next;
    }
    
    mutex_unlock(queue->lock);
}

/*
 * Check for timeouts in a wait queue
 */
static void check_timeouts(wait_queue_t* queue) {
    uint64_t current_time = get_tick_count();
    
    mutex_lock(queue->lock);
    
    waiter_t* current = queue->head;
    waiter_t* prev = NULL;
    
    while (current) {
        waiter_t* next = current->next;
        
        // If timeout is 0, wait forever
        if (current->timeout > 0) {
            uint64_t elapsed = current_time - current->wait_start_time;
            
            // If timeout expired
            if (elapsed >= current->timeout) {
                current->status = WAIT_STATUS_TIMEOUT;
                
                // Unblock the task
                unblock_task(current
            } else {
                queue->head = current->next;
            }
            
            // Update tail if needed
            if (queue->tail == current) {
                queue->tail = prev;
            }
            
            // Free the waiter
            kfree(current);
            queue->waiter_count--;
            break;
        }
        
        prev = current;
        current = current->next;
    }
    
    mutex_unlock(queue->lock);
}

/*
 * Wake up waiters in a queue
 */
static void wake_waiters(wait_queue_t* queue, uint32_t count, uint32_t status, void* event) {
    mutex_lock(queue->lock);
    
    uint32_t woken = 0;
    waiter_t* current = queue->head;
    
    while (current && (count == 0 || woken < count)) {
        waiter_t* next = current->next;
        
        // Update status and event
        current->status = status;
        current->user_data = event;
        
        // Unblock the task
        unblock_task(current->pid);
        
        if (count > 0) {
            // If we need to wake up a specific number of waiters,
            // remove them from the queue
            if (current == queue->head) {
                queue->head = next;
            } else {
                waiter_t* prev = queue->head;
                while (prev && prev->next !=
    return NULL;
}

/*
 * Dump information about all events for debugging
 */
void dump_all_events(void) {
    if (!event_subsystem_initialized) {
        kernel_printf("Event subsystem not initialized\n");
        return;
    }
    
    kernel_printf("=== EVENT DUMP ===\n");
    kernel_printf("Total events: %u/%u\n", event_count, MAX_EVENTS);
    
    mutex_lock(event_pool_mutex);
    
    for (uint32_t i = 0; i < MAX_EVENTS; i++) {
        if (event_pool[i].header.type == IPC_TYPE_EVENT) {
            kernel_printf("Event %u: %s, owner=%d, state=%s, flags=%u, waiters=%u\n",
                         i,
                         event_pool[i].header.name,
                         event_pool[i].owner,
                         event_pool[i].state == EVENT_STATE_SIGNALED ? "SIGNALED" : "NONSIGNALED",
                         event_pool[i].flags,
                         event_pool[i].wait_queue.waiter_count);
        }
    }
    
    mutex_unlock(event_pool_mutex);
    
    kernel_printf("Total event sets: %u/%u\n", event_set_count, MAX_EVENT_SETS);
    
    mutex_lock(event_set_pool_mutex);
    
    for (uint32_t i = 0; i < MAX_EVENT_SETS; i++) {
        if (event_set_pool[i].header.type == IPC_TYPE_EVENT_SET) {
            kernel_printf("Event Set %u: %s, owner=%d, events=%u/%u, waiters=%u\n",
                         i,
                         event_set_pool[i].header.name,
                         event_set_pool[i].owner,
                         event_set_pool[i].event_count,
                         event_set_pool[i].max_events,
                         event_set_pool[i].wait_queue.waiter_count);
            
            // List events in set
            for (uint32_t j = 0; j < event_set_pool[i].event_count; j++) {
                event_t event = event_set_pool[i].events[j];
                kernel_printf("  - Event: %s, state=%s\n",
                             event->header.name,
                             event->state == EVENT_STATE_SIGNALED ? "SIGNALED" : "NONSIGNALED");
            }
        }
    }
    
    mutex_unlock(event_set_pool_mutex);
    
    kernel_printf("==================\n");
}
            kernel_printf("Event Set %u: %s, owner=%d, events=%u/%u, waiters=%u\n",
                         i,
                         event_set_pool[i].header.name,
                         event_set_pool[i].owner,
                         event_set_pool[i].event_count,
                         event_set_pool[i].max_events,
                         event_set_pool[i].wait_queue.waiter_count);
            
            // List events in set
            for (uint32_t j = 0; j < event_set_pool[i].event_count; j++) {
                event_t event = event_set_pool[i].events[j];
                kernel_printf("  - Event: %s, state=%s\n",
                             event->header.name,
                             event->state == EVENT_STATE_SIGNALED ? "SIGNALED" : "NONSIGNALED");
            }
        }
    }
    
    mutex_unlock(event_set_pool_mutex);
    
    kernel_printf("==================\n");
}

/*
 * Create an event
 */
event_t create_event(const char* name) {
    event_t event = find_free_event();
    if (!event) {
        kernel_printf("Failed to allocate event: no free slots\n");
        update_ipc_stats(IPC_STAT_ALLOCATION_FAILURES, 1);
        return NULL;
    }
    
    // Initialize event
    event->header.type = IPC_TYPE_EVENT;
    strncpy(event->header.name, name, MAX_IPC_NAME_LENGTH - 1);
    event->header.name[MAX_IPC_NAME_LENGTH - 1] = '\0';
    event->header.ref_count = 1;
    event->header.destroy_fn = destroy_event_internal;
    
    event->state = EVENT_STATE_NONSIGNALED;
    event->flags = 0;  // Auto-reset by default
    event->owner = get_current_pid();
    
    // Initialize wait queue
    init_wait_queue(&event->wait_queue, name, WAIT_QUEUE_EVENT, event);
    
    // Update statistics
    mutex_lock(event_pool_mutex);
    event_count++;
    mutex_unlock(event_pool_mutex);
    
    update_ipc_stats(IPC_STAT_OBJECT_CREATED, 1);
    return event;
}

/*
 * Destroy an event internally
 */
static void destroy_event_internal(void* event_ptr) {
    event_t event = (event_t)event_ptr;
    
    if (!event || event->header.type != IPC_TYPE_EVENT) {
        return;
    }
    
    // Wake up all waiters
    wake_waiters(&event->wait_queue, 0, WAIT_STATUS_SIGNALED, event);
    
    // Destroy the wait queue mutex
    destroy_mutex(event->wait_queue.lock);
    
    // Clear the event
    memset(event, 0, sizeof(struct event));
    
    // Update statistics
    mutex_lock(event_pool_mutex);
    event_count--;
    mutex_unlock(event_pool_mutex);
    
    update_ipc_stats(IPC_STAT_OBJECT_DESTROYED, 1);
}
    // Disable interrupts for atomic operation
    __asm__ volatile("cli");
    
    // Update stats
    update_ipc_stats(IPC_STAT_EVENT_OPERATIONS, 1);
    
    // Check if any event is already signaled
    for (uint32_t i = 0; i < event_set->event_count; i++) {
        event_t event = event_set->events[i];
        if (event->state == EVENT_STATE_SIGNALED) {
            // If auto-reset event, clear the signal
            if (!(event->flags & EVENT_FLAG_MANUAL_RESET)) {
                event->state = EVENT_STATE_NONSIGNALED;
            }
            
            // Return the signaor atomic operation
    __asm__ volatile("cli");
    
    // Update stats
    update_ipc_stats(IPC_STAT_EVENT_OPERATIONS, 1);
    
    // Mark the event as signaled
    event->state = EVENT_STATE_SIGNALED;
    
    // If auto-reset event and there are waiters, wake one
    if (!(event->flags & EVENT_FLAG_MANUAL_RESET) && event->wait_queue.waiter_count > 0) {
        wake_waiters(&event->wait_queue, 1, WAIT_STATUS_SIGNALED, event);
        // Auto-reset events reset after waking one waiter
        event->state = EVENT_STATE_NONSIGNALED;
    } 
    // If manual reset, wake all waiters
    else if (event->flags & EVENT_FLAG_MANUAL_RESET && event->wait_queue.waiter_count > 0) {
        wake_waiters(&event->wait_queue, 0, WAIT_STATUS_SIGNALED, event);
        // Manual reset events stay signaled
    }
    
    __asm__ volatile("sti");
    return 0;
}

/*
 * Broadcast an event (wake up all waiters)
 */
int event_broadcast(event_t event) {
    if (!event || event->header.type != IPC_TYPE_EVENT) {
        return -1;
    }
    
    // Disable interrupts for atomic operation
    __asm__ volatile("cli");
    
    // Update stats
    update_ipc_stats(IPC_STAT_EVENT_OPERATIONS, 1);
    
    // Mark the event as signaled
    event->state = EVENT_STATE_SIGNALED;
    
    // Wake all waiters
    if (event->wait_queue.waiter_count > 0) {
        wake_waiters(&event->wait_queue, 0, WAIT_STATUS_SIGNALED, event);
    }
    
    // Auto-reset events reset after all waiters are woken
    if (!(event->flags & EVENT_FLAG_MANUAL_RESET)) {
        event->state = EVENT_STATE_NONSIGNALED;
    }
    
    __asm__ volatile("sti");
    return 0;
}

/*
 * Reset an event to non-signaled state
 */
int event_reset(event_t event) {
    if (!event || event->header.type != IPC_TYPE_EVENT) {
        return -1;
    }
    
    // Disable interrupts for atomic operation
    __asm__ volatile("cli");
    
    // Update stats
    update_ipc_stats(IPC_STAT_EVENT_OPERATIONS, 1);
    
    // Reset the event to non-signaled state
    event->state = EVENT_STATE_NONSIGNALED;
    
    __asm__ volatile("sti");
    return 0;
}

/*
 * Create an event set
 */
event_set_t create_event_set(const char* name, uint32_t max_events) {
    // Validate input
    if (max_events == 0 || max_events > MAX_EVENTS_PER_SET) {
        kernel_printf("Invalid max events: %u (max is %u)\n", max_events, MAX_EVENTS_PER_SET);
        update_ipc_stats(IPC_STAT_ALLOCATION_FAILURES, 1);
        return NULL;
    }
    
    event_set_t event_set = find_free_event_set();
    if (!event_set) {
        kernel_printf("Failed to allocate event set: no free slots\n");
        update_ipc_stats(IPC_STAT_ALLOCATION_FAILURES, 1);
        return NULL;
    }
    
    // Initialize event set
    event_set->header.type = IPC_TYPE_EVENT_SET;
    strncpy(event_set->header.name, name, MAX_IPC_NAME_LENGTH - 1);
    event_set->header.name[MAX_IPC_NAME_LENGTH - 1] = '\0';
    event_set->header.ref_count = 1;
    event_set->header.destroy_fn = destroy_event_set_internal;
    
    event_set->event_count = 0;
    event_set->max_events = max_events;
    event_set->owner = get_current_pid();
    
    // Clear the events array
    memset(event_set->events, 0, sizeof(event_t) * MAX_EVENTS_PER_SET);
    
    // Initialize wait queue
    init_wait_queue(&event_set->wait_queue, name, WAIT_QUEUE_EVENT_SET, event_set);
    
    // Update statistics
    mutex_lock(event_set_pool_mutex);
    event_set_count++;
    mutex_unlock(event_set_pool_mutex);
    
    update_ipc_stats(IPC_STAT_OBJECT_CREATED, 1);
    return event_set;
}

/*
 * Destroy an event set internally
 */
static void destroy_event_set_internal(void* event_set_ptr) {
    event_set_t event_set = (event_set_t)event_set_ptr;
    
    if (!event_set || event_set->header.type != IPC_TYPE_EVENT_SET) {
        return;
    }
    
    // Wake up all waiters
    wake_waiters(&event_set->wait_queue, 0, WAIT_STATUS_SIGNALED, NULL);
    
    // Destroy the wait queue mutex
    destroy_mutex(event_set->wait_queue.lock);
    
    // Clear the event set
    memset(event_set, 0, sizeof(struct event_set));
    
    // Update statistics
    mutex_lock(event_set_pool_mutex);
    event_set_count--;
    mutex_unlock(event_set_pool_mutex);
    
    update_ipc_stats(IPC_STAT_OBJECT_DESTROYED, 1);
}

/*
 * Destroy an event set (public API)
 */
void destroy_event_set(event_set_t event_set) {
    if (!event_set || event_set->header.type != IPC_TYPE_EVENT_SET) {
        return;
    }
    
    // Only the owner or the kernel can destroy an event set
    pid_t current_pid = get_current_pid();
    if (event_set->owner != current_pid && current_pid != KERNEL_PID) {
        kernel_printf("Permission denied: task %d cannot destroy event set owned by %d\n", 
                     current_pid, event_set->owner);
        update_ipc_stats(IPC_STAT_PERMISSION_FAILURES, 1);
        return;
    }
    
    destroy_event_set_internal(event_set);
}

/*
 * Add an event to an event set
 */
int event_set_add(event_set_t event_set, event_t event) {
    if (!event_set || event_set->header.type != IPC_TYPE_EVENT_SET || 
        !event || event->header.type != IPC_TYPE_EVENT) {
        return -1;
    }
    
    // Check if the event set is full
    if (event_set->event_count >= event_set->max_events) {
        kernel_printf("Event set is full: %u/%u events\n", 
                     event_set->event_count, event_set->max_events);
        return -1;
    }
    
    // Check if the event is already in the set
    for (uint32_t i = 0; i < event_set->event_count; i++) {
        if (event_set->events[i] == event) {
            return 0;  // Already in the set
        }
    }
    
    // Add the event to the set
    event_set->events[event_set->event_count++] = event;
    
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
    
    // Find the event in the set
    uint32_t i;
    for (i = 0; i < event_set->event_count; i++) {
        if (event_set->events[i] == event) {
            break;
        }
    }
    
    // If not found, return error
    if (i >= event_set->event_count) {
        return -1;
    }
    
    // Remove the event by shifting all subsequent events
    for (; i < event_set->event_count - 1; i++) {
        event_set->events[i] = event_set->events[i + 1];
    }
    
    // Clear the last slot and decrement count
    event_set->events[--event_set->event_count] = NULL;
    
    return 0;
}

/*
 * Wait for any event in the set to be signaled
 */
int event_set_wait(event_set_t event_set, event_t* signaled_event) {
    return event_set_timedwait(event_set, signaled_event, 0);  // Wait forever
}

/*
 * Wait for any event in the set to be signaled, with timeout
 */
int event_set_timedwait(event_set_t event_set, event_t* signaled_event, uint64_t timeout_ms) {
    if (!event_set || event_set->header.type != IPC_TYPE_EVENT_SET) {
        return -1;
    }
    
    // If no events in the set, return error
    if (event_set->event_count == 0) {
        return -1;
    }
    
    pid_t current_pid = get_current_pid();
    
    // Disable interrupts for atomic operation
    __asm__ volatile("cli");
    
    // Update

