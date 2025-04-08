/*
 * EdgeX OS - Message Interface
 *
 * This file defines the interface for the message passing system
 * in EdgeX OS, which allows tasks to communicate with each other.
 */

#ifndef EDGEX_IPC_MESSAGE_H
#define EDGEX_IPC_MESSAGE_H

#include <edgex/ipc/common.h>

/* Maximum size of a message in bytes */
#define MAX_MESSAGE_SIZE     1024

/* Message priority levels */
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

/* Message flags */
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

/* Message types */
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

/* Message header structure */
typedef struct {
    uint32_t id;              /* Message ID */
    uint32_t sender;          /* Sender process ID */
    uint32_t receiver;        /* Receiver process ID */
    uint32_t type;            /* Message type */
    uint32_t priority;        /* Message priority */
    uint32_t flags;           /* Message flags */
    uint32_t size;            /* Size of payload */
    uint32_t reply_id;        /* ID of message this is a reply to (if any) */
} message_header_t;

/* Message structure */
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

/* Message queue handle */
typedef struct message_queue* message_queue_t;

/* Queue lookup modes */
#define QUEUE_LOOKUP_SEND     1  /* Lookup a queue for sending */
#define QUEUE_LOOKUP_RECEIVE  2  /* Lookup a queue for receiving */
#define QUEUE_LOOKUP_ANY      3  /* Any queue accessible to the task */

/* Message queue functions */
/* Message queue functions */
message_queue_t create_message_queue(const char* name, uint32_t max_messages);
message_queue_t create_task_message_queue(const char* name, uint32_t max_messages, uint32_t owner_pid);
void destroy_message_queue(message_queue_t queue);
int send_message(message_queue_t queue, message_t* message, uint32_t flags);
int receive_message(message_queue_t queue, message_t* message, uint32_t flags);
int reply_to_message(message_t* original, message_t* reply, uint32_t flags);

/* Queue lookup functions */
message_queue_t find_task_queue(uint32_t pid, int lookup_mode);
message_queue_t get_current_task_queue(int lookup_mode);

/* Internal functions */
void init_message_subsystem(void);
void cleanup_task_messages(pid_t pid);
void check_message_timeouts(void);
void register_task_queue(uint32_t pid, message_queue_t queue);
void unregister_task_queue(uint32_t pid, message_queue_t queue);
void dump_all_message_queues(void);

#endif /* EDGEX_IPC_MESSAGE_H */

