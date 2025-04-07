/*
 * EdgeX OS - Message Passing System
 *
 * This file implements the message queue and message passing functionality
 * for inter-task communication in EdgeX OS.
 */

#include <edgex/kernel.h>
#include <edgex/memory.h>
#include <edgex/scheduler.h>
#include <edgex/ipc.h>

/* Maximum number of message queues in the system */
#define MAX_MESSAGE_QUEUES 32

/* Message queue flags */
#define MSG_QUEUE_FLAG_PRIVATE      0x01  /* Only owner can receive */
#define MSG_QUEUE_FLAG_NONBLOCKING  0x02  /* Never block on send */

/* Message node in a queue */
typedef struct message_node {
    message_t message;              /* The actual message */
    struct message_node* next;      /* Next message in queue */
} message_node_t;

/* Message queue structure */
struct message_queue {
    ipc_object_header_t header;     /* Common IPC header */
    mutex_t mutex;                  /* Queue access mutex */
    semaphore_t free_slots;         /* Semaphore signaling free space */
    semaphore_t used_slots;         /* Semaphore signaling used space */
    
    pid_t owner;                    /* Owner task PID */
    uint32_t max_messages;          /* Maximum number of messages */
    uint32_t message_count;         /* Current number of messages */
    uint32_t flags;                 /* Queue flags */
    
    message_node_t* head[4];        /* Heads of priority queues */
    message_node_t* tail[4];        /* Tails of priority queues */
    
    /* Statistics */
    uint64_t messages_sent;         /* Total messages sent */
    uint64_t messages_received;     /* Total messages received */
};

/* Pool of all message queues */
static struct message_queue message_queue_pool[MAX_MESSAGE_QUEUES];
static uint32_t message_queue_count = 0;

/* Global message ID counter */
static uint32_t next_message_id = 1;

/* Forward declarations */
static void destroy_message_queue(void* queue_ptr);
static message_node_t* allocate_message_node(void);
static void free_message_node(message_node_t* node);
static int enqueue_message(message_queue_t queue, message_t* message, uint32_t flags);
static int dequeue_message(message_queue_t queue, message_t* message, uint32_t flags);

/*
 * Find a free message queue slot
 */
static message_queue_t find_free_message_queue(void) {
    for (uint32_t i = 0; i < MAX_MESSAGE_QUEUES; i++) {
        if (message_queue_pool[i].header.type == 0) {
            return &message_queue_pool[i];
        }
    }
    return NULL;
}

/*
 * Create a message queue
 */
message_queue_t create_message_queue(const char* name, uint32_t max_messages) {
    if (max_messages == 0 || max_messages > 1000) {
        return NULL;
    }
    
    message_queue_t queue = find_free_message_queue();
    if (!queue) {
        kernel_printf("Failed to allocate message queue: no free slots\n");
        return NULL;
    }
    
    // Initialize message queue
    queue->header.type = IPC_TYPE_MESSAGE_QUEUE;
    strncpy(queue->header.name, name, MAX_IPC_NAME_LENGTH - 1);
    queue->header.name[MAX_IPC_NAME_LENGTH - 1] = '\0';
    queue->header.ref_count = 1;
    queue->header.destroy_fn = destroy_message_queue;
    
    // Create synchronization objects
    char mutex_name[MAX_IPC_NAME_LENGTH];
    char sem_free_name[MAX_IPC_NAME_LENGTH];
    char sem_used_name[MAX_IPC_NAME_LENGTH];
    
    snprintf(mutex_name, MAX_IPC_NAME_LENGTH, "%s_mutex", name);
    snprintf(sem_free_name, MAX_IPC_NAME_LENGTH, "%s_free", name);
    snprintf(sem_used_name, MAX_IPC_NAME_LENGTH, "%s_used", name);
    
    queue->mutex = create_mutex(mutex_name);
    queue->free_slots = create_semaphore(sem_free_name, max_messages);
    queue->used_slots = create_semaphore(sem_used_name, 0);
    
    if (!queue->mutex || !queue->free_slots || !queue->used_slots) {
        // Clean up on failure
        if (queue->mutex) destroy_mutex(queue->mutex);
        if (queue->free_slots) destroy_semaphore(queue->free_slots);
        if (queue->used_slots) destroy_semaphore(queue->used_slots);
        memset(queue, 0, sizeof(struct message_queue));
        return NULL;
    }
    
    queue->owner = get_current_pid();
    queue->max_messages = max_messages;
    queue->message_count = 0;
    queue->flags = 0;
    
    // Initialize priority queues
    for (int i = 0; i < 4; i++) {
        queue->head[i] = NULL;
        queue->tail[i] = NULL;
    }
    
    queue->messages_sent = 0;
    queue->messages_received = 0;
    
    message_queue_count++;
    return queue;
}

/*
 * Destroy a message queue
 */
static void destroy_message_queue(void* queue_ptr) {
    message_queue_t queue = (message_queue_t)queue_ptr;
    
    if (!queue || queue->header.type != IPC_TYPE_MESSAGE_QUEUE) {
        return;
    }
    
    // Lock the queue
    mutex_lock(queue->mutex);
    
    // Free all messages in the queue
    for (int priority = 0; priority < 4; priority++) {
        message_node_t* current = queue->head[priority];
        message_node_t* next;
        
        while (current) {
            next = current->next;
            free_message_node(current);
            current = next;
        }
        
        queue->head[priority] = NULL;
        queue->tail[priority] = NULL;
    }
    
    // Unlock and destroy synchronization objects
    mutex_unlock(queue->mutex);
    destroy_mutex(queue->mutex);
    destroy_semaphore(queue->free_slots);
    destroy_semaphore(queue->used_slots);
    
    // Clear the queue
    memset(queue, 0, sizeof(struct message_queue));
    
    message_queue_count--;
}

/*
 * Destroy a message queue (public API)
 */
void destroy_message_queue(message_queue_t queue) {
    if (!queue || queue->header.type != IPC_TYPE_MESSAGE_QUEUE) {
        return;
    }
    
    // Check if caller owns the queue
    if (queue->owner != get_current_pid()) {
        return;
    }
    
    destroy_message_queue((void*)queue);
}

/*
 * Allocate a message node from the heap
 */
static message_node_t* allocate_message_node(void) {
    message_node_t* node = (message_node_t*)kmalloc(sizeof(message_node_t));
    if (node) {
        memset(node, 0, sizeof(message_node_t));
    }
    return node;
}

/*
 * Free a message node
 */
static void free_message_node(message_node_t* node) {
    if (node) {
        kfree(node);
    }
}

/*
 * Enqueue a message into a queue
 */
static int enqueue_message(message_queue_t queue, message_t* message, uint32_t flags) {
    // Determine the priority (0-3)
    message_priority_t priority = message->header.priority;
    if (priority > MESSAGE_PRIORITY_URGENT) {
        priority = MESSAGE_PRIORITY_NORMAL;
    }
    
    // Allocate a new node
    message_node_t* node = allocate_message_node();
    if (!node) {
        return -1;
    }
    
    // Copy the message
    memcpy(&node->message, message, sizeof(message_t));
    node->next = NULL;
    
    // Lock the queue
    mutex_lock(queue->mutex);
    
    // Add to the appropriate priority queue
    if (queue->tail[priority]) {
        queue->tail[priority]->next = node;
        queue->tail[priority] = node;
    } else {
        queue->head[priority] = queue->tail[priority] = node;
    }
    
    queue->message_count++;
    queue->messages_sent++;
    
    // Unlock the queue
    mutex_unlock(queue->mutex);
    
    // Signal that a message is available
    semaphore_post(queue->used_slots);
    
    return 0;
}

/*
 * Dequeue a message from a queue
 */
static int dequeue_message(message_queue_t queue, message_t* message, uint32_t flags) {
    message_node_t* node = NULL;
    
    // Lock the queue
    mutex_lock(queue->mutex);
    
    // Find the highest priority non-empty queue
    for (int priority = MESSAGE_PRIORITY_URGENT; priority >= MESSAGE_PRIORITY_LOW; priority--) {
        if (queue->head[priority]) {
            // Get the message from the head
            node = queue->head[priority];
            queue->head[priority] = node->next;
            
            // If queue is now empty, update tail
            if (!queue->head[priority]) {
                queue->tail[priority] = NULL;
            }
            
            break;
        }
    }
    
    if (!node) {
        // No messages available
        mutex_unlock(queue->mutex);
        return -1;
    }
    
    // Copy the message
    memcpy(message, &node->message, sizeof(message_t));
    
    queue->message_count--;
    queue->messages_received++;
    
    // Unlock the queue
    mutex_unlock(queue->mutex);
    
    // Free the node
    free_message_node(node);
    
    // Signal that a slot is available
    semaphore_post(queue->free_slots);
    
    return 0;
}

/*
 * Send a message (blocking or non-blocking)
 */
int send_message(message_queue_t queue, message_t* message, uint32_t flags) {
    if (!queue || queue->header.type != IPC_TYPE_MESSAGE_QUEUE || !message) {
        return -1;
    }
    
    // Fill in missing header fields
    message->header.sender = get_current_pid();
    message->header.message_id = next_message_id++;
    message->header.timestamp = get_tick_count();
    
    // Check if we should block or not
    if (flags & MESSAGE_FLAG_BLOCKING) {
        // Wait for a free slot
        if (semaphore_wait(queue->free_slots) != 0) {
            return -1;
        }
    } else {
        // Try to get a free slot without blocking
        if (semaphore_trywait(queue->free_slots) != 0) {
            return -1;
        }
    }
    
    // Enqueue the message
    int result = enqueue_message(queue, message, flags);
    
    // If message is urgent, put it at the front
    if (result == 0 && (flags & MESSAGE_FLAG_URGENT)) {
        // TODO: Implement urgent message priority
    }
    
    // If we need to wait for a reply
    if (result == 0 && (flags & MESSAGE_FLAG_WAIT_REPLY)) {
        // TODO: Implement reply waiting
    }
    
    return result;
}

/*
 * Receive a message (blocking or non-blocking)
 */
int receive_message(message_queue_t queue, message_t* message, uint32_t flags) {
    if (!queue || queue->header.type != IPC_TYPE_MESSAGE_QUEUE || !message) {
        return -1;
    }
    
    // Check if queue is private and caller is not the owner
    if ((queue->flags & MSG_QUEUE_FLAG_PRIVATE) && queue->owner != get_current_pid()) {
        return -1;
    }
    
    // Check if we should block or not
    if (flags & MESSAGE_FLAG_BLOCKING) {
        // Wait for a message
        if (semaphore_wait(queue->used_slots) != 0) {
            return -1;
        }
    } else {
        // Try to get a message without blocking
        if (semaphore_trywait(queue->used_slots) != 0) {
            return -1;
        }
    }
    
    // Dequeue the message
    return dequeue_message(queue, message, flags);
}

/*
 * Reply to a message
 */
int reply_to_message(message_t* original, message_t* reply, uint32_t flags) {
    if (!original || !reply) {
        return -1;
    }
    
    // Set up the reply message
    reply->header.receiver = original->header.sender;
    reply->header.sender = get_current_pid();
    reply->header.message_id = next_message_id++;
    reply->header.type = MESSAGE_TYPE_RESPONSE;
    reply->header.timestamp = get_tick_count();
    
    // TODO: Find the task's receive queue and send the reply
    
    return 0;
}

/*
 * Clean up message queues for a terminated task
 */
void cleanup_task_message_queues(pid_t pid) {
    for (uint32_t i = 0; i < MAX_MESSAGE_QUEUES; i++) {
        message_queue_t queue = &message_queue_pool[i];
        
        if (queue->header.type == IPC_TYPE_MESSAGE_QUEUE && queue->owner == pid) {
            destroy_message_queue(queue);
        }
    }
}

/*
 * Initialize the message system
 */
void init_message_system(void) {
    kernel_printf("Initializing message passing system...\n");
    
    // Initialize message queue pool
    memset(message_queue_pool, 0, sizeof(message_queue_pool));
    message_queue_count = 0;
    next_message_id = 1;
    
    kernel_printf("Message system initialized\n");
}

