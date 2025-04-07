/*
 * EdgeX OS - Message Passing System Unit Tests
 *
 * This file contains comprehensive unit tests for the message passing
 * system implementation, testing all major functionality including queue
 * creation/destruction, message sending/receiving, priority handling, and
 * error case handling.
 */

#include <edgex/ipc/message.h>
#include <edgex/ipc/mutex.h>
#include <edgex/ipc/semaphore.h>
#include <edgex/kernel.h>
#include <edgex/test/framework.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>

/* Test framework declarations */
#define TEST_PASSED 0
#define TEST_FAILED 1

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("ASSERTION FAILED at %s:%d: %s\n", __FILE__, __LINE__, message); \
            return TEST_FAILED; \
        } \
    } while (0)

#define TEST_ASSERT_EQUAL(expected, actual, message) \
    do { \
        if ((expected) != (actual)) { \
            printf("ASSERTION FAILED at %s:%d: %s (expected %d, got %d)\n", \
                __FILE__, __LINE__, message, (int)(expected), (int)(actual)); \
            return TEST_FAILED; \
        } \
    } while (0)

#define TEST_ASSERT_STRING_EQUAL(expected, actual, message) \
    do { \
        if (strcmp((expected), (actual)) != 0) { \
            printf("ASSERTION FAILED at %s:%d: %s (expected '%s', got '%s')\n", \
                __FILE__, __LINE__, message, (expected), (actual)); \
            return TEST_FAILED; \
        } \
    } while (0)

#define TEST_ASSERT_NOT_NULL(ptr, message) \
    do { \
        if ((ptr) == NULL) { \
            printf("ASSERTION FAILED at %s:%d: %s (expected non-NULL)\n", \
                __FILE__, __LINE__, message); \
            return TEST_FAILED; \
        } \
    } while (0)

#define TEST_RUN(test_func) \
    do { \
        printf("Running test: %s\n", #test_func); \
        int result = test_func(); \
        if (result == TEST_PASSED) { \
            printf("PASSED: %s\n", #test_func); \
            test_passed++; \
        } else { \
            printf("FAILED: %s\n", #test_func); \
            test_failed++; \
        } \
        total_tests++; \
    } while (0)

/* Global variables for multi-threaded tests */
static message_queue_t g_queue = NULL;
static int g_sender_count = 0;
static int g_receiver_count = 0;
static bool g_test_running = false;

/* Test Setup and Teardown */
static void test_setup(void) {
    /* Initialize the message subsystem */
    init_message_subsystem();
    g_queue = NULL;
}

static void test_cleanup(void) {
    /* Cleanup any remaining queues */
    if (g_queue != NULL) {
        destroy_message_queue(g_queue);
        g_queue = NULL;
    }
    
    /* Additional cleanup for any other global state if needed */
    g_test_running = false;
}

/* Helper functions */
static void fill_test_message(message_t* message, const char* content, message_type_t type, 
                            message_priority_t priority, pid_t receiver) {
    /* Clear the message structure */
    memset(message, 0, sizeof(message_t));
    
    /* Set up the header */
    message->header.type = type;
    message->header.priority = priority;
    message->header.receiver = receiver;
    
    /* Set the payload */
    if (content != NULL) {
        size_t len = strlen(content);
        if (len > MAX_MESSAGE_SIZE) {
            len = MAX_MESSAGE_SIZE;
        }
        
        memcpy(message->payload, content, len);
        message->header.size = len;
    } else {
        message->header.size = 0;
    }
}

/* Thread worker functions for multi-threaded tests */
static void* sender_thread(void* arg) {
    message_queue_t queue = (message_queue_t)arg;
    char buffer[128];
    
    while (g_test_running) {
        /* Create a test message */
        message_t message;
        snprintf(buffer, sizeof(buffer), "Message from sender thread %lu", pthread_self());
        fill_test_message(&message, buffer, MESSAGE_TYPE_NORMAL, 
                          MESSAGE_PRIORITY_NORMAL, getpid());
        
        /* Send the message */
        if (send_message(queue, &message, MESSAGE_FLAG_BLOCKING) == 0) {
            __atomic_fetch_add(&g_sender_count, 1, __ATOMIC_SEQ_CST);
        }
        
        /* Small delay to avoid overwhelming the system */
        usleep(10000); /* 10ms */
    }
    
    return NULL;
}

static void* receiver_thread(void* arg) {
    message_queue_t queue = (message_queue_t)arg;
    
    while (g_test_running) {
        /* Receive a message */
        message_t message;
        if (receive_message(queue, &message, MESSAGE_FLAG_BLOCKING) == 0) {
            __atomic_fetch_add(&g_receiver_count, 1, __ATOMIC_SEQ_CST);
        }
        
        /* Small delay to avoid overwhelming the system */
        usleep(5000); /* 5ms */
    }
    
    return NULL;
}

/*
 * BASIC TESTS
 */

/* Test queue creation and destruction */
int test_queue_create_destroy(void) {
    const char* queue_name = "test_queue";
    uint32_t max_messages = 10;
    
    /* Create a queue */
    message_queue_t queue = create_message_queue(queue_name, max_messages);
    TEST_ASSERT_NOT_NULL(queue, "Failed to create message queue");
    
    /* Destroy the queue */
    destroy_message_queue(queue);
    
    /* Create another queue to verify we can reuse the name */
    queue = create_message_queue(queue_name, max_messages);
    TEST_ASSERT_NOT_NULL(queue, "Failed to recreate message queue with same name");
    
    /* Cleanup */
    destroy_message_queue(queue);
    
    return TEST_PASSED;
}

/* Test creating a queue with invalid parameters */
int test_queue_create_invalid(void) {
    /* Test with NULL name */
    message_queue_t queue = create_message_queue(NULL, 10);
    TEST_ASSERT(queue == NULL, "Should fail to create queue with NULL name");
    
    /* Test with very long name (exceeding MAX_IPC_NAME_LENGTH) */
    char long_name[256];
    memset(long_name, 'A', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = '\0';
    
    queue = create_message_queue(long_name, 10);
    TEST_ASSERT(queue == NULL, "Should fail to create queue with too long name");
    
    /* Test with zero max_messages (should use default) */
    queue = create_message_queue("zero_size_queue", 0);
    TEST_ASSERT_NOT_NULL(queue, "Failed to create queue with default size");
    destroy_message_queue(queue);
    
    return TEST_PASSED;
}

/* Test basic message sending */
int test_basic_send_receive(void) {
    const char* queue_name = "test_send_receive";
    const char* test_content = "Test message content";
    
    /* Create a queue */
    message_queue_t queue = create_message_queue(queue_name, 10);
    TEST_ASSERT_NOT_NULL(queue, "Failed to create message queue");
    
    /* Create a test message */
    message_t send_msg;
    fill_test_message(&send_msg, test_content, MESSAGE_TYPE_NORMAL, 
                      MESSAGE_PRIORITY_NORMAL, getpid());
    
    /* Send the message */
    int result = send_message(queue, &send_msg, 0);
    TEST_ASSERT_EQUAL(0, result, "Failed to send message");
    
    /* Receive the message */
    message_t recv_msg;
    result = receive_message(queue, &recv_msg, 0);
    TEST_ASSERT_EQUAL(0, result, "Failed to receive message");
    
    /* Verify the content */
    TEST_ASSERT_EQUAL(send_msg.header.size, recv_msg.header.size, 
                       "Message size mismatch");
    TEST_ASSERT(memcmp(send_msg.payload, recv_msg.payload, 
                         send_msg.header.size) == 0, 
                 "Message content mismatch");
    
    /* Cleanup */
    destroy_message_queue(queue);
    
    return TEST_PASSED;
}

/* Test blocking send and receive */
int test_blocking_send_receive(void) {
    const char* queue_name = "test_blocking";
    const char* test_content = "Blocking test message";
    
    /* Create a small queue */
    message_queue_t queue = create_message_queue(queue_name, 1);
    TEST_ASSERT_NOT_NULL(queue, "Failed to create message queue");
    
    /* Fill the queue */
    message_t send_msg;
    fill_test_message(&send_msg, test_content, MESSAGE_TYPE_NORMAL, 
                      MESSAGE_PRIORITY_NORMAL, getpid());
    
    int result = send_message(queue, &send_msg, 0);
    TEST_ASSERT_EQUAL(0, result, "Failed to send first message");
    
    /* Try non-blocking send (should fail since queue is full) */
    result = send_message(queue, &send_msg, 0);
    TEST_ASSERT(result != 0, "Non-blocking send should fail when queue is full");
    
    /* Receive a message to make space */
    message_t recv_msg;
    result = receive_message(queue, &recv_msg, 0);
    TEST_ASSERT_EQUAL(0, result, "Failed to receive message");
    
    /* Now send should succeed */
    result = send_message(queue, &send_msg, 0);
    TEST_ASSERT_EQUAL(0, result, "Failed to send message after receiving");
    
    /* Cleanup */
    destroy_message_queue(queue);
    
    return TEST_PASSED;
}

/* Test message priority handling */
int test_message_priority(void) {
    const char* queue_name = "test_priority";
    
    /* Create a queue */
    message_queue_t queue = create_message_queue(queue_name, 10);
    TEST_ASSERT_NOT_NULL(queue, "Failed to create message queue");
    
    /* Send messages with different priorities */
    message_t low_msg, normal_msg, high_msg, urgent_msg;
    fill_test_message(&low_msg, "Low priority", MESSAGE_TYPE_NORMAL, 
                      MESSAGE_PRIORITY_LOW, getpid());
    fill_test_message(&normal_msg, "Normal priority", MESSAGE_TYPE_NORMAL, 
                      MESSAGE_PRIORITY_NORMAL, getpid());
    fill_test_message(&high_msg, "High priority", MESSAGE_TYPE_NORMAL, 
                      MESSAGE_PRIORITY_HIGH, getpid());
    fill_test_message(&urgent_msg, "Urgent priority", MESSAGE_TYPE_NORMAL, 
                      MESSAGE_PRIORITY_URGENT, getpid());
    
    /* Send in reverse priority order */
    send_message(queue, &low_msg, 0);
    send_message(queue, &normal_msg, 0);
    send_message(queue, &high_msg, 0);
    send_message(queue, &urgent_msg, MESSAGE_FLAG_URGENT); /* Use URGENT flag for urgent message */
    
    /* Receive messages - should come in priority order */
    message_t recv_msg;
    
    /* First should be urgent */
    receive_message(queue, &recv_msg, 0);
    TEST_ASSERT_EQUAL(MESSAGE_PRIORITY_URGENT, recv_msg.header.priority, 
                       "First message should be URGENT priority");
    
    /* Second should be high */
    receive_message(queue, &recv_msg, 0);
    TEST_ASSERT_EQUAL(MESSAGE_PRIORITY_HIGH, recv_msg.header.priority, 
                       "Second message should be HIGH priority");
    
    /* Third should be normal */
    receive_message(queue, &recv_msg, 0);
    TEST_ASSERT_EQUAL(MESSAGE_PRIORITY_NORMAL, recv_msg.header.priority, 
                       "Third message should be NORMAL priority");
    
    /* Fourth should be low */
    receive_message(queue, &recv_msg, 0);
    TEST_ASSERT_EQUAL(MESSAGE_PRIORITY_LOW, recv_msg.header.priority, 
                       "Fourth message should be LOW priority");
    
    /* Cleanup */
    destroy_message_queue(queue);
    
    return TEST_PASSED;
}

/* Test message reply functionality */
int test_message_reply(void) {
    const char* queue_name = "test_reply";
    const char* original_content = "Original message";
    const char* reply_content = "Reply message";
    
    /* Create a queue */
    message_queue_t queue = create_message_queue(queue_name, 10);
    TEST_ASSERT_NOT_NULL(queue, "Failed to create message queue");
    
    /* Send original message */
    message_t original_msg;
    fill_test_message(&original_msg, original_content, MESSAGE_TYPE_NORMAL, 
                      MESSAGE_PRIORITY_NORMAL, getpid());
    
    int result = send_message(queue, &original_msg, MESSAGE_FLAG_WAIT_REPLY);
    TEST_ASSERT_EQUAL(0, result, "Failed to send original message");
    
    /* Receive the message */
    message_t received_msg;
    result = receive_message(queue, &received_msg, 0);
    TEST_ASSERT_EQUAL(0, result, "Failed to receive original message");
    
    /* Create and send reply */
    message_t reply_msg;
    fill_test_message(&reply_msg, reply_content, MESSAGE_TYPE_RESPONSE, 
                      MESSAGE_PRIORITY_HIGH, received_msg.header.sender);
    
    result = reply_to_message(&received_msg, &reply_msg, 0);
    TEST_ASSERT_EQUAL(0, result, "Failed to send reply message");
    
    /* Verify that a reply was sent (by receiving it) */
    message_t reply_received;
    result = receive_message(queue, &reply_received, 0);
    TEST_ASSERT_EQUAL(0, result, "Failed to receive reply message");
    
    /* Verify the reply content */
    TEST_ASSERT_EQUAL(reply_msg.header.size, reply_received.header.size, 
                     "Reply message size mismatch");
    TEST_ASSERT(memcmp(reply_msg.payload, reply_received.payload, 
                       reply_msg.header.size) == 0, 
               "Reply message content mismatch");
    TEST_ASSERT_EQUAL(MESSAGE_TYPE_RESPONSE, reply_received.header.type,
                     "Reply should have RESPONSE type");
    
    /* Cleanup */
    destroy_message_queue(queue);
    
    return TEST_PASSED;
}

/* Test message timeout behavior */
int test_message_timeout(void) {
    const char* queue_name = "test_timeout";
    
    /* Create a queue */
    message_queue_t queue = create_message_queue(queue_name, 10);
    TEST_ASSERT_NOT_NULL(queue, "Failed to create message queue");
    
    /* Try to receive from an empty queue with timeout */
    message_t message;
    int result = receive_message(queue, &message, MESSAGE_FLAG_BLOCKING);
    
    /* Should fail with ETIMEDOUT since the default timeout should apply */
    TEST_ASSERT_EQUAL(-ETIMEDOUT, result, "Expected timeout on empty queue receive");
    
    /* Fill the queue */
    for (int i = 0; i < 10; i++) {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "Test message %d", i);
        
        message_t send_msg;
        fill_test_message(&send_msg, buffer, MESSAGE_TYPE_NORMAL,
                         MESSAGE_PRIORITY_NORMAL, getpid());
        send_message(queue, &send_msg, 0);
    }
    
    /* Try to send to a full queue with timeout */
    message_t send_msg;
    fill_test_message(&send_msg, "Overflow message", MESSAGE_TYPE_NORMAL,
                     MESSAGE_PRIORITY_NORMAL, getpid());
    
    result = send_message(queue, &send_msg, MESSAGE_FLAG_BLOCKING);
    
    /* Should fail with ETIMEDOUT since the queue is full */
    TEST_ASSERT_EQUAL(-ETIMEDOUT, result, "Expected timeout on full queue send");
    
    /* Verify that message timeout checking works */
    /* In our test environment, we can simulate this by manually calling timeout check */
    check_message_timeouts();
    
    /* Cleanup */
    destroy_message_queue(queue);
    
    return TEST_PASSED;
}

/* Test queue cleanup for terminated tasks */
int test_task_cleanup(void) {
    const char* queue_name = "test_cleanup";
    
    /* Create a queue */
    message_queue_t queue = create_message_queue(queue_name, 10);
    TEST_ASSERT_NOT_NULL(queue, "Failed to create message queue");
    
    /* Fill the queue with messages from different "tasks" */
    for (int i = 0; i < 5; i++) {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "Message from task %d", i + 100);
        
        message_t send_msg;
        fill_test_message(&send_msg, buffer, MESSAGE_TYPE_NORMAL,
                         MESSAGE_PRIORITY_NORMAL, getpid());
        
        /* Set up fake sender/receiver IDs */
        send_msg.header.sender = 100 + i;  /* Simulate different sender PIDs */
        send_msg.header.receiver = 200;    /* Common receiver */
        
        send_message(queue, &send_msg, 0);
    }
    
    /* Should have 5 messages now */
    
    /* Simulate cleanup for task ID 102 */
    cleanup_task_messages(102);
    
    /* Should have 4 messages now - verify by receiving and checking */
    int msg_count = 0;
    message_t recv_msg;
    
    while (receive_message(queue, &recv_msg, 0) == 0) {
        /* Check that none of the remaining messages are from the cleaned-up task */
        TEST_ASSERT(recv_msg.header.sender != 102, 
                  "Message from terminated task should not be present");
        msg_count++;
    }
    
    TEST_ASSERT_EQUAL(4, msg_count, "Should have 4 messages after cleanup");
    
    /* Cleanup */
    destroy_message_queue(queue);
    
    return TEST_PASSED;
}

/* Test concurrent operations (stress test) */
int test_concurrent_operations(void) {
    const char* queue_name = "test_concurrent";
    const int num_senders = 3;
    const int num_receivers = 2;
    const int test_duration_ms = 1000;  /* Run for 1 second */
    
    /* Reset counters */
    g_sender_count = 0;
    g_receiver_count = 0;
    
    /* Create a queue */
    g_queue = create_message_queue(queue_name, 64);
    TEST_ASSERT_NOT_NULL(g_queue, "Failed to create message queue");
    
    /* Start the test */
    g_test_running = true;
    
    /* Create sender threads */
    pthread_t sender_threads[num_senders];
    for (int i = 0; i < num_senders; i++) {
        if (pthread_create(&sender_threads[i], NULL, sender_thread, g_queue) != 0) {
            printf("Failed to create sender thread %d\n", i);
            g_test_running = false;
            destroy_message_queue(g_queue);
            g_queue = NULL;
            return TEST_FAILED;
        }
    }
    
    /* Create receiver threads */
    pthread_t receiver_threads[num_receivers];
    for (int i = 0; i < num_receivers; i++) {
        if (pthread_create(&receiver_threads[i], NULL, receiver_thread, g_queue) != 0) {
            printf("Failed to create receiver thread %d\n", i);
            g_test_running = false;
            destroy_message_queue(g_queue);
            g_queue = NULL;
            return TEST_FAILED;
        }
    }
    
    /* Let the test run for a while */
    usleep(test_duration_ms * 1000);
    
    /* Stop the test */
    g_test_running = false;
    
    /* Wait for all threads to complete */
    for (int i = 0; i < num_senders; i++) {
        pthread_join(sender_threads[i], NULL);
    }
    
    for (int i = 0; i < num_receivers; i++) {
        pthread_join(receiver_threads[i], NULL);
    }
    
    /* Check the results */
    printf("Concurrent test results: %d messages sent, %d messages received\n",
           g_sender_count, g_receiver_count);
    
    /* Verify that messages were sent and received */
    TEST_ASSERT(g_sender_count > 0, "No messages were sent");
    TEST_ASSERT(g_receiver_count > 0, "No messages were received");
    
    /* The difference between sent and received should be at most the queue size */
    int diff = abs(g_sender_count - g_receiver_count);
    TEST_ASSERT(diff <= 64, "Too many messages lost in transmission");
    
    /* Cleanup */
    destroy_message_queue(g_queue);
    g_queue = NULL;
    
    return TEST_PASSED;
}

/* Test error conditions */
int test_error_conditions(void) {
    /* Test operations with NULL queue */
    message_t message;
    int result = send_message(NULL, &message, 0);
    TEST_ASSERT_EQUAL(-EINVAL, result, "Send with NULL queue should fail with EINVAL");
    
    result = receive_message(NULL, &message, 0);
    TEST_ASSERT_EQUAL(-EINVAL, result, "Receive with NULL queue should fail with EINVAL");
    
    /* Test operations with NULL message */
    message_queue_t queue = create_message_queue("test_errors", 10);
    TEST_ASSERT_NOT_NULL(queue, "Failed to create message queue");
    
    result = send_message(queue, NULL, 0);
    TEST_ASSERT_EQUAL(-EINVAL, result, "Send with NULL message should fail with EINVAL");
    
    result = receive_message(queue, NULL, 0);
    TEST_ASSERT_EQUAL(-EINVAL, result, "Receive with NULL message should fail with EINVAL");
    
    /* Test reply with NULL arguments */
    message_t src_msg, reply_msg;
    result = reply_to_message(NULL, &reply_msg, 0);
    TEST_ASSERT_EQUAL(-EINVAL, result, "Reply with NULL original should fail with EINVAL");
    
    result = reply_to_message(&src_msg, NULL, 0);
    TEST_ASSERT_EQUAL(-EINVAL, result, "Reply with NULL reply should fail with EINVAL");
    
    /* Cleanup */
    destroy_message_queue(queue);
    
    return TEST_PASSED;
}

/*
 * Main test runner
 */
int main(int argc, char** argv) {
    int test_passed = 0;
    int test_failed = 0;
    int total_tests = 0;
    
    printf("============================\n");
    printf("Message Passing System Tests\n");
    printf("============================\n\n");
    
    /* Setup */
    test_setup();
    
    /* Basic tests */
    TEST_RUN(test_queue_create_destroy);
    TEST_RUN(test_queue_create_invalid);
    TEST_RUN(test_basic_send_receive);
    TEST_RUN(test_blocking_send_receive);
    TEST_RUN(test_message_priority);
    TEST_RUN(test_message_reply);
    
    /* Advanced tests */
    TEST_RUN(test_message_timeout);
    TEST_RUN(test_task_cleanup);
    TEST_RUN(test_error_conditions);
    
    /* Stress test (run this last as it's more intensive) */
    TEST_RUN(test_concurrent_operations);
    
    /* Summary */
    printf("\n============================\n");
    printf("Test Summary:\n");
    printf("  Total tests: %d\n", total_tests);
    printf("  Passed:      %d\n", test_passed);
    printf("  Failed:      %d\n", test_failed);
    printf("============================\n");
    
    /* Cleanup */
    test_cleanup();
    
    return (test_failed == 0) ? 0 : 1;
}
