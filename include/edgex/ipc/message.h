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
#define MAX_MESSAGE_SIZE    1024

/* Message priorities */
typedef enum {
    MESSAGE_PRIORITY_LOW = 0,
    MESSAGE_PRIORITY_NORMAL = 1,
    MESSAGE_PRIORITY_HIGH = 2,
    MESSAGE_PRIORITY_URGENT = 3
} message_priority_t;

/* Message types */
typedef enum {
    MESSAGE_TYPE_NORMAL = 0,    /* Normal user-defined message */
    MESSAGE_TYPE_SIGNAL = 1,    /* Signal message (predefined meaning) */
    MESSAGE_TYPE_RESPONSE = 2,  /* Response to a previous message */
    MESSAGE_TYPE_SYSTEM = 3     /* System message from kernel */
} message_type_t;

/* Message header */
typedef struct {
    pid_t sender;               /* PID of sending task */
    pid_t receiver;             /* PID of receiving task */
    uint32_t message_id;        /* Unique message ID */
    message_type_t type;        /* Message type */
    message_priority_t priority;/* Message priority */
    uint32_t size;              /* Size of the message payload */
    uint32_t flags;             /* Message flags */
    uint64_t timestamp;         /* Timestamp when message was sent */
} message_header_t;

/* Message structure */
typedef struct {
    message_header_t header;    /* Message header */
    uint8_t payload[MAX_MESSAGE_SIZE]; /* Message payload */
} message_t;

/* Message queue handle */
typedef struct message_queue* message_queue_t;

/* Message sending flags */
#define MESSAGE_FLAG_BLOCKING    (1 << 0)  /* Block if queue is full */
#define MESSAGE_FLAG_WAIT_REPLY  (1 << 1)  /* Wait for a reply */
#define MESSAGE_FLAG_URGENT      (1 << 2)  /* Urgent message, goes to front of queue */

/* Message queue functions */
message_queue_t create_message_queue(const char* name, uint32_t max_messages);
void destroy_message_queue(message_queue_t queue);
int send_message(message_queue_t queue, message_t* message, uint32_t flags);
int receive_message(message_queue_t queue, message_t* message, uint32_t flags);
int reply_to_message(message_t* original, message_t* reply, uint32_t flags);

/* Internal functions */
void init_message_subsystem(void);
void cleanup_task_messages(pid_t pid);
void check_message_timeouts(void);
void dump_all_message_queues(void);

#endif /* EDGEX_IPC_MESSAGE_H */

