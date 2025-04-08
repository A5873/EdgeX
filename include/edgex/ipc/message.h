/*
 * EdgeX OS - Message Interface
 *
 * This file defines the interface for the message passing system
 * in EdgeX OS, which allows tasks to communicate with each other.
 * 
 * The message passing system provides a reliable, prioritized, and efficient
 * inter-process communication mechanism for EdgeX OS. It supports:
 *   - Priority-based message queuing
 *   - Synchronous and asynchronous communication
 *   - Message routing between tasks
 *   - Timeouts and non-blocking operations
 *   - Reply tracking for request-response patterns
 *
 * Messages are passed through message queues, which can be created and
 * controlled by tasks. Each task can have multiple message queues, with
 * designated default queues for sending and receiving.
 */

#ifndef EDGEX_IPC_MESSAGE_H
#define EDGEX_IPC_MESSAGE_H

#include <edgex/ipc/common.h>

/* Maximum size of a message in bytes 
 * This defines the maximum payload size that can be sent in a single message.
 * Larger data transfers should use shared memory with message signaling.
 */
#define MAX_MESSAGE_SIZE     1024

/* Message priority levels 
 * The message system supports four priority levels that determine the order 
 * in which messages are delivered when multiple messages are queued.
 * 
 * Higher priority messages are delivered before lower priority ones:
 *   - URGENT: Critical system messages, processed immediately
 *   - HIGH: Important messages that should be processed promptly
 *   - NORMAL: Standard priority for most application messages
 *   - LOW: Background or non-time-sensitive messages
 *
 * When using priority queuing (MSG_FLAG_PRIORITY), messages are strictly ordered
 * by priority. Otherwise, they are processed in FIFO order within each priority level.
 */
#define MSG_PRIORITY_LOW     0
#define MSG_PRIORITY_NORMAL  1
#define MSG_PRIORITY_HIGH    2
#define MSG_PRIORITY_URGENT  3

/* For backward compatibility */
typedef enum {
    MESSAGE_PRIORITY_LOWEST  = MSG_PRIORITY_LOW,
    MESSAGE_PRIORITY_LOW     = MSG_PRIORITY_LOW,
    MESSAGE_PRIORITY_NORMAL  = MSG_PRIORITY_NORMAL,
    MESSAGE_PRIORITY_HIGH    = MSG_PRIORITY_HIGH,
    MESSAGE_PRIORITY_HIGHEST = MSG_PRIORITY_URGENT,
    MESSAGE_PRIORITY_URGENT  = MSG_PRIORITY_URGENT
} message_priority_t;

/* Message flags
 * These flags control the behavior of message sending and receiving operations.
 * Multiple flags can be combined with the bitwise OR operator (|).
 *
 * MSG_FLAG_NONBLOCK: Operation returns immediately instead of blocking.
 *   - For send: Returns -EAGAIN if the queue is full
 *   - For receive: Returns -EAGAIN if no message is available
 *
 * MSG_FLAG_NOWAIT: Similar to NONBLOCK but used for conditional waiting.
 *   - Returns immediately with -EAGAIN if operation would block
 *   - Useful in polling situations or with event-driven architectures
 *
 * MSG_FLAG_PRIORITY: Uses strict priority ordering on the queue.
 *   - Messages are delivered in strict priority order, ignoring FIFO
 *   - Can cause starvation of low-priority messages under heavy load
 *
 * MSG_FLAG_SYNC: Indicates a synchronous request-response pattern.
 *   - Send: Blocks until a reply is received (reply_id will match sent message id)
 *   - Requires the receiver to use reply_to_message() to respond
 *
 * MSG_FLAG_TIMEOUT: Enables timeout mechanism for blocking operations.
 *   - Must specify timeout value in system call arguments
 *   - Returns -ETIMEDOUT if operation doesn't complete within timeout
 *
 * Example:
 *   // Non-blocking send with timeout
 *   send_message(queue, &message, MSG_FLAG_NONBLOCK | MSG_FLAG_TIMEOUT);
 */
#define MSG_FLAG_NONBLOCK    (1 << 0)  /* Non-blocking operation */
#define MSG_FLAG_NOWAIT      (1 << 1)  /* Don't wait if queue is full/empty */
#define MSG_FLAG_PRIORITY    (1 << 2)  /* Use priority queue ordering */
#define MSG_FLAG_SYNC        (1 << 3)  /* Synchronous message (wait for reply) */
#define MSG_FLAG_TIMEOUT     (1 << 4)  /* Allow operation to timeout */

/* Message flags for compatibility with test code */
#define MESSAGE_FLAG_BLOCKING    0
#define MESSAGE_FLAG_NON_BLOCKING MSG_FLAG_NONBLOCK
#define MESSAGE_FLAG_WAIT_REPLY  MSG_FLAG_SYNC
#define MESSAGE_FLAG_URGENT      MSG_FLAG_PRIORITY
#define MESSAGE_FLAG_TIMEOUT     MSG_FLAG_TIMEOUT

/* Message types
 * These constants define the semantic type of a message, which helps
 * receivers determine how to interpret and process the message content.
 *
 * MESSAGE_TYPE_NORMAL: Standard user-defined message
 *   - General purpose message with application-defined semantics
 *   - The most common type for application-to-application communication
 *
 * MESSAGE_TYPE_CONTROL: Control/command message
 *   - Used to send commands or control signals between components
 *   - Can represent state change requests or system signals
 *
 * MESSAGE_TYPE_RESPONSE: Response to a previous message
 *   - Contains a reply to a previously sent message
 *   - The reply_id field references the original message's ID
 *
 * MESSAGE_TYPE_ERROR: Error notification
 *   - Indicates an error condition in response to a previous message
 *   - May include error code and description in the payload
 *
 * MESSAGE_TYPE_SYSTEM: System message from the kernel
 *   - Special messages generated by the kernel or system services
 *   - Used for system notifications, resource management, etc.
 */
#define MESSAGE_TYPE_NORMAL      0
#define MESSAGE_TYPE_CONTROL     1
#define MESSAGE_TYPE_RESPONSE    2
#define MESSAGE_TYPE_ERROR       3
#define MESSAGE_TYPE_SYSTEM      4

/* Legacy message type enum for backward compatibility */
typedef enum {
    MESSAGE_TYPE_NORMAL_ENUM = MESSAGE_TYPE_NORMAL,     /* Normal user-defined message */
    MESSAGE_TYPE_SIGNAL = MESSAGE_TYPE_CONTROL,         /* Signal message (predefined meaning) */
    MESSAGE_TYPE_RESPONSE_ENUM = MESSAGE_TYPE_RESPONSE, /* Response to a previous message */
    MESSAGE_TYPE_SYSTEM_ENUM = MESSAGE_TYPE_SYSTEM      /* System message from kernel */
} message_type_t;

/* Message header structure
 * Contains metadata and routing information for a message.
 * Applications typically don't need to fill all fields manually;
 * the system will populate most fields automatically when sending.
 */
typedef struct {
    uint32_t id;              /* Message ID - auto-assigned by the system */
    uint32_t sender;          /* Sender process ID - auto-populated on send */
    uint32_t receiver;        /* Receiver process ID - must be set by sender */
    uint32_t type;            /* Message type - see MESSAGE_TYPE_* constants */
    uint32_t priority;        /* Message priority - see MSG_PRIORITY_* constants */
    uint32_t flags;           /* Message flags - see MSG_FLAG_* constants */
    uint32_t size;            /* Size of payload in bytes - must match actual payload */
    uint32_t reply_id;        /* ID of message this is a reply to (if any) */
} message_header_t;

/* Message structure
 * Complete message structure including header metadata and payload data.
 * This is the primary structure used for all message operations.
 *
 * The union allows direct access to header fields without using the header
 * structure, making it more convenient for common operations.
 *
 * Usage example:
 *   message_t msg;
 *   
 *   // Initialize message
 *   memset(&msg, 0, sizeof(message_t));
 *   
 *   // Set fields directly
 *   msg.receiver = target_pid;
 *   msg.type = MESSAGE_TYPE_NORMAL;
 *   msg.priority = MSG_PRIORITY_NORMAL;
 *   
 *   // Set payload
 *   msg.size = snprintf(msg.payload, MAX_MESSAGE_SIZE, "Hello, PID %d", target_pid);
 *   
 *   // Send message
 *   send_message(queue, &msg, 0);
 */
typedef struct {
    union {
        message_header_t header;  /* Message header */
        struct {
            uint32_t id;              /* Message ID */
            uint32_t sender;          /* Sender process ID */
            uint32_t receiver;        /* Receiver process ID */
            uint32_t type;            /* Message type */
            uint32_t priority;        /* Message priority */
            uint32_t flags;           /* Message flags */
            uint32_t size;            /* Size of payload */
            uint32_t reply_id;        /* ID of message this is a reply to (if any) */
        };
    };
    uint64_t timestamp;       /* Creation timestamp */
    char payload[MAX_MESSAGE_SIZE]; /* Message payload */
} message_t;

/* Message queue handle 
 * Opaque handle to a message queue used for all queue operations.
 * The actual queue structure is maintained by the kernel and not
 * directly accessible to user tasks.
 */
typedef struct message_queue* message_queue_t;

/* Queue lookup modes
 * These constants define the lookup modes when searching for a task's queue.
 * Used with find_task_queue() and get_current_task_queue() functions.
 *
 * QUEUE_LOOKUP_SEND: Find a queue suitable for sending messages to a task
 *   - Returns the task's designated receiving queue
 *   - This is what you typically want when sending a message to another task
 *
 * QUEUE_LOOKUP_RECEIVE: Find a queue suitable for receiving messages from a task
 *   - Returns the task's designated sending queue
 *   - Use this to find where a specific task will send its messages
 *
 * QUEUE_LOOKUP_ANY: Find any queue associated with the task
 *   - Returns the first queue found for the specified task
 *   - Less specific, but useful when exact queue type doesn't matter
 */
#define QUEUE_LOOKUP_SEND     1  /* Lookup a queue for sending */
#define QUEUE_LOOKUP_RECEIVE  2  /* Lookup a queue for receiving */
#define QUEUE_LOOKUP_ANY      3  /* Any queue accessible to the task */

/* Message queue functions */
/**
 * Create a new message queue
 *
 * @param name          Unique name for the queue (used for debugging and lookups)
 * @param max_messages  Maximum number of messages the queue can hold
 *
 * @return Handle to the new message queue or NULL on failure
 *
 * This function creates a standard message queue owned by the current task.
 * The queue is automatically registered with the task registry.
 *
 * Example:
 *   message_queue_t my_queue = create_message_queue("task1_main_queue", 64);
 *   if (my_queue == NULL) {
 *       // Handle error
 *   }
 */
message_queue_t create_message_queue(const char* name, uint32_t max_messages);

/**
 * Create a message queue for a specific task
 *
 * @param name          Unique name for the queue
 * @param max_messages  Maximum number of messages the queue can hold
 * @param owner_pid     PID of the task that will own this queue
 *
 * @return Handle to the new message queue or NULL on failure
 *
 * This function creates a message queue owned by the specified task rather than
 * the current task. This is useful for system services that manage message queues
 * on behalf of other tasks.
 *
 * The queue is automatically registered with the task registry for the specified owner.
 *
 * Example:
 *   // Create a queue for task with PID 123
 *   message_queue_t task_queue = create_task_message_queue("worker_queue", 32, 123);
 *   if (task_queue == NULL) {
 *       // Handle error
 *   }
 */
message_queue_t create_task_message_queue(const char* name, uint32_t max_messages, uint32_t owner_pid);

/**
 * Destroy a message queue
 *
 * @param queue  Handle to the queue to destroy
 *
 * This function destroys a message queue, freeing all associated resources.
 * Any pending messages in the queue are discarded. The queue handle becomes
 * invalid after this call and should not be used again.
 *
 * The queue is automatically unregistered from the task registry.
 *
 * Example:
 *   destroy_message_queue(my_queue);
 *   my_queue = NULL;  // Good practice to avoid dangling pointers
 */
void destroy_message_queue(message_queue_t queue);

/**
 * Send a message to a queue
 *
 * @param queue    Handle to the destination queue
 * @param message  Pointer to the message to send
 * @param flags    Flags controlling the send operation (see MSG_FLAG_* constants)
 *
 * @return 0 on success, negative error code on failure:
 *         -EINVAL: Invalid parameters
 *         -ENOMEM: Queue is full and MSG_FLAG_NONBLOCK was specified
 *         -ETIMEDOUT: Operation timed out (with MSG_FLAG_TIMEOUT)
 *         -EPIPE: Queue was destroyed while waiting
 *
 * This function sends a message to the specified queue. The caller must set
 * the message's receiver field to the target task's PID. Other header fields
 * like sender, id, and timestamp are automatically populated by the system.
 *
 * For synchronous messages (MSG_FLAG_SYNC), this function will block until
 * a reply is received or the operation times out.
 *
 * Example (asynchronous send):
 *   message_t msg;
 *   memset(&msg, 0, sizeof(message_t));
 *   msg.receiver = target_pid;
 *   msg.type = MESSAGE_TYPE_NORMAL;
 *   msg.priority = MSG_PRIORITY_NORMAL;
 *   msg.size = strlen("Hello") + 1;
 *   strcpy(msg.payload, "Hello");
 *   
 *   int result = send_message(queue, &msg, 0);
 *   if (result < 0) {
 *       // Handle error
 *   }
 *
 * Example (synchronous request-response):
 *   message_t request, response;
 *   memset(&request, 0, sizeof(message_t));
 *   request.receiver = server_pid;
 *   request.type = MESSAGE_TYPE_NORMAL;
 *   request.priority = MSG_PRIORITY_NORMAL;
 *   request.size = strlen("GetTime") + 1;
 *   strcpy(request.payload, "GetTime");
 *   
 *   int result = send_message(server_queue, &request, MSG_FLAG_SYNC);
 *   if (result == 0) {
 *       // Response is now in 'request' with the reply data
 *       printf("Server time: %s\n", request.payload);
 *   }
 */
int send_message(message_queue_t queue, message_t* message, uint32_t flags);

/**
 * Receive a message from a queue
 *
 * @param queue    Handle to the source queue
 * @param message  Pointer to a message structure to store the received message
 * @param flags    Flags controlling the receive operation (see MSG_FLAG_* constants)
 *
 * @return 0 on success, negative error code on failure:
 *         -EINVAL: Invalid parameters
 *         -EAGAIN: No messages available and MSG_FLAG_NONBLOCK was specified
 *         -ETIMEDOUT: Operation timed out (with MSG_FLAG_TIMEOUT)
 *         -EPIPE: Queue was destroyed while waiting
 *
 * This function receives a message from the specified queue. If no messages are
 * available, the behavior depends on the flags:
 * - With no flags (blocking mode), it waits until a message arrives
 * - With MSG_FLAG_NONBLOCK, it returns -EAGAIN immediately if no messages
 * - With MSG_FLAG_TIMEOUT, it waits for the specified timeout duration
 *
 * When using MSG_FLAG_PRIORITY, messages are delivered in strict priority order.
 * Otherwise, they are delivered in FIFO order within each priority level.
 *
 * Example (blocking receive):
 *   message_t msg;
 *   int result = receive_message(my_queue, &msg, 0);
 *   if (result == 0) {
 *       printf("Received message from PID %u: %s\n", msg.sender, msg.payload);
 *   }
 *
 * Example (non-blocking receive):
 *   message_t msg;
 *   int result = receive_message(my_queue, &msg, MSG_FLAG_NONBLOCK);
 *   if (result == 0) {
 *       // Process message
 *   } else if (result == -EAGAIN) {
 *       // No messages available, continue with other work
 *   } else {
 *       // Handle other errors
 *   }
 */
int receive_message(message_queue_t queue, message_t* message, uint32_t flags);

/**
 * Reply to a received message
 *
 * @param original  Pointer to the original message being replied to
 * @param reply     Pointer to the reply message
 * @param flags     Flags controlling the reply operation (see MSG_FLAG_* constants)
 *
 * @return 0 on success, negative error code on failure:
 *         -EINVAL: Invalid parameters
 *         -ENOENT: No suitable queue found for the target task
 *         Other errors from send_message() may also be returned
 *
 * This function simplifies replying to a received message by automatically setting
 * the appropriate header fields in the reply message. It sets:
 * - receiver = original sender
 * - type = MESSAGE_TYPE_RESPONSE
 * - priority = MESSAGE_PRIORITY_HIGH
 * - reply_id = original message ID
 *
 * This function is especially useful for implementing request-response patterns
 * when the sender used MSG_FLAG_SYNC.
 *
 * Example:
 *   // Receive a request
 *   message_t request, response;
 *   receive_message(server_queue, &request, 0);
 *   
 *   // Process the request and prepare response
 *   memset(&response, 0, sizeof(message_t));
 *   response.size = strlen("Request processed") + 1;
 *   strcpy(response.payload, "Request processed");
 *   
 *   // Send the reply
 *   reply_to_message(&request, &response, 0);
 */
int reply_to_message(message_t* original, message_t* reply, uint32_t flags);
/* Queue lookup functions */
/**
 * Find a message queue associated with a specific task
 *
 * @param pid          PID of the task whose queue to find
 * @param lookup_mode  Queue lookup mode (QUEUE_LOOKUP_SEND, QUEUE_LOOKUP_RECEIVE, or QUEUE_LOOKUP_ANY)
 *
 * @return Handle to the found queue or NULL if no matching queue exists
 *
 * This function searches the queue registry for a queue associated with the specified task.
 * The lookup_mode parameter determines which queue to return when multiple queues exist:
 * - QUEUE_LOOKUP_SEND: Returns the queue suitable for sending messages to the task
 * - QUEUE_LOOKUP_RECEIVE: Returns the queue suitable for receiving messages from the task
 * - QUEUE_LOOKUP_ANY: Returns any queue associated with the task
 *
 * Example:
 *   // Find the queue to send messages to task with PID 123
 *   message_queue_t target_queue = find_task_queue(123, QUEUE_LOOKUP_SEND);
 *   if (target_queue != NULL) {
 *       // Send a message to the task
 *       message_t msg;
 *       // ... setup message ...
 *       send_message(target_queue, &msg, 0);
 *   }
 */
message_queue_t find_task_queue(uint32_t pid, int lookup_mode);

/**
 * Get a message queue associated with the current task
 *
 * @param lookup_mode  Queue lookup mode (QUEUE_LOOKUP_SEND, QUEUE_LOOKUP_RECEIVE, or QUEUE_LOOKUP_ANY)
 *
 * @return Handle to the found queue or NULL if no matching queue exists
 *
 * This function is a convenience wrapper around find_task_queue() that uses the
 * current task's PID automatically.
 *
 * Example:
 *   // Get the current task's receiving queue
 *   message_queue_t my_recv_queue = get_current_task_queue(QUEUE_LOOKUP_RECEIVE);
 *   if (my_recv_queue != NULL) {
 *       // Wait for incoming messages
 *       message_t msg;
 *       receive_message(my_recv_queue, &msg, 0);
 *   }
 */
message_queue_t get_current_task_queue(int lookup_mode);

/* Internal functions */
/**
 * Initialize the message subsystem
 *
 * This function initializes the message passing subsystem.
 * It is automatically called during system startup and should not be
 * called directly by application code.
 */
void init_message_subsystem(void);

/**
 * Clean up message resources for a terminated task
 *
 * @param pid  PID of the terminated task
 *
 * This function is called by the task scheduler when a task terminates.
 * It cleans up all message queues and pending messages associated with the task.
 * Application code should not call this function directly.
 */
void cleanup_task_messages(pid_t pid);

/**
 * Check for message timeouts
 *
 * This function is called periodically by the system to check for and
 * handle timed-out message operations. It is not meant to be called
 * directly by application code.
 */
void check_message_timeouts(void);

/**
 * Register a queue with a task
 *
 * @param pid    PID of the task
 * @param queue  Queue to register
 *
 * This function registers a queue with a task in the queue registry.
 * It is automatically called by create_message_queue() and should not be
 * called directly unless implementing custom queue management.
 */
void register_task_queue(uint32_t pid, message_queue_t queue);

/**
 * Unregister a queue from a task
 *
 * @param pid    PID of the task
 * @param queue  Queue to unregister
 *
 * This function removes a queue from a task's registry entries.
 * It is automatically called by destroy_message_queue() and should not be
 * called directly unless implementing custom queue management.
 */
void unregister_task_queue(uint32_t pid, message_queue_t queue);

/**
 * Dump information about all message queues
 *
 * This function prints debug information about all message queues in the system
 * to the system log. It is useful for debugging and diagnostics.
 */
void dump_all_message_queues(void);

#endif /* EDGEX_IPC_MESSAGE_H */

