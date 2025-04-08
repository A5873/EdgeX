/*
 * EdgeX OS - Message Passing Implementation
 *
 * This file implements the message passing system for EdgeX OS,
 * providing inter-process communication via message queues.
 */

#include <edgex/ipc/message.h>
#include <edgex/ipc/mutex.h>
#include <edgex/ipc/semaphore.h>
#include <edgex/kernel.h>
#include <string.h>

/* Size definitions */
#define DEFAULT_MAX_QUEUE_SIZE 64  /* Default size for message queues */
#define MAX_QUEUES 256             /* Maximum number of message queues system-wide */
#define QUEUE_TIMEOUT_MS 5000      /* Default timeout for blocking operations (5 seconds) */

/* Message queue structure */
struct message_queue {
    ipc_object_header_t header;       /* Common IPC object header */
    mutex_t lock;                     /* Mutex to protect queue access */
    semaphore_t msg_available;        /* Semaphore for blocking receive */
    semaphore_t space_available;      /* Semaphore for blocking send */
    uint32_t max_size;                /* Maximum number of messages in queue */
    uint32_t current_size;            /* Current number of messages in queue */
    uint32_t head;                    /* Index of the next message to be read */
    uint32_t tail;                    /* Index of the next slot to write a message */
    uint32_t high_priority_count;     /* Count of high-priority messages */
    uint32_t urgent_priority_count;   /* Count of urgent-priority messages */
    message_t* messages;              /* Circular buffer of messages */
    
    /* Statistics */
    uint64_t messages_sent;           /* Total messages sent through this queue */
    uint64_t messages_received;       /* Total messages received from this queue */
    uint64_t blocked_sends;           /* Number of times a send operation blocked */
    uint64_t blocked_receives;        /* Number of times a receive operation blocked */
    uint64_t dropped_messages;        /* Number of messages dropped (queue full) */
    uint64_t timeouts;                /* Number of timeouts on this queue */
};

/* Global state */
static mutex_t global_message_lock;                  /* Global lock for message subsystem */
static struct message_queue* message_queues[MAX_QUEUES]; /* Array of all message queues */
static uint32_t num_message_queues = 0;              /* Current number of message queues */
static uint32_t next_message_id = 1;                 /* Incrementing message ID counter */
static bool subsystem_initialized = false;           /* Initialization flag */

/* Forward declarations of internal functions */
static void destroy_message_queue_internal(struct message_queue* queue);
static int find_queue_index(message_queue_t queue);
static int insert_message_by_priority(message_queue_t queue, message_t* message);
static message_t* find_response_message(message_queue_t queue, uint32_t original_id, pid_t sender);
static void update_message_statistics(message_queue_t queue, int operation);

/*
 * Initialize the message subsystem
 */
void init_message_subsystem(void) {
    if (!subsystem_initialized) {
        mutex_init(&global_message_lock);
        memset(message_queues, 0, sizeof(message_queues));
        num_message_queues = 0;
        next_message_id = 1;
        subsystem_initialized = true;
        
        /* Update IPC statistics */
        update_ipc_stats(IPC_STAT_OBJECT_CREATED, 1);
        
        log_info("Message subsystem initialized");
    }
}

/*
 * Create a new message queue
 */
    struct message_queue* queue;
    
    /* Check initialization */
    if (!subsystem_initialized) {
        init_message_subsystem();
    }
    
    /* Validate parameters */
    if (name == NULL || strlen(name) >= MAX_IPC_NAME_LENGTH) {
        log_error("Invalid message queue name");
        return NULL;
    }
    
    if (max_messages == 0) {
        max_messages = DEFAULT_MAX_QUEUE_SIZE;
    }
    
    /* Allocate memory for the queue structure */
    queue = (struct message_queue*)kmalloc(sizeof(struct message_queue));
    if (queue == NULL) {
        log_error("Failed to allocate memory for message queue");
        update_ipc_stats(IPC_STAT_OBJECT_CREATED, 0);
        return NULL;
    }
    
    /* Allocate memory for the message buffer */
    queue->messages = (message_t*)kmalloc(sizeof(message_t) * max_messages);
    if (queue->messages == NULL) {
        kfree(queue);
        log_error("Failed to allocate memory for message buffer");
        update_ipc_stats(IPC_STAT_OBJECT_CREATED, 0);
        return NULL;
    }
    
    /* Initialize the queue structure */
    queue->header.type = IPC_TYPE_MESSAGE_QUEUE;
    strncpy(queue->header.name, name, MAX_IPC_NAME_LENGTH - 1);
    queue->header.name[MAX_IPC_NAME_LENGTH - 1] = '\0';
    queue->header.ref_count = 1;
    queue->header.destroy_fn = (void (*)(void*))destroy_message_queue_internal;
    
    /* Initialize synchronization objects */
    if (mutex_init(&queue->lock) != 0) {
        kfree(queue->messages);
        kfree(queue);
        log_error("Failed to initialize message queue mutex");
        update_ipc_stats(IPC_STAT_OBJECT_CREATED, 0);
        return NULL;
    }
    
    if (semaphore_init(&queue->msg_available, 0, max_messages) != 0) {
        mutex_destroy(&queue->lock);
        kfree(queue->messages);
        kfree(queue);
        log_error("Failed to initialize message available semaphore");
        update_ipc_stats(IPC_STAT_OBJECT_CREATED, 0);
        return NULL;
    }
    
    if (semaphore_init(&queue->space_available, max_messages, max_messages) != 0) {
        semaphore_destroy(&queue->msg_available);
        mutex_destroy(&queue->lock);
        kfree(queue->messages);
        kfree(queue);
        log_error("Failed to initialize space available semaphore");
        update_ipc_stats(IPC_STAT_OBJECT_CREATED, 0);
        return NULL;
    }
    
    /* Initialize queue state */
    queue->max_size = max_messages;
    queue->current_size = 0;
    queue->head = 0;
    queue->tail = 0;
    queue->high_priority_count = 0;
    queue->urgent_priority_count = 0;
    
    /* Initialize statistics */
    queue->messages_sent = 0;
    queue->messages_received = 0;
    queue->blocked_sends = 0;
    queue->blocked_receives = 0;
    queue->dropped_messages = 0;
    queue->timeouts = 0;
    
    /* Register the queue in the global registry */
    mutex_lock(&global_message_lock);
    if (num_message_queues < MAX_QUEUES) {
        message_queues[num_message_queues++] = queue;
        update_ipc_stats(IPC_STAT_OBJECT_CREATED, 1);
        mutex_unlock(&global_message_lock);
        log_info("Created message queue '%s' with capacity %d", name, max_messages);
        return queue;
    } else {
        mutex_unlock(&global_message_lock);
        semaphore_destroy(&queue->space_available);
        semaphore_destroy(&queue->msg_available);
        mutex_destroy(&queue->lock);
        kfree(queue->messages);
        kfree(queue);
        log_error("Maximum number of message queues reached");
        update_ipc_stats(IPC_STAT_OBJECT_CREATED, 0);
        return NULL;
    }
}

/*
 * Internal function to destroy a message queue
 */
static void destroy_message_queue_internal(struct message_queue* queue) {
    if (queue == NULL) {
        return;
    }
    
    /* Free all resources */
    semaphore_destroy(&queue->space_available);
    semaphore_destroy(&queue->msg_available);
    mutex_destroy(&queue->lock);
    kfree(queue->messages);
    kfree(queue);
    
    /* Update IPC statistics */
    update_ipc_stats(IPC_STAT_OBJECT_DESTROYED, 1);
    
    log_info("Destroyed message queue '%s'", queue->header.name);
}

/*
 * Destroy a message queue
 */
void destroy_message_queue(message_queue_t queue) {
    int index;
    
    if (queue == NULL) {
        return;
    }
    
    /* Find the queue in the global registry */
    mutex_lock(&global_message_lock);
    index = find_queue_index(queue);
    if (index >= 0) {
        /* Remove from the registry */
        message_queues[index] = message_queues[--num_message_queues];
        message_queues[num_message_queues] = NULL;
    }
    mutex_unlock(&global_message_lock);
    
    /* Now actually destroy the queue */
    if (index >= 0) {
        destroy_message_queue_internal(queue);
    }
}

/*
 * Send a message to a queue
 */
int send_message(message_queue_t queue, message_t* message, uint32_t flags) {
    int result = 0;
    bool is_blocking = (flags & MESSAGE_FLAG_BLOCKING) != 0;
    bool is_urgent = (flags & MESSAGE_FLAG_URGENT) != 0;
    
    if (queue == NULL || message == NULL) {
        return -EINVAL;
    }
    
    /* Fill in message header fields */
    message->header.sender = get_current_task_id();
    message->header.message_id = __atomic_fetch_add(&next_message_id, 1, __ATOMIC_SEQ_CST);
    message->header.timestamp = get_current_time_ms();
    message->header.flags = flags;
    
    /* Ensure size is valid */
    if (message->header.size > MAX_MESSAGE_SIZE) {
        message->header.size = MAX_MESSAGE_SIZE;
    }
    
    /* Try to acquire a slot in the queue */
    if (is_blocking) {
        if (semaphore_wait(&queue->space_available, QUEUE_TIMEOUT_MS) != 0) {
            queue->timeouts++;
            return -ETIMEDOUT;
        }
        queue->blocked_sends++;
    } else if (semaphore_trywait(&queue->space_available) != 0) {
        queue->dropped_messages++;
        return -EAGAIN;
    }
    
    /* Acquire the queue lock */
    mutex_lock(&queue->lock);
    
    /* Insert the message based on priority */
    if (is_urgent) {
        /* Place urgent messages at the head of the queue */
        uint32_t prev_head = (queue->head > 0) ? queue->head - 1 : queue->max_size - 1;
        
        if (queue->current_size < queue->max_size) {
            /* Queue is not full, can insert at head */
            queue->head = prev_head;
            memcpy(&queue->messages[queue->head], message, sizeof(message_t));
            queue->current_size++;
            queue->urgent_priority_count++;
        } else {
            /* Shouldn't happen with semaphore, but just in case */
            result = -EAGAIN;
        }
    } else {
        /* Normal priority-based insertion */
        result = insert_message_by_priority(queue, message);
    }
    
    /* Update statistics */
    if (result == 0) {
        queue->messages_sent++;
        update_message_statistics(queue, 1);
    }
    
    mutex_unlock(&queue->lock);
    
    /* Signal that a message is available */
    if (result == 0) {
        semaphore_post(&queue->msg_available);
    } else {
        /* If we couldn't insert, release the slot we reserved */
        semaphore_post(&queue->space_available);
    }
    
    return result;
}

/*
 * Receive a message from a queue
 */
int receive_message(message_queue_t queue, message_t* message, uint32_t flags) {
    int result = 0;
    bool is_blocking = (flags & MESSAGE_FLAG_BLOCKING) != 0;
    
    if (queue == NULL || message == NULL) {
        return -EINVAL;
    }
    
    /* Try to acquire a message from the queue */
    if (is_blocking) {
        if (semaphore_wait(&queue->msg_available, QUEUE_TIMEOUT_MS) != 0) {
            queue->timeouts++;
            return -ETIMEDOUT;
        }
        queue->blocked_receives++;
    } else if (semaphore_trywait(&queue->msg_available) != 0) {
        return -EAGAIN;
    }
    
    /* Acquire the queue lock */
    mutex_lock(&queue->lock);
    
    /* Get the message from the head of the queue */
    if (queue->current_size > 0) {
        memcpy(message, &queue->messages[queue->head], sizeof(message_t));
        queue->head = (queue->head + 1) % queue->max_size;
        queue->current_size--;
        
        /* Update priority counts */
        if (message->header.priority == MESSAGE_PRIORITY_URGENT) {
            queue->urgent_priority_count--;
        } else if (message->header.priority == MESSAGE_PRIORITY_HIGH) {
            queue->high_priority_count--;
        }
        
        /* Update statistics */
        queue->messages_received++;
        update_message_statistics(queue, 2);
    } else {
        /* Shouldn't happen with semaphore, but just in case */
        result = -EAGAIN;
    }
    
    mutex_unlock(&queue->lock);
    
    /* Signal that space is available in the queue */
    if (result == 0) {
        semaphore_post(&queue->space_available);
    } else {
        /* If we couldn't get a message, release the signal we acquired */
        semaphore_post(&queue->msg_available);
    }
    
    return result;
}

/*
 * Reply to a message
 */
int reply_to_message(message_t* original, message_t* reply, uint32_t flags) {
    if (original == NULL || reply == NULL) {
        return -EINVAL;
    }
    
    /* Set up the reply message */
    reply->header.receiver = original->header.sender;
    reply->header.sender = get_current_task_id();
    reply->header.type = MESSAGE_TYPE_RESPONSE;
    reply->header.priority = MESSAGE_PRIORITY_HIGH;  /* Replies are high priority by default */
    reply->header.message_id = __atomic_fetch_add(&next_message_id, 1, __ATOMIC_SEQ_CST);
    reply->header.timestamp = get_current_time_ms();
    
    /* Link this reply to the original message */
    reply->header.reply_id = original->header.message_id;
    
    /* Find a queue belonging to the target task for receiving */
    message_queue_t target_queue = find_task_queue(original->header.sender, QUEUE_LOOKUP_RECEIVE);
    
    if (target_queue == NULL) {
        /* Fall back to the old lookup method if registry fails */
        mutex_lock(&global_message_lock);
        for (uint32_t i = 0; i < num_message_queues; i++) {
            if (message_queues[i] != NULL) {
                /* Just use the first queue we find - not ideal but better than nothing */
                target_queue = message_queues[i];
                break;
            }
        }
        mutex_unlock(&global_message_lock);
        
        if (target_queue == NULL) {
            return -ENOENT;  /* No suitable queue found */
        }
    }
    
    /* Send the reply */
    return send_message(target_queue, reply, flags);
}

/*
 * Find the index of a queue in the global registry
 */
static int find_queue_index(message_queue_t queue) {
    for (uint32_t i = 0; i < num_message_queues; i++) {
        if (message_queues[i] == queue) {
            return i;
        }
    }
    return -1;  /* Not found */
}

/*
 * Insert a message into the queue based on its priority
 */
static int insert_message_by_priority(message_queue_t queue, message_t* message) {
    /* Check if the queue is full (shouldn't happen with semaphore, but check anyway) */
    if (queue->current_size >= queue->max_size) {
        return -EAGAIN;
    }
    
    /* Default insertion at the tail */
    uint32_t insert_pos = queue->tail;
    
    /* For higher priority messages, we may need to find a better position */
    if (message->header.priority == MESSAGE_PRIORITY_HIGH ||
        message->header.priority == MESSAGE_PRIORITY_URGENT) {
        
        /* Start from the tail and go backwards */
        uint32_t pos = queue->tail;
        uint32_t count = 0;
        
        /* Find the right position based on priority */
        while (count < queue->current_size) {
            uint32_t idx = (pos == 0) ? queue->max_size - 1 : pos - 1;
            
            /* Stop when we find a message with higher or equal priority */
            if (queue->messages[idx].header.priority >= message->header.priority) {
                break;
            }
            
            pos = idx;
            count++;
        }
        
        /* If we found a better position, use it */
        if (count > 0) {
            insert_pos = pos;
        }
        
        /* Update the appropriate counter */
        if (message->header.priority == MESSAGE_PRIORITY_HIGH) {
            queue->high_priority_count++;
        } else if (message->header.priority == MESSAGE_PRIORITY_URGENT) {
            queue->urgent_priority_count++;
        }
    }
    
    /* If inserting in the middle, need to shift elements */
    if (insert_pos != queue->tail) {
        /* Move elements from insert_pos to tail-1 one step forward */
        uint32_t curr = queue->tail;
        while (curr != insert_pos) {
            uint32_t prev = (curr == 0) ? queue->max_size - 1 : curr - 1;
            memcpy(&queue->messages[curr], &queue->messages[prev], sizeof(message_t));
            curr = prev;
        }
    }
    
    /* Insert the message */
    memcpy(&queue->messages[insert_pos], message, sizeof(message_t));
    
    /* Update the tail pointer */
    queue->tail = (queue->tail + 1) % queue->max_size;
    queue->current_size++;
    
    return 0;
}

/*
 * Find a response message in a queue
 */
static message_t* find_response_message(message_queue_t queue, uint32_t original_id, pid_t sender) {
    if (queue->current_size == 0) {
        return NULL;
    }
    
    /* Start at the head and search forward */
    uint32_t pos = queue->head;
    for (uint32_t i = 0; i < queue->current_size; i++) {
        message_t* msg = &queue->messages[pos];
        
        /* Check if this is a response to the original message */
        if (msg->header.type == MESSAGE_TYPE_RESPONSE &&
            msg->header.message_id == original_id &&
            msg->header.sender == sender) {
            return msg;
        }
        
        /* Move to the next message */
        pos = (pos + 1) % queue->max_size;
    }
    
    return NULL;  /* No response found */
}

/*
 * Update message statistics
 */
static void update_message_statistics(message_queue_t queue, int operation) {
    /* Update the global IPC statistics */
    update_ipc_stats(IPC_STAT_OBJECT_CREATED + operation, 1);
}

/*
 * Clean up messages for a terminated task
 */
void cleanup_task_messages(pid_t pid) {
    if (pid <= 0) {
        return;
    }
    
    log_info("Cleaning up messages for terminated task %d", pid);
    
    mutex_lock(&global_message_lock);
    
    /* Go through all message queues */
    for (uint32_t i = 0; i < num_message_queues; i++) {
        struct message_queue* queue = message_queues[i];
        if (queue == NULL) {
            continue;
        }
        
        mutex_lock(&queue->lock);
        
        /* Check if there are any messages to or from this task */
        uint32_t pos = queue->head;
        uint32_t removed = 0;
        
        for (uint32_t j = 0; j < queue->current_size; ) {
            message_t* msg = &queue->messages[pos];
            uint32_t next_pos = (pos + 1) % queue->max_size;
            
            if (msg->header.sender == pid || msg->header.receiver == pid) {
                /* This message involves the terminated task, remove it */
                
                /* If it's not the last message, shift all following messages */
                if (j < queue->current_size - 1) {
                    uint32_t move_pos = pos;
                    uint32_t next_move_pos = next_pos;
                    
                    for (uint32_t k = j + 1; k < queue->current_size; k++) {
                        memcpy(&queue->messages[move_pos], 
                               &queue->messages[next_move_pos], 
                               sizeof(message_t));
                        move_pos = next_move_pos;
                        next_move_pos = (next_move_pos + 1) % queue->max_size;
                    }
                }
                
                /* Update priority counts if needed */
                if (msg->header.priority == MESSAGE_PRIORITY_HIGH) {
                    queue->high_priority_count--;
                } else if (msg->header.priority == MESSAGE_PRIORITY_URGENT) {
                    queue->urgent_priority_count--;
                }
                
                /* Adjust tail pointer */
                queue->tail = (queue->tail > 0) ? queue->tail - 1 : queue->max_size - 1;
                queue->current_size--;
                removed++;
                
                /* Don't increment pos since we moved the next message to this position */
            } else {
                /* Move to next message */
                pos = next_pos;
                j++;
            }
        }
        
        /* If we removed messages, update the semaphores */
        if (removed > 0) {
            /* Reset the semaphores to match current queue state */
            semaphore_destroy(&queue->msg_available);
            semaphore_destroy(&queue->space_available);
            
            semaphore_init(&queue->msg_available, queue->current_size, queue->max_size);
            semaphore_init(&queue->space_available, 
                          queue->max_size - queue->current_size, 
                          queue->max_size);
            
            log_info("Removed %d messages related to terminated task %d from queue '%s'", 
                     removed, pid, queue->header.name);
        }
        
        mutex_unlock(&queue->lock);
    }
    
    mutex_unlock(&global_message_lock);
}

/*
 * Check for message timeouts
 */
void check_message_timeouts(void) {
    uint64_t current_time = get_current_time_ms();
    uint32_t timeouts_processed = 0;
    
    /* Define the timeout threshold (e.g., 30 seconds) */
    const uint64_t timeout_threshold = 30000;  /* 30 seconds in milliseconds */
    
    mutex_lock(&global_message_lock);
    
    /* Go through all message queues */
    for (uint32_t i = 0; i < num_message_queues; i++) {
        struct message_queue* queue = message_queues[i];
        if (queue == NULL) {
            continue;
        }
        
        mutex_lock(&queue->lock);
        
        /* Check for timed-out messages waiting for replies */
        uint32_t pos = queue->head;
        
        for (uint32_t j = 0; j < queue->current_size; j++) {
            message_t* msg = &queue->messages[pos];
            
            /* Check if this message has the WAIT_REPLY flag and has timed out */
            if ((msg->header.flags & MESSAGE_FLAG_WAIT_REPLY) && 
                (current_time - msg->header.timestamp > timeout_threshold)) {
                
                /* This message has timed out, mark it as such */
                /* In a real implementation, we might send a timeout notification */
                /* or handle this in a more sophisticated way */
                msg->header.flags |= (1 << 31);  /* Use high bit as timeout flag */
                timeouts_processed++;
                
                /* Update queue statistics */
                queue->timeouts++;
            }
            
            /* Move to next message */
            pos = (pos + 1) % queue->max_size;
        }
        
        mutex_unlock(&queue->lock);
    }
    
    mutex_unlock(&global_message_lock);
    
    if (timeouts_processed > 0) {
        log_info("Processed %d message timeouts", timeouts_processed);
    }
}

/*
 * Dump information about all message queues
 */
void dump_all_message_queues(void) {
    mutex_lock(&global_message_lock);
    
    log_info("===== Message Queue Dump =====");
    log_info("Total queues: %d", num_message_queues);
    
    for (uint32_t i = 0; i < num_message_queues; i++) {
        struct message_queue* queue = message_queues[i];
        if (queue == NULL) {
            continue;
        }
        
        mutex_lock(&queue->lock);
        
        log_info("Queue %d: '%s'", i, queue->header.name);
        log_info("  Messages: %d/%d (current/max)", queue->current_size, queue->max_size);
        log_info("  Priority messages: %d high, %d urgent", 
                 queue->high_priority_count, queue->urgent_priority_count);
        log_info("  Statistics:");
        log_info("    Sent: %llu, Received: %llu", 
                 queue->messages_sent, queue->messages_received);
        log_info("    Blocked operations: %llu sends, %llu receives", 
                 queue->blocked_sends, queue->blocked_receives);
        log_info("    Dropped: %llu, Timeouts: %llu", 
                 queue->dropped_messages, queue->timeouts);
        
        if (queue->current_size > 0) {
            log_info("  Message details:");
            uint32_t pos = queue->head;
            
            for (uint32_t j = 0; j < queue->current_size; j++) {
                message_t* msg = &queue->messages[pos];
                
                log_info("    [%d] ID: %u, Type: %d, Priority: %d, Size: %u bytes", 
                         j, msg->header.message_id, msg->header.type, 
                         msg->header.priority, msg->header.size);
                log_info("        From: %d, To: %d, Time: %llu ms ago", 
                         msg->header.sender, msg->header.receiver, 
                         get_current_time_ms() - msg->header.timestamp);
                
                /* Move to next message */
                pos = (pos + 1) % queue->max_size;
            }
        }
        
        mutex_unlock(&queue->lock);
    }
    
    log_info("===== End of Message Queue Dump =====");
    mutex_unlock(&global_message_lock);
}
