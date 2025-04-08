/*
 * EdgeX OS - Shared Memory Interface
 *
 * This file defines the interface for the shared memory system in EdgeX OS,
 * allowing tasks to share memory regions with controlled access permissions
 * and lifecycle management.
 *
 * The shared memory system provides a secure and efficient way for tasks to share
 * data without copying, supporting:
 *   - Named memory segments that can be accessed by multiple tasks
 *   - Granular access control with read/write/execute permissions
 *   - Copy-on-write memory optimization
 *   - Automatic cleanup of shared resources when tasks terminate
 *   - Memory segment resizing
 *
 * Shared memory is typically used in conjunction with the message queue system,
 * where messages signal that new data is available in a shared memory region.
 */

#ifndef EDGEX_IPC_SHARED_MEMORY_H
#define EDGEX_IPC_SHARED_MEMORY_H

#include <edgex/memory.h>
#include <edgex/scheduler.h>

/* Shared memory permissions */
/**
 * Permission flags controlling access to shared memory segments.
 * These can be combined with the bitwise OR operator (|).
 */
#define SHM_PERM_READ    (1 << 0)  /* Read permission */
#define SHM_PERM_WRITE   (1 << 1)  /* Write permission */
#define SHM_PERM_EXEC    (1 << 2)  /* Execute permission */

/* Common permission combinations */
#define SHM_PERM_NONE    0                               /* No access */
#define SHM_PERM_RW      (SHM_PERM_READ | SHM_PERM_WRITE) /* Read-write access */
#define SHM_PERM_RX      (SHM_PERM_READ | SHM_PERM_EXEC)  /* Read-execute access */
#define SHM_PERM_RWX     (SHM_PERM_READ | SHM_PERM_WRITE | SHM_PERM_EXEC) /* Full access */

/* Shared memory flags */
/**
 * Creation and control flags for shared memory segments.
 * These can be combined with the bitwise OR operator (|).
 */
#define SHM_FLAG_CREATE  (1 << 0)  /* Create the segment if it doesn't exist */
#define SHM_FLAG_EXCL    (1 << 1)  /* Fail if segment already exists */
#define SHM_FLAG_RESIZE  (1 << 2)  /* Allow segment to be resized */
#define SHM_FLAG_COW     (1 << 3)  /* Use copy-on-write optimization */
#define SHM_FLAG_PERSIST (1 << 4)  /* Keep segment after all tasks detach */
#define SHM_FLAG_LOCKED  (1 << 5)  /* Lock segment in physical memory (no swapping) */

/* Shared memory handle (opaque to user code) */
typedef void* shm_handle_t;

/* Shared memory information structure */
typedef struct {
    char name[64];             /* Name of the shared memory segment */
    size_t size;               /* Size of the segment in bytes */
    uint32_t permissions;      /* Default permissions */
    uint32_t flags;            /* Segment flags */
    uint32_t ref_count;        /* Number of tasks using this segment */
    pid_t creator_pid;         /* PID of the task that created the segment */
    uint64_t creation_time;    /* Creation timestamp */
} shm_info_t;

/**
 * Initialize the shared memory subsystem
 *
 * This function initializes the shared memory management subsystem.
 * It is called automatically during system startup and should not be
 * called directly by application code.
 */
void init_shared_memory(void);

/**
 * Create a new shared memory segment
 *
 * @param name         Name of the shared memory segment (must be unique)
 * @param size         Size of the segment in bytes
 * @param permissions  Default permissions (SHM_PERM_* flags)
 * @param flags        Control flags (SHM_FLAG_* flags)
 *
 * @return Handle to the shared memory segment or NULL on failure
 *
 * This function creates a new shared memory segment with the specified name,
 * size, permissions, and flags. If a segment with the same name already exists,
 * the behavior depends on the flags:
 * - With SHM_FLAG_CREATE | SHM_FLAG_EXCL: Fails if segment exists
 * - With SHM_FLAG_CREATE only: Returns the existing segment if size is sufficient
 * - Without SHM_FLAG_CREATE: Fails if segment doesn't exist
 *
 * The returned handle can be used with other shared memory functions.
 *
 * Example:
 *   // Create a 4KB shared memory segment with read-write access
 *   shm_handle_t shm = create_shared_memory("app_data", 4096, 
 *                                         SHM_PERM_READ | SHM_PERM_WRITE,
 *                                         SHM_FLAG_CREATE | SHM_FLAG_RESIZE);
 *   if (shm == NULL) {
 *       // Handle error
 *   }
 */
shm_handle_t create_shared_memory(const char* name, size_t size, 
                                uint32_t permissions, uint32_t flags);

/**
 * Destroy a shared memory segment
 *
 * @param handle  Handle to the shared memory segment
 *
 * This function destroys a shared memory segment, freeing all associated resources.
 * The actual destruction only happens when all tasks have unmapped the segment,
 * unless the SHM_FLAG_PERSIST flag was used during creation.
 *
 * The handle becomes invalid after this call and should not be used again.
 *
 * Example:
 *   destroy_shared_memory(shm);
 *   shm = NULL;  // Good practice to avoid dangling pointers
 */
void destroy_shared_memory(shm_handle_t handle);

/**
 * Map a shared memory segment into the current task's address space
 *
 * @param handle       Handle to the shared memory segment
 * @param permissions  Desired access permissions (SHM_PERM_* flags)
 *
 * @return Virtual address of the mapped segment or NULL on failure
 *
 * This function maps the shared memory segment into the current task's address space
 * with the requested permissions. The effective permissions are the intersection of
 * the requested permissions and the segment's default permissions.
 *
 * Example:
 *   // Map shared memory with read-write access
 *   void* data = map_shared_memory(shm, SHM_PERM_READ | SHM_PERM_WRITE);
 *   if (data == NULL) {
 *       // Handle error
 *   }
 *
 *   // Use the shared memory
 *   int* values = (int*)data;
 *   values[0] = 42;
 */
void* map_shared_memory(shm_handle_t handle, uint32_t permissions);

/**
 * Unmap a shared memory segment from the current task's address space
 *
 * @param addr  Virtual address of the mapped segment
 * @param size  Size of the mapped region (must match the original mapping size)
 *
 * @return 0 on success, negative error code on failure
 *
 * This function unmaps a previously mapped shared memory segment from the
 * current task's address space. The address must be the same as that returned
 * by map_shared_memory().
 *
 * Example:
 *   // Unmap shared memory
 *   int result = unmap_shared_memory(data, 4096);
 *   if (result != 0) {
 *       // Handle error
 *   }
 *   data = NULL;  // Good practice to avoid dangling pointers
 */
int unmap_shared_memory(void* addr, size_t size);

/**
 * Resize a shared memory segment
 *
 * @param handle    Handle to the shared memory segment
 * @param new_size  New size for the segment (in bytes)
 *
 * @return 0 on success, negative error code on failure
 *
 * This function resizes a shared memory segment. If the segment grows,
 * the new memory is initialized to zero. If the segment shrinks, data
 * beyond the new size is lost.
 *
 * The resize operation is only allowed if the segment was created with
 * the SHM_FLAG_RESIZE flag.
 *
 * Example:
 *   // Resize shared memory to 8KB
 *   int result = resize_shared_memory(shm, 8192);
 *   if (result != 0) {
 *       // Handle error
 *   }
 */
int resize_shared_memory(shm_handle_t handle, size_t new_size);

/**
 * Get information about a shared memory segment
 *
 * @param handle  Handle to the shared memory segment
 * @param info    Pointer to an shm_info_t structure to fill
 *
 * @return 0 on success, negative error code on failure
 *
 * This function retrieves information about a shared memory segment and
 * fills the provided info structure.
 *
 * Example:
 *   shm_info_t info;
 *   int result = get_shared_memory_info(shm, &info);
 *   if (result == 0) {
 *       printf("Shared memory '%s' size: %zu bytes, refs: %u\n",
 *              info.name, info.size, info.ref_count);
 *   }
 */
int get_shared_memory_info(shm_handle_t handle, shm_info_t* info);

/**
 * Find a shared memory segment by name
 *
 * @param name  Name of the shared memory segment to find
 *
 * @return Handle to the found segment or NULL if not found
 *
 * This function searches for a shared memory segment with the specified name.
 * If found, it returns a handle to the segment.
 *
 * Example:
 *   // Find an existing shared memory segment
 *   shm_handle_t shm = find_shared_memory("app_data");
 *   if (shm != NULL) {
 *       // Use the shared memory
 *       void* data = map_shared_memory(shm, SHM_PERM_READ);
 *       // ...
 *   }
 */
shm_handle_t find_shared_memory(const char* name);

/**
 * Clean up shared memory resources for a terminated task
 *
 * @param task_id  PID of the terminated task
 *
 * This function is called by the task scheduler when a task terminates.
 * It cleans up all shared memory mappings owned by the task and decrements
 * reference counts for all segments it was using.
 *
 * Application code should not call this function directly.
 */
void cleanup_task_shared_memory(pid_t task_id);

/*
 * Shared Memory with Copy-on-Write
 * --------------------------------
 *
 * The shared memory system supports copy-on-write (COW) optimization, which
 * allows multiple tasks to share the same physical memory pages until one of
 * them attempts to modify the data. When a write occurs, a private copy of the
 * modified page is created for that task, while other tasks continue to share
 * the original page.
 *
 * This is particularly useful for scenarios like process forking, where a child
 * process initially shares all memory pages with its parent, but gradually builds
 * its own set of private pages as it makes modifications.
 *
 * To use COW, create a shared memory segment with the SHM_FLAG_COW flag:
 *
 *   shm_handle_t shm = create_shared_memory("cowdata", 4096,
 *                                         SHM_PERM_RW,
 *                                         SHM_FLAG_CREATE | SHM_FLAG_COW);
 *
 * When a task maps this segment with write permission and attempts to modify it,
 * the page directory manager will automatically create a private copy of the
 * affected page(s) for that task.
 *
 * Note that COW optimization works at the page level (4KB, 2MB, or 1GB depending
 * on the page size configuration), not at the byte level. Writing even a single
 * byte to a shared page will cause the entire page to be copied.
 */

/*
 * Usage Example: Producer-Consumer Pattern
 * ---------------------------------------
 *
 * The following example demonstrates a simple producer-consumer pattern where
 * one task produces data and another consumes it, using shared memory for the
 * data buffer and message queues for synchronization.
 *
 * Producer task:
 * ```c
 * // Create shared memory for the buffer
 * shm_handle_t shm = create_shared_memory("data_buffer", 4096, SHM_PERM_RW, SHM_FLAG_CREATE);
 * if (shm == NULL) {
 *     // Handle error
 *     return;
 * }
 *
 * // Map the shared memory
 * int* buffer = (int*)map_shared_memory(shm, SHM_PERM_RW);
 * if (buffer == NULL) {
 *     // Handle error
 *     destroy_shared_memory(shm);
 *     return;
 * }
 *
 * // Create message queue for signaling
 * message_queue_t queue = create_message_queue("data_signal", 10);
 *
 * // Produce data
 * for (int i = 0; i < 100; i++) {
 *     // Write to shared memory
 *     buffer[i % 10] = i;
 *
 *     // Signal consumer
 *     message_t msg;
 *     memset(&msg, 0, sizeof(message_t));
 *     msg.receiver = consumer_pid;
 *     msg.type = MESSAGE_TYPE_NORMAL;
 *     msg.size = sizeof(int);
 *     *((int*)msg.payload) = i % 10;  // Index of updated data
 *     send_message(queue, &msg, 0);
 * }
 *
 * // Cleanup
 * unmap_shared_memory(buffer, 4096);
 * destroy_shared_memory(shm);
 * destroy_message_queue(queue);
 * ```
 *
 * Consumer task:
 * ```c
 * // Find shared memory
 * shm_handle_t shm = find_shared_memory("data_buffer");
 * if (shm == NULL) {
 *     // Handle error
 *     return;
 * }
 *
 * // Map the shared memory (read-only for safety)
 * int* buffer = (int*)map_shared_memory(shm, SHM_PERM_READ);
 * if (buffer == NULL) {
 *     // Handle error
 *     return;
 * }
 *
 * // Create message queue for receiving signals
 * message_queue_t queue = create_message_queue("signal_receiver", 10);
 *
 * // Process data
 * for (int i = 0; i < 100; i++) {
 *     // Wait for signal from producer
 *     message_t msg;
 *     receive_message(queue, &msg, 0);
 *
 *     // Read the index of updated data
 *     int index = *((int*)msg.payload);
 *
 *     // Process the data
 *     printf("Received value: %d\n", buffer[index]);
 * }
 *
 * // Cleanup
 * unmap_shared_memory(buffer, 4096);
 * destroy_message_queue(queue);
 * ```
 */

#endif /* EDGEX_IPC_SHARED_MEMORY_H */

