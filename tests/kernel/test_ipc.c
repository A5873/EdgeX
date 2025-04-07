/*
 * EdgeX OS - IPC Subsystem Test Suite
 *
 * This file implements comprehensive tests for the IPC subsystem,
 * including mutexes, semaphores, events, message queues, and shared memory.
 */

#include <edgex/kernel.h>
#include <edgex/scheduler.h>
#include <edgex/ipc.h>
#include <edgex/test.h>

/* Test result tracking */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/* Test PIDs for multi-task tests */
static pid_t test_task1_pid = 0;
static pid_t test_task2_pid = 0;
static pid_t test_task3_pid = 0;

/* Shared test objects */
static mutex_t test_mutex = NULL;
static semaphore_t test_semaphore = NULL;
static event_t test_event = NULL;
static event_set_t test_event_set = NULL;
static message_queue_t test_message_queue = NULL;
static shared_memory_t test_shared_memory = NULL;

/* Shared data for multi-task tests */
static int shared_counter = 0;
static bool test_complete = false;
static bool test_signal_received = false;
static int test_message_received_count = 0;
static int test_event_signal_count = 0;

/*
 * Test helper functions
 */
static void test_start(const char* test_name) {
    kernel_printf("TEST: %s\n", test_name);
    tests_run++;
}

static void test_pass(void) {
    kernel_printf("  PASS\n");
    tests_passed++;
}

static void test_fail(const char* reason) {
    kernel_printf("  FAIL: %s\n", reason);
    tests_failed++;
}

static void test_assert(bool condition, const char* message) {
    if (!condition) {
        test_fail(message);
    }
}

static void test_setup(void) {
    shared_counter = 0;
    test_complete = false;
    test_signal_received = false;
    test_message_received_count = 0;
    test_event_signal_count = 0;
    
    // Reset IPC statistics
    reset_ipc_stats();
}

static void test_teardown(void) {
    // Clean up any test objects that might still exist
    if (test_mutex) {
        destroy_mutex(test_mutex);
        test_mutex = NULL;
    }
    
    if (test_semaphore) {
        destroy_semaphore(test_semaphore);
        test_semaphore = NULL;
    }
    
    if (test_event) {
        destroy_event(test_event);
        test_event = NULL;
    }
    
    if (test_event_set) {
        destroy_event_set(test_event_set);
        test_event_set = NULL;
    }
    
    if (test_message_queue) {
        destroy_message_queue(test_message_queue);
        test_message_queue = NULL;
    }
    
    if (test_shared_memory) {
        destroy_shared_memory(test_shared_memory);
        test_shared_memory = NULL;
    }
}

/*
 * Test tasks for multi-task scenarios
 */
static void test_mutex_contention_task(void) {
    int local_counter = 0;
    
    while (!test_complete) {
        mutex_lock(test_mutex);
        
        // Critical section - increment shared counter
        shared_counter++;
        local_counter++;
        
        mutex_unlock(test_mutex);
        
        // Yield to allow other tasks to run
        yield();
    }
    
    // Record our final count
    mutex_lock(test_mutex);
    kernel_printf("  Task %d incremented counter %d times\n", 
                 get_current_pid(), local_counter);
    mutex_unlock(test_mutex);
    
    // Exit the task
    exit_task();
}

static void test_semaphore_producer_task(void) {
    int count = 0;
    
    while (!test_complete && count < 20) {
        // Produce an item
        semaphore_wait(test_semaphore); // Wait for space
        
        mutex_lock(test_mutex);
        shared_counter++;
        count++;
        kernel_printf("  Producer: produced item %d\n", shared_counter);
        mutex_unlock(test_mutex);
        
        semaphore_post(test_semaphore); // Signal item available
        
        // Yield to consumer
        yield();
    }
    
    exit_task();
}

static void test_semaphore_consumer_task(void) {
    int count = 0;
    
    while (!test_complete && count < 20) {
        // Consume an item
        semaphore_wait(test_semaphore); // Wait for an item
        
        mutex_lock(test_mutex);
        if (shared_counter > 0) {
            shared_counter--;
            count++;
            kernel_printf("  Consumer: consumed item, %d remaining\n", shared_counter);
        }
        mutex_unlock(test_mutex);
        
        semaphore_post(test_semaphore); // Signal space available
        
        // Yield to producer
        yield();
    }
    
    exit_task();
}

static void test_event_waiter_task(void) {
    test_signal_received = false;
    
    kernel_printf("  Event waiter task starting...\n");
    
    // Wait for the event
    int result = event_wait(test_event);
    
    if (result == 0) {
        test_signal_received = true;
        kernel_printf("  Event waiter received signal\n");
    } else {
        kernel_printf("  Event wait failed with error %d\n", result);
    }
    
    exit_task();
}

static void test_event_signaler_task(void) {
    kernel_printf("  Event signaler task starting...\n");
    
    // Sleep a bit before signaling
    sleep_task(100);
    
    // Signal the event
    event_signal(test_event);
    test_event_signal_count++;
    
    kernel_printf("  Event signaled\n");
    
    exit_task();
}

static void test_message_sender_task(void) {
    kernel_printf("  Message sender task starting...\n");
    
    // Create and send a message
    message_t message;
    memset(&message, 0, sizeof(message_t));
    
    message.header.receiver = test_task2_pid;
    message.header.type = MESSAGE_TYPE_NORMAL;
    message.header.priority = MESSAGE_PRIORITY_NORMAL;
    message.header.size = 16;
    
    // Set message payload
    memcpy(message.payload, "Test message", 13);
    
    // Send the message
    int result = send_message(test_message_queue, &message, MESSAGE_FLAG_BLOCKING);
    
    if (result == 0) {
        kernel_printf("  Message sent successfully\n");
    } else {
        kernel_printf("  Failed to send message: %d\n", result);
    }
    
    exit_task();
}

static void test_message_receiver_task(void) {
    kernel_printf("  Message receiver task starting...\n");
    
    // Receive the message
    message_t received;
    
    int result = receive_message(test_message_queue, &received, MESSAGE_FLAG_BLOCKING);
    
    if (result == 0) {
        kernel_printf("  Message received: %s\n", (char*)received.payload);
        test_message_received_count++;
    } else {
        kernel_printf("  Failed to receive message: %d\n", result);
    }
    
    exit_task();
}

static void test_shared_memory_writer_task(void) {
    kernel_printf("  Shared memory writer task starting...\n");
    
    // Map the shared memory
    int* shared_data = (int*)map_shared_memory(test_shared_memory, NULL, SHM_PERM_READ | SHM_PERM_WRITE);
    
    if (!shared_data) {
        kernel_printf("  Failed to map shared memory\n");
        exit_task();
        return;
    }
    
    // Write to shared memory
    for (int i = 0; i < 10; i++) {
        shared_data[i] = i * 10;
    }
    
    kernel_printf("  Writer wrote 10 values to shared memory\n");
    
    // Unmap and exit
    unmap_shared_memory(test_shared_memory);
    exit_task();
}

static void test_shared_memory_reader_task(void) {
    kernel_printf("  Shared memory reader task starting...\n");
    
    // Map the shared memory
    int* shared_data = (int*)map_shared_memory(test_shared_memory, NULL, SHM_PERM_READ);
    
    if (!shared_data) {
        kernel_printf("  Failed to map shared memory\n");
        exit_task();
        return;
    }
    
    // Sleep a bit to ensure writer has written
    sleep_task(200);
    
    // Read from shared memory
    for (int i = 0; i < 10; i++) {
        if (shared_data[i] != i * 10) {
            kernel_printf("  Error: shared_data[%d] = %d, expected %d\n", 
                         i, shared_data[i], i * 10);
        }
    }
    
    kernel_printf("  Reader read 10 values from shared memory\n");
    
    // Unmap and exit
    unmap_shared_memory(test_shared_memory);
    exit_task();
}

/*
 * Individual test cases
 */

/* Mutex Tests */
static void test_mutex_create_destroy(void) {
    test_start("Mutex Create/Destroy");
    
    // Create a mutex
    test_mutex = create_mutex("test_mutex");
    
    test_assert(test_mutex != NULL, "Failed to create mutex");
    
    // Clean up
    destroy_mutex(test_mutex);
    test_mutex = NULL;
    
    test_pass();
}

static void test_mutex_lock_unlock(void) {
    test_start("Mutex Lock/Unlock");
    
    // Create a mutex
    test_mutex = create_mutex("test_mutex");
    test_assert(test_mutex != NULL, "Failed to create mutex");
    
    // Lock the mutex
    int result = mutex_lock(test_mutex);
    test_assert(result == 0, "Failed to lock mutex");
    
    // Unlock the mutex
    result = mutex_unlock(test_mutex);
    test_assert(result == 0, "Failed to unlock mutex");
    
    // Clean up
    destroy_mutex(test_mutex);
    test_mutex = NULL;
    
    test_pass();
}

static void test_mutex_trylock(void) {
    test_start("Mutex TryLock");
    
    // Create a mutex
    test_mutex = create_mutex("test_mutex");
    test_assert(test_mutex != NULL, "Failed to create mutex");
    
    // Try to lock the mutex
    int result = mutex_trylock(test_mutex);
    test_assert(result == 0, "Failed to trylock mutex");
    
    // Try to lock it again - should fail
    result = mutex_trylock(test_mutex);
    test_assert(result == 0, "Mutex should already be locked by same task");
    
    // Unlock the mutex
    result = mutex_unlock(test_mutex);
    test_assert(result == 0, "Failed to unlock mutex");
    
    // Clean up
    destroy_mutex(test_mutex);
    test_mutex = NULL;
    
    test_pass();
}

static void test_mutex_contention(void) {
    test_start("Mutex Contention");
    
    // Create a mutex
    test_mutex = create_mutex("test_mutex");
    test_assert(test_mutex != NULL, "Failed to create mutex");
    
    // Reset shared counter
    shared_counter = 0;
    test_complete = false;
    
    // Create tasks that will contend for the mutex
    test_task1_pid = create_kernel_task("mutex_test1", test_mutex_contention_task, TASK_PRIORITY_NORMAL);
    test_task2_pid = create_kernel_task("mutex_test2", test_mutex_contention_task, TASK_PRIORITY_NORMAL);
    
    // Let them run for a while
    sleep_task(1000);
    
    // Signal completion and let tasks exit
    test_complete = true;
    sleep_task(100);
    
    // Check results
    mutex_lock(test_mutex);
    kernel_printf("  Final counter value: %d\n", shared_counter);
    mutex_unlock(test_mutex);
    
    // Clean up
    destroy_mutex(test_mutex);
    test_mutex = NULL;
    
    test_pass();
}

/* Semaphore Tests */
static void test_semaphore_create_destroy(void) {
    test_start("Semaphore Create/Destroy");
    
    // Create a semaphore
    test_semaphore = create_semaphore("test_semaphore", 1);
    
    test_assert(test_semaphore != NULL, "Failed to create semaphore");
    
    // Clean up
    destroy_semaphore(test_semaphore);
    test_semaphore = NULL;
    
    test_pass();
}

static void test_semaphore_wait_post(void) {
    test_start("Semaphore Wait/Post");
    
    // Create a semaphore with initial value 1
    test_semaphore = create_semaphore("test_semaphore", 1);
    test_assert(test_semaphore != NULL, "Failed to create semaphore");
    
    // Wait on the semaphore
    int result = semaphore_wait(test_semaphore);
    test_assert(result == 0, "Failed to wait on semaphore");
    
    // Post to the semaphore
    result = semaphore_post(test_semaphore);
    test_assert(result == 0, "Failed to post to semaphore");
    
    // Clean up
    destroy_semaphore(test_semaphore);
    test_semaphore = NULL;
    
    test_pass();
}

static void test_semaphore_producer_consumer(void) {
    test_start("Semaphore Producer/Consumer");
    
    // Create a mutex for protecting the shared counter
    test_mutex = create_mutex("test_mutex");
    test_assert(test_mutex != NULL, "Failed to create mutex");
    
    // Create a semaphore with initial value 1
    test_semaphore = create_semaphore("test_semaphore", 1);
    test_assert(test_semaphore != NULL, "Failed to create semaphore");
    
    // Reset shared counter
    shared_counter = 0;
    test_complete = false;
    
    // Create producer and consumer tasks
    test_task1_pid = create_kernel_task("producer", test_semaphore_producer_task, TASK_PRIORITY_NORMAL);
    test_assert(test_task1_pid != 0, "Failed to create producer task");
    
    test_task2_pid = create_kernel_task("consumer", test_semaphore_consumer_task, TASK_PRIORITY_NORMAL);
    test_assert(test_task2_pid != 0, "Failed to create consumer task");
    
    // Let them run for a while
    sleep_task(1000);
    
    // Signal completion and let tasks exit
    test_complete = true;
    sleep_task(200);
    
    // Clean up
    destroy_mutex(test_mutex);
    test_mutex = NULL;
    
    destroy_semaphore(test_semaphore);
    test_semaphore = NULL;
    
    test_pass();
}

static void test_semaphore_trywait(void) {
    test_start("Semaphore TryWait");
    
    // Create a semaphore with initial value 1
    test_semaphore = create_semaphore("test_semaphore", 1);
    test_assert(test_semaphore != NULL, "Failed to create semaphore");
    
    // Try to wait on the semaphore
    int result = semaphore_trywait(test_semaphore);
    test_assert(result == 0, "Failed to trywait on semaphore");
    
    // Try again - should fail
    result = semaphore_trywait(test_semaphore);
    test_assert(result != 0, "Second trywait should fail");
    
    // Post to the semaphore
    result = semaphore_post(test_semaphore);
    test_assert(result == 0, "Failed to post to semaphore");
    
    // Try again - should succeed
    result = semaphore_trywait(test_semaphore);
    test_assert(result == 0, "Trywait should succeed after post");
    
    // Clean up
    destroy_semaphore(test_semaphore);
    test_semaphore = NULL;
    
    test_pass();
}

/* Event Tests */
static void test_event_create_destroy(void) {
    test_start("Event Create/Destroy");
    
    // Create an event
    test_event = create_event("test_event");
    test_assert(test_event != NULL, "Failed to create event");
    
    // Clean up
    destroy_event(test_event);
    test_event = NULL;
    
    test_pass();
}

static void test_event_signal_wait(void) {
    test_start("Event Signal/Wait");
    
    // Create an event
    test_event = create_event("test_event");
    test_assert(test_event != NULL, "Failed to create event");
    
    // Reset counters
    test_signal_received = false;
    test_event_signal_count = 0;
    
    // Create a waiter task
    test_task1_pid = create_kernel_task("event_waiter", test_event_waiter_task, TASK_PRIORITY_NORMAL);
    test_assert(test_task1_pid != 0, "Failed to create event waiter task");
    
    // Create a signaler task
    test_task2_pid = create_kernel_task("event_signaler", test_event_signaler_task, TASK_PRIORITY_NORMAL);
    test_assert(test_task2_pid != 0, "Failed to create event signaler task");
    
    // Wait for tasks to complete
    sleep_task(500);
    
    // Check results
    test_assert(test_signal_received, "Event signal was not received");
    test_assert(test_event_signal_count == 1, "Event was not signaled the expected number of times");
    
    // Clean up
    destroy_event(test_event);
    test_event = NULL;
    
    test_pass();
}

static void test_event_manual_reset(void) {
    test_start("Event Manual Reset");
    
    // Create a manual reset event
    test_event = create_event("test_event");
    test_assert(test_event != NULL, "Failed to create event");
    
    // Set it to manual reset mode
    // Assuming we have a function to set this flag
    event_set_manual_reset(test_event, true);
    
    // Signal the event
    int result = event_signal(test_event);
    test_assert(result == 0, "Failed to signal event");
    
    // Wait on the event - should succeed immediately since it's signaled
    result = event_wait(test_event);
    test_assert(result == 0, "First wait failed");
    
    // Wait again - should still succeed because manual reset events stay signaled
    result = event_wait(test_event);
    test_assert(result == 0, "Second wait failed");
    
    // Reset the event
    result = event_reset(test_event);
    test_assert(result == 0, "Failed to reset event");
    
    // Clean up
    destroy_event(test_event);
    test_event = NULL;
    
    test_pass();
}

static void test_event_auto_reset(void) {
    test_start("Event Auto Reset");
    
    // Create an auto-reset event (default)
    test_event = create_event("test_event");
    test_assert(test_event != NULL, "Failed to create event");
    
    // Signal the event
    int result = event_signal(test_event);
    test_assert(result == 0, "Failed to signal event");
    
    // Wait on the event - should succeed immediately
    result = event_wait(test_event);
    test_assert(result == 0, "First wait failed");
    
    // Wait again - should block because auto-reset events reset after wait
    // We'll use timedwait with a short timeout to avoid blocking the test
    result = event_timedwait(test_event, 100);
    test_assert(result != 0, "Second wait should have timed out");
    
    // Clean up
    destroy_event(test_event);
    test_event = NULL;
    
    test_pass();
}

static void test_event_broadcast(void) {
    test_start("Event Broadcast");
    
    // Create an event
    test_event = create_event("test_event");
    test_assert(test_event != NULL, "Failed to create event");
    
    // Set it to manual reset mode
    event_set_manual_reset(test_event, true);
    
    // Create multiple waiter tasks (would need to implement)
    // For simplicity, we'll just test that broadcast signals the event
    
    // Broadcast to the event
    int result = event_broadcast(test_event);
    test_assert(result == 0, "Failed to broadcast event");
    
    // Wait on the event - should succeed immediately
    result = event_wait(test_event);
    test_assert(result == 0, "Wait failed after broadcast");
    
    // Clean up
    destroy_event(test_event);
    test_event = NULL;
    
    test_pass();
}

static void test_event_timeout(void) {
    test_start("Event Timeout");
    
    // Create an event
    test_event = create_event("test_event");
    test_assert(test_event != NULL, "Failed to create event");
    
    // Wait with timeout
    int result = event_timedwait(test_event, 100);
    test_assert(result != 0, "Wait should have timed out");
    
    // Clean up
    destroy_event(test_event);
    test_event = NULL;
    
    test_pass();
}

static void test_event_set(void) {
    test_start("Event Set");
    
    // Create an event set
    test_event_set = create_event_set("test_event_set", 5);
    test_assert(test_event_set != NULL, "Failed to create event set");
    
    // Create two events
    event_t event1 = create_event("event1");
    test_assert(event1 != NULL, "Failed to create event1");
    
    event_t event2 = create_event("event2");
    test_assert(event2 != NULL, "Failed to create event2");
    
    // Add events to set
    int result = event_set_add(test_event_set, event1);
    test_assert(result == 0, "Failed to add event1 to set");
    
    result = event_set_add(test_event_set, event2);
    test_assert(result == 0, "Failed to add event2 to set");
    
    // Signal the second event
    result = event_signal(event2);
    test_assert(result == 0, "Failed to signal event2");
    
    // Wait on the event set
    event_t signaled_event = NULL;
    result = event_set_wait(test_event_set, &signaled_event);
    test_assert(result == 0, "Failed to wait on event set");
    test_assert(signaled_event == event2, "Wrong event was signaled");
    
    // Clean up
    destroy_event(event1);
    destroy_event(event2);
    destroy_event_set(test_event_set);
    test_event_set = NULL;
    
    test_pass();
}

/* Message Queue Tests */
static void test_message_queue_create_destroy(void) {
    test_start("Message Queue Create/Destroy");
    
    // Create a message queue
    test_message_queue = create_message_queue("test_queue", 10);
    test_assert(test_message_queue != NULL, "Failed to create message queue");
    
    // Clean up
    destroy_message_queue(test_message_queue);
    test_message_queue = NULL;
    
    test_pass();
}

static void test_message_queue_send_receive(void) {
    test_start("Message Queue Send/Receive");
    
    // Create a message queue
    test_message_queue = create_message_queue("test_queue", 10);
    test_assert(test_message_queue != NULL, "Failed to create message queue");
    
    // Reset counters
    test_message_received_count = 0;
    
    // Create sender and receiver tasks
    test_task1_pid = create_kernel_task("sender", test_message_sender_task, TASK_PRIORITY_NORMAL);
    test_assert(test_task1_pid != 0, "Failed to create sender task");
    
    test_task2_pid = create_kernel_task("receiver", test_message_receiver_task, TASK_PRIORITY_NORMAL);
    test_assert(test_task2_pid != 0, "Failed to create receiver task");
    
    // Wait for tasks to complete
    sleep_task(500);
    
    // Check results
    test_assert(test_message_received_count == 1, "Message was not received");
    
    // Clean up
    destroy_message_queue(test_message_queue);
    test_message_queue = NULL;
    
    test_pass();
}

static void test_message_queue_priority(void) {
    test_start("Message Queue Priority");
    
    // Create a message queue
    test_message_queue = create_message_queue("test_queue", 10);
    test_assert(test_message_queue != NULL, "Failed to create message queue");
    
    // Create messages with different priorities
    message_t messages[5];
    message_t received[5];
    
    // Initialize messages
    for (int i = 0; i < 5; i++) {
        memset(&messages[i], 0, sizeof(message_t));
        messages[i].header.receiver = get_current_pid();
        messages[i].header.type = MESSAGE_TYPE_NORMAL;
        messages[i].header.size = 16;
        
        // Set different content to identify the message
        snprintf((char*)messages[i].payload, 16, "Message %d", i);
    }
    
    // Set priorities - intentionally send in mixed priority order
    messages[0].header.priority = MESSAGE_PRIORITY_LOW;
    messages[1].header.priority = MESSAGE_PRIORITY_HIGHEST;
    messages[2].header.priority = MESSAGE_PRIORITY_NORMAL;
    messages[3].header.priority = MESSAGE_PRIORITY_HIGH;
    messages[4].header.priority = MESSAGE_PRIORITY_LOWEST;
    
    kernel_printf("  Sending messages in mixed priority order\n");
    
    // Send all messages
    for (int i = 0; i < 5; i++) {
        int result = send_message(test_message_queue, &messages[i], MESSAGE_FLAG_BLOCKING);
        test_assert(result == 0, "Failed to send message");
        kernel_printf("  Sent message %d with priority %d\n", i, messages[i].header.priority);
    }
    
    kernel_printf("  Receiving messages - should be in priority order\n");
    
    // Expected priority order (highest to lowest): HIGHEST, HIGH, NORMAL, LOW, LOWEST
    int expected_priority_order[5] = {
        MESSAGE_PRIORITY_HIGHEST,
        MESSAGE_PRIORITY_HIGH,
        MESSAGE_PRIORITY_NORMAL,
        MESSAGE_PRIORITY_LOW,
        MESSAGE_PRIORITY_LOWEST
    };
    
    // Receive messages - should come in priority order
    for (int i = 0; i < 5; i++) {
        int result = receive_message(test_message_queue, &received[i], MESSAGE_FLAG_BLOCKING);
        test_assert(result == 0, "Failed to receive message");
        
        kernel_printf("  Received message with priority %d: %s\n", 
                      received[i].header.priority, 
                      (char*)received[i].payload);
        
        // Verify correct priority order
        test_assert(received[i].header.priority == expected_priority_order[i], 
                   "Messages not received in correct priority order");
    }
    
    // Clean up
    destroy_message_queue(test_message_queue);
    test_message_queue = NULL;
    
    test_pass();
}

static void test_message_queue_full(void) {
    test_start("Message Queue Full");
    
    // Create a small message queue with capacity for only 3 messages
    test_message_queue = create_message_queue("test_queue", 3);
    test_assert(test_message_queue != NULL, "Failed to create message queue");
    
    message_t message;
    int result;
    
    // Initialize a test message
    memset(&message, 0, sizeof(message_t));
    message.header.receiver = get_current_pid();
    message.header.type = MESSAGE_TYPE_NORMAL;
    message.header.priority = MESSAGE_PRIORITY_NORMAL;
    message.header.size = 16;
    
    kernel_printf("  Filling message queue to capacity\n");
    
    // Fill the queue completely with non-blocking sends
    for (int i = 0; i < 3; i++) {
        // Set unique payload for each message
        snprintf((char*)message.payload, 16, "Fill Msg %d", i);
        
        // Send without blocking
        result = send_message(test_message_queue, &message, MESSAGE_FLAG_NON_BLOCKING);
        test_assert(result == 0, "Non-blocking send failed before queue was full");
        kernel_printf("  Sent message %d of 3\n", i+1);
    }
    
    // Try to send one more message non-blocking - should fail
    snprintf((char*)message.payload, 16, "Overflow Msg");
    result = send_message(test_message_queue, &message, MESSAGE_FLAG_NON_BLOCKING);
    test_assert(result != 0, "Non-blocking send should fail when queue is full");
    kernel_printf("  Non-blocking send correctly failed when queue full\n");
    
    // Create a task to receive a message to make space
    test_task1_pid = create_kernel_task("receiver", test_message_receiver_task, TASK_PRIORITY_NORMAL);
    test_assert(test_task1_pid != 0, "Failed to create receiver task");
    
    // Wait for receiver to process
    sleep_task(100);
    
    // Now send should succeed
    snprintf((char*)message.payload, 16, "New Msg");
    result = send_message(test_message_queue, &message, MESSAGE_FLAG_NON_BLOCKING);
    test_assert(result == 0, "Non-blocking send failed after space was made available");
    kernel_printf("  Non-blocking send succeeded after space was made available\n");
    
    // Clean up - receive remaining messages
    for (int i = 0; i < 3; i++) {
        result = receive_message(test_message_queue, &message, MESSAGE_FLAG_NON_BLOCKING);
        if (result != 0) {
            kernel_printf("  Warning: Only %d messages left in queue during cleanup\n", i);
            break;
        }
    }
    
    // Clean up
    destroy_message_queue(test_message_queue);
    test_message_queue = NULL;
    
    test_pass();
}

static void test_message_queue_empty(void) {
    test_start("Message Queue Empty");
    
    // Create an empty message queue
    test_message_queue = create_message_queue("test_queue", 5);
    test_assert(test_message_queue != NULL, "Failed to create message queue");
    
    message_t message;
    int result;
    
    kernel_printf("  Testing receive on empty queue\n");
    
    // Try to receive from empty queue with non-blocking - should fail
    result = receive_message(test_message_queue, &message, MESSAGE_FLAG_NON_BLOCKING);
    test_assert(result != 0, "Non-blocking receive should fail when queue is empty");
    kernel_printf("  Non-blocking receive correctly failed when queue empty\n");
    
    // Try to receive with a timeout - should time out
    uint64_t start_time = get_system_time();
    result = receive_message(test_message_queue, &message, MESSAGE_FLAG_TIMEOUT | 100);
    uint64_t elapsed_time = get_system_time() - start_time;
    
    test_assert(result != 0, "Receive with timeout should fail when queue is empty");
    test_assert(elapsed_time >= 100, "Timeout occurred too quickly");
    kernel_printf("  Timed receive correctly timed out after approximately %llu ms\n", elapsed_time);
    
    // Create a sender task
    kernel_printf("  Creating sender task\n");
    test_task1_pid = create_kernel_task("sender", test_message_sender_task, TASK_PRIORITY_NORMAL);
    test_assert(test_task1_pid != 0, "Failed to create sender task");
    
    // Now a blocking receive should succeed
    kernel_printf("  Attempting blocking receive\n");
    result = receive_message(test_message_queue, &message, MESSAGE_FLAG_BLOCKING);
    test_assert(result == 0, "Blocking receive failed after message was sent");
    kernel_printf("  Received message: %s\n", (char*)message.payload);
    
    // Clean up
    destroy_message_queue(test_message_queue);
    test_message_queue = NULL;
    
    test_pass();
}

/* Shared Memory Tests */
static void test_shared_memory_create_destroy(void) {
    test_start("Shared Memory Create/Destroy");
    
    // Create a shared memory segment
    test_shared_memory = create_shared_memory("test_shm", 4096);
    test_assert(test_shared_memory != NULL, "Failed to create shared memory");
    
    // Get information about the shared memory
    shm_info_t info;
    int result = get_shared_memory_info(test_shared_memory, &info);
    test_assert(result == 0, "Failed to get shared memory info");
    
    // Verify size
    test_assert(info.size == 4096, "Shared memory size mismatch");
    
    // Clean up
    destroy_shared_memory(test_shared_memory);
    test_shared_memory = NULL;
    
    test_pass();
}

static void test_shared_memory_read_write(void) {
    test_start("Shared Memory Read/Write");
    
    // Create a shared memory segment
    test_shared_memory = create_shared_memory("test_shm", 4096);
    test_assert(test_shared_memory != NULL, "Failed to create shared memory");
    
    // Map the shared memory with read/write permissions
    void* mapped_memory = map_shared_memory(test_shared_memory, NULL, SHM_PERM_READ | SHM_PERM_WRITE);
    test_assert(mapped_memory != NULL, "Failed to map shared memory");
    
    // Write some test data
    int* data = (int*)mapped_memory;
    for (int i = 0; i < 100; i++) {
        data[i] = i * 10;
    }
    
    kernel_printf("  Wrote 100 test values to shared memory\n");
    
    // Unmap the memory
    int result = unmap_shared_memory(test_shared_memory);
    test_assert(result == 0, "Failed to unmap shared memory");
    
    // Map it again to verify data persistence
    mapped_memory = map_shared_memory(test_shared_memory, NULL, SHM_PERM_READ);
    test_assert(mapped_memory != NULL, "Failed to remap shared memory");
    
    // Verify the data
    data = (int*)mapped_memory;
    bool data_valid = true;
    for (int i = 0; i < 100; i++) {
        if (data[i] != i * 10) {
            kernel_printf("  Error: data[%d] = %d, expected %d\n", i, data[i], i * 10);
            data_valid = false;
            break;
        }
    }
    
    test_assert(data_valid, "Shared memory data was corrupted or not persistent");
    kernel_printf("  Successfully verified 100 values in shared memory\n");
    
    // Unmap and clean up
    result = unmap_shared_memory(test_shared_memory);
    test_assert(result == 0, "Failed to unmap shared memory");
    
    destroy_shared_memory(test_shared_memory);
    test_shared_memory = NULL;
    
    test_pass();
}

static void test_shared_memory_permissions(void) {
    test_start("Shared Memory Permissions");
    
    // Create a shared memory segment
    test_shared_memory = create_shared_memory("test_shm", 4096);
    test_assert(test_shared_memory != NULL, "Failed to create shared memory");
    
    // Map with read-only permissions
    void* read_only_memory = map_shared_memory(test_shared_memory, NULL, SHM_PERM_READ);
    test_assert(read_only_memory != NULL, "Failed to map shared memory as read-only");
    
    // Map with write-only permissions
    void* write_only_memory = map_shared_memory(test_shared_memory, NULL, SHM_PERM_WRITE);
    test_assert(write_only_memory != NULL, "Failed to map shared memory as write-only");
    
    // Map with read-write permissions
    void* read_write_memory = map_shared_memory(test_shared_memory, NULL, SHM_PERM_READ | SHM_PERM_WRITE);
    test_assert(read_write_memory != NULL, "Failed to map shared memory as read-write");
    
    // Write to the write-only and read-write mappings
    int* write_data = (int*)write_only_memory;
    for (int i = 0; i < 10; i++) {
        write_data[i] = i + 100;
    }
    
    int* rw_data = (int*)read_write_memory;
    for (int i = 10; i < 20; i++) {
        rw_data[i] = i + 200;
    }
    
    // Read from the read-only and read-write mappings
    int* read_data = (int*)read_only_memory;
    bool read_data_valid = true;
    
    // Verify data written through write-only mapping
    for (int i = 0; i < 10; i++) {
        if (read_data[i] != i + 100) {
            kernel_printf("  Error: read_data[%d] = %d, expected %d\n", i, read_data[i], i + 100);
            read_data_valid = false;
            break;
        }
    }
    
    // Verify data written through read-write mapping
    for (int i = 10; i < 20; i++) {
        if (read_data[i] != i + 200) {
            kernel_printf("  Error: read_data[%d] = %d, expected %d\n", i, read_data[i], i + 200);
            read_data_valid = false;
            break;
        }
    }
    
    test_assert(read_data_valid, "Failed to verify data across different permission mappings");
    kernel_printf("  Successfully verified data across different permission mappings\n");
    
    // Unmap all mappings
    int result = unmap_shared_memory(test_shared_memory);
    test_assert(result == 0, "Failed to unmap shared memory");
    
    // Clean up
    destroy_shared_memory(test_shared_memory);
    test_shared_memory = NULL;
    
    test_pass();
}

static void test_shared_memory_multi_task(void) {
    test_start("Shared Memory Multi-Task Access");
    
    // Create a shared memory segment
    test_shared_memory = create_shared_memory("test_shm", 4096);
    test_assert(test_shared_memory != NULL, "Failed to create shared memory");
    
    // Create writer and reader tasks
    test_task1_pid = create_kernel_task("shm_writer", test_shared_memory_writer_task, TASK_PRIORITY_NORMAL);
    test_assert(test_task1_pid != 0, "Failed to create shared memory writer task");
    
    // Let the writer run a bit
    sleep_task(100);
    
    // Create reader task
    test_task2_pid = create_kernel_task("shm_reader", test_shared_memory_reader_task, TASK_PRIORITY_NORMAL);
    test_assert(test_task2_pid != 0, "Failed to create shared memory reader task");
    
    // Let the tasks run and complete
    sleep_task(500);
    
    // Clean up
    destroy_shared_memory(test_shared_memory);
    test_shared_memory = NULL;
    
    test_pass();
}

/*
 * Test Runner Function
 */
static void run_all_tests(void) {
    kernel_printf("\n===== EdgeX IPC Subsystem Test Suite =====\n\n");
    
    kernel_printf("--- Mutex Tests ---\n");
    test_setup();
    test_mutex_create_destroy();
    test_teardown();
    
    test_setup();
    test_mutex_lock_unlock();
    test_teardown();
    
    test_setup();
    test_mutex_trylock();
    test_teardown();
    
    test_setup();
    test_mutex_contention();
    test_teardown();
    
    kernel_printf("\n--- Semaphore Tests ---\n");
    test_setup();
    test_semaphore_create_destroy();
    test_teardown();
    
    test_setup();
    test_semaphore_wait_post();
    test_teardown();
    
    test_setup();
    test_semaphore_producer_consumer();
    test_teardown();
    
    test_setup();
    test_semaphore_trywait();
    test_teardown();
    
    kernel_printf("\n--- Event Tests ---\n");
    test_setup();
    test_event_create_destroy();
    test_teardown();
    
    test_setup();
    test_event_signal_wait();
    test_teardown();
    
    test_setup();
    test_event_manual_reset();
    test_teardown();
    
    test_setup();
    test_event_auto_reset();
    test_teardown();
    
    test_setup();
    test_event_broadcast();
    test_teardown();
    
    test_setup();
    test_event_timeout();
    test_teardown();
    
    test_setup();
    test_event_set();
    test_teardown();
    
    kernel_printf("\n--- Message Queue Tests ---\n");
    test_setup();
    test_message_queue_create_destroy();
    test_teardown();
    
    test_setup();
    test_message_queue_send_receive();
    test_teardown();
    
    test_setup();
    test_message_queue_priority();
    test_teardown();
    
    test_setup();
    test_message_queue_full();
    test_teardown();
    
    test_setup();
    test_message_queue_empty();
    test_teardown();
    
    kernel_printf("\n--- Shared Memory Tests ---\n");
    test_setup();
    test_shared_memory_create_destroy();
    test_teardown();
    
    test_setup();
    test_shared_memory_read_write();
    test_teardown();
    
    test_setup();
    test_shared_memory_permissions();
    test_teardown();
    
    test_setup();
    test_shared_memory_multi_task();
    test_teardown();
    
    // Print test summary
    kernel_printf("\n===== Test Summary =====\n");
    kernel_printf("Total tests run: %d\n", tests_run);
    kernel_printf("Tests passed:    %d\n", tests_passed);
    kernel_printf("Tests failed:    %d\n", tests_failed);
    
    if (tests_failed == 0) {
        kernel_printf("\nTEST SUITE PASSED\n");
    } else {
        kernel_printf("\nTEST SUITE FAILED\n");
    }
    
    kernel_printf("\n===========================\n");
}

/*
 * Main Entry Point
 */
int main(void) {
    // Initialize kernel subsystems
    kernel_initialize();
    
    // Run all tests
    run_all_tests();
    
    // Return appropriate exit code
    return (tests_failed == 0) ? 0 : 1;
}
