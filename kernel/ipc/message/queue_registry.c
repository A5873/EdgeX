/*
 * EdgeX OS - Message Queue Registry
 *
 * This file implements the task-to-queue mapping registry that allows
 * efficient lookup of message queues belonging to specific tasks.
 */

#include <edgex/ipc/message.h>
#include <edgex/ipc/mutex.h>
#include <edgex/scheduler.h>
#include <edgex/kernel.h>
#include <string.h>

/* Registry constants */
#define MAX_TASK_QUEUES        16      /* Maximum queues per task */
#define MAX_REGISTERED_TASKS   64      /* Maximum tasks in registry */

/* Queue registration entry */
typedef struct {
    uint32_t task_id;                          /* Task that owns this entry */
    message_queue_t queues[MAX_TASK_QUEUES];   /* Queues owned by this task */
    uint32_t queue_count;                      /* Number of queues in use */
    uint32_t recv_queue_idx;                   /* Default receive queue index */
    uint32_t send_queue_idx;                   /* Default send queue index */
} queue_registry_entry_t;

/* Global registry state */
static mutex_t registry_lock;
static queue_registry_entry_t task_registry[MAX_REGISTERED_TASKS];
static uint32_t registry_entry_count = 0;
static bool registry_initialized = false;

/*
 * Initialize the queue registry
 */
static void init_queue_registry(void) {
    if (!registry_initialized) {
        mutex_init(&registry_lock);
        memset(task_registry, 0, sizeof(task_registry));
        registry_entry_count = 0;
        registry_initialized = true;
    }
}

/*
 * Find a registry entry for a task
 * Returns the index of the entry or -1 if not found
 * Note: registry_lock must be held when calling this function
 */
static int find_registry_entry(uint32_t task_id) {
    for (uint32_t i = 0; i < registry_entry_count; i++) {
        if (task_registry[i].task_id == task_id) {
            return i;
        }
    }
    return -1;
}

/*
 * Find or create a registry entry for a task
 * Returns the index of the entry or -1 if registry is full
 * Note: registry_lock must be held when calling this function
 */
static int find_or_create_registry_entry(uint32_t task_id) {
    int idx = find_registry_entry(task_id);
    
    if (idx >= 0) {
        return idx;
    }
    
    if (registry_entry_count >= MAX_REGISTERED_TASKS) {
        return -1;  /* Registry is full */
    }
    
    /* Create a new entry */
    idx = registry_entry_count++;
    task_registry[idx].task_id = task_id;
    task_registry[idx].queue_count = 0;
    task_registry[idx].recv_queue_idx = 0;
    task_registry[idx].send_queue_idx = 0;
    
    return idx;
}

/*
 * Register a queue with a task
 */
void register_task_queue(uint32_t task_id, message_queue_t queue) {
    if (!registry_initialized) {
        init_queue_registry();
    }
    
    if (queue == NULL) {
        return;
    }
    
    mutex_lock(&registry_lock);
    
    int idx = find_or_create_registry_entry(task_id);
    if (idx >= 0) {
        /* Check if this queue is already registered */
        for (uint32_t i = 0; i < task_registry[idx].queue_count; i++) {
            if (task_registry[idx].queues[i] == queue) {
                /* Already registered */
                mutex_unlock(&registry_lock);
                return;
            }
        }
        
        /* Add to registry if there's space */
        if (task_registry[idx].queue_count < MAX_TASK_QUEUES) {
            uint32_t queue_idx = task_registry[idx].queue_count++;
            task_registry[idx].queues[queue_idx] = queue;
            
            /* If this is the first queue, set it as default for both sending and receiving */
            if (queue_idx == 0) {
                task_registry[idx].recv_queue_idx = 0;
                task_registry[idx].send_queue_idx = 0;
            }
        }
    }
    
    mutex_unlock(&registry_lock);
}

/*
 * Unregister a queue from a task
 */
void unregister_task_queue(uint32_t task_id, message_queue_t queue) {
    if (!registry_initialized || queue == NULL) {
        return;
    }
    
    mutex_lock(&registry_lock);
    
    int idx = find_registry_entry(task_id);
    if (idx >= 0) {
        for (uint32_t i = 0; i < task_registry[idx].queue_count; i++) {
            if (task_registry[idx].queues[i] == queue) {
                /* Found the queue, remove it */
                if (i < task_registry[idx].queue_count - 1) {
                    /* Shift remaining queues down */
                    memmove(&task_registry[idx].queues[i], 
                            &task_registry[idx].queues[i + 1],
                            (task_registry[idx].queue_count - i - 1) * sizeof(message_queue_t));
                }
                
                task_registry[idx].queue_count--;
                
                /* Update default indices if needed */
                if (task_registry[idx].recv_queue_idx >= task_registry[idx].queue_count) {
                    task_registry[idx].recv_queue_idx = 0;
                }
                if (task_registry[idx].send_queue_idx >= task_registry[idx].queue_count) {
                    task_registry[idx].send_queue_idx = 0;
                }
                
                break;
            }
        }
    }
    
    mutex_unlock(&registry_lock);
}

/*
 * Find a queue for a task with the given lookup mode
 */
message_queue_t find_task_queue(uint32_t task_id, int lookup_mode) {
    if (!registry_initialized) {
        init_queue_registry();
    }
    
    message_queue_t result = NULL;
    
    mutex_lock(&registry_lock);
    
    int idx = find_registry_entry(task_id);
    if (idx >= 0 && task_registry[idx].queue_count > 0) {
        if (lookup_mode == QUEUE_LOOKUP_RECEIVE) {
            /* Get the default receive queue */
            result = task_registry[idx].queues[task_registry[idx].recv_queue_idx];
        } else if (lookup_mode == QUEUE_LOOKUP_SEND) {
            /* Get the default send queue */
            result = task_registry[idx].queues[task_registry[idx].send_queue_idx];
        } else if (lookup_mode == QUEUE_LOOKUP_ANY) {
            /* Get any queue (first one) */
            result = task_registry[idx].queues[0];
        }
    }
    
    mutex_unlock(&registry_lock);
    
    return result;
}

/*
 * Get a queue for the current task with the given lookup mode
 */
message_queue_t get_current_task_queue(int lookup_mode) {
    return find_task_queue(get_current_task_id(), lookup_mode);
}

/*
 * Create a message queue associated with a task
 */
message_queue_t create_task_message_queue(const char* name, uint32_t max_messages, uint32_t owner_pid) {
    /* Create the message queue first */
    message_queue_t queue = create_message_queue(name, max_messages);
    
    if (queue == NULL) {
        return NULL;
    }
    
    /* Register the queue with the task */
    register_task_queue(owner_pid, queue);
    
    return queue;
}

/*
 * Clean up all queues for a task
 */
void cleanup_task_queues(uint32_t task_id) {
    if (!registry_initialized) {
        return;
    }
    
    mutex_lock(&registry_lock);
    
    int idx = find_registry_entry(task_id);
    if (idx >= 0) {
        /* We don't destroy the queues here - that happens at the message subsystem level.
         * We just remove the registry entries */
        if (idx < registry_entry_count - 1) {
            /* Shift remaining entries down */
            memmove(&task_registry[idx], 
                    &task_registry[idx + 1],
                    (registry_entry_count - idx - 1) * sizeof(queue_registry_entry_t));
        }
        registry_entry_count--;
    }
    
    mutex_unlock(&registry_lock);
}

/*
 * Dump the queue registry (for debugging)
 */
void dump_queue_registry(void) {
    if (!registry_initialized) {
        kernel_printf("Queue registry not initialized\n");
        return;
    }
    
    mutex_lock(&registry_lock);
    
    kernel_printf("===== Message Queue Registry =====\n");
    kernel_printf("Total registered tasks: %u\n", registry_entry_count);
    
    for (uint32_t i = 0; i < registry_entry_count; i++) {
        kernel_printf("Task ID %u: %u queue(s)\n", 
                     task_registry[i].task_id, 
                     task_registry[i].queue_count);
        
        for (uint32_t j = 0; j < task_registry[i].queue_count; j++) {
            char marker = ' ';
            if (j == task_registry[i].recv_queue_idx) {
                marker = 'R'; /* Default receive queue */
            }
            if (j == task_registry[i].send_queue_idx) {
                marker = (marker == 'R') ? 'B' : 'S'; /* B = both default send and receive */
            }
            
            kernel_printf("  [%c] Queue %u: %p\n", 
                         marker, j, task_registry[i].queues[j]);
        }
    }
    
    kernel_printf("=================================\n");
    
    mutex_unlock(&registry_lock);
}

