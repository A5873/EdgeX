/*
 * EdgeX OS - Inter-Process Communication Implementation
 *
 * This file implements the core IPC mechanisms for EdgeX OS,
 * including mutexes, semaphores, and common utilities.
 */

#include <edgex/kernel.h>
#include <edgex/memory.h>
#include <edgex/scheduler.h>
#include <edgex/ipc.h>

/* Maximum name length for IPC objects */
#define MAX_IPC_NAME_LENGTH 64

/* Maximum number of wait queues in the system */
#define MAX_WAIT_QUEUES 256

/* Wait queue flags */
#define WAIT_QUEUE_MUTEX     0x01    /* Queue is for a mutex */
#define WAIT_QUEUE_SEMAPHORE 0x02    /* Queue is for a semaphore */
#define WAIT_QUEUE_EVENT     0x04    /* Queue is for an event */
#define WAIT_QUEUE_MSGQUEUE  0x08    /* Queue is for a message queue */

/* IPC object types */
typedef enum {
    IPC_TYPE_MUTEX = 1,
    IPC_TYPE_SEMAPHORE,
    IPC_TYPE_EVENT,
    IPC_TYPE_EVENT_SET,
    IPC_TYPE_MESSAGE_QUEUE,
    IPC_TYPE_SHARED_MEMORY
} ipc_object_type_t;

/* Basic IPC object header (common to all IPC objects) */
typedef struct {
    uint32_t type;                /* IPC object type */
    char name[MAX_IPC_NAME_LENGTH]; /* Object name */
    uint32_t ref_count;           /* Reference count */
    void (*destroy_fn)(void*);    /* Function to destroy this object */
} ipc_object_header_t;

/* Waiter structure - represents a task waiting on a resource */
typedef struct waiter {
    pid_t pid;                    /* PID of waiting task */
    uint64_t wait_start_time;     /* When the wait started */
    uint64_t timeout;             /* Timeout in ticks (0 = forever) */
    struct waiter* next;          /* Next waiter in queue */
} waiter_t;

/* Wait queue - list of tasks waiting for a resource */
typedef struct {
    char name[MAX_IPC_NAME_LENGTH]; /* Queue name */
    uint32_t flags;               /* Queue flags */
    waiter_t* head;               /* Head of waiters list */
    waiter_t* tail;               /* Tail of waiters list */
    uint32_t waiter_count;        /* Number of waiters */
    void* owner;                  /* IPC object that owns this queue */
} wait_queue_t;

/* Mutex structure */
struct mutex {
    ipc_object_header_t header;   /* Common header */
    pid_t owner;                  /* Current owner (0 if unlocked) */
    uint32_t lock_count;          /* For recursive mutexes */
    wait_queue_t wait_queue;      /* Queue of waiting tasks */
};

/* Semaphore structure */
struct semaphore {
    ipc_object_header_t header;   /* Common header */
    int32_t value;                /* Current semaphore value */
    int32_t max_value;            /* Maximum value */
    wait_queue_t wait_queue;      /* Queue of waiting tasks */
};

/* Array of all mutexes */
static mutex_t mutex_pool[MAX_WAIT_QUEUES];
static uint32_t mutex_count = 0;

/* Array of all semaphores */
static semaphore_t semaphore_pool[MAX_WAIT_QUEUES];
static uint32_t semaphore_count = 0;

/* Forward declarations */
static void destroy_mutex(void* mutex);
static void destroy_semaphore(void* semaphore);
static void wake_waiters(wait_queue_t* queue, uint32_t count);
static void add_waiter(wait_queue_t* queue, pid_t pid, uint64_t timeout);
static void remove_waiter(wait_queue_t* queue, pid_t pid);

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
}

/*
 * Add a waiter to a wait queue
 */
static void add_waiter(wait_queue_t* queue, pid_t pid, uint64_t timeout) {
    waiter_t* waiter = (waiter_t*)kmalloc(sizeof(waiter_t));
    if (!waiter) {
        kernel_printf("Failed to allocate waiter structure\n");
        return;
    }
    
    // Initialize waiter
    waiter->pid = pid;
    waiter->wait_start_time = get_tick_count();
    waiter->timeout = timeout;
    waiter->next = NULL;
    
    // Add to end of queue
    if (queue->tail) {
        queue->tail->next = waiter;
        queue->tail = waiter;
    } else {
        queue->head = queue->tail = waiter;
    }
    
    queue->waiter_count++;
}

/*
 * Remove a waiter from a wait queue
 */
static void remove_waiter(wait_queue_t* queue, pid_t pid) {
    waiter_t* prev = NULL;
    waiter_t* current = queue->head;
    
    while (current) {
        if (current->pid == pid) {
            // Remove from list
            if (prev) {
                prev->next = current->next;
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
            return;
        }
        
        prev = current;
        current = current->next;
    }
}

/*
 * Wake up waiters in a wait queue
 */
static void wake_waiters(wait_queue_t* queue, uint32_t count) {
    uint32_t woken = 0;
    waiter_t* current = queue->head;
    waiter_t* next;
    
    while (current && (count == 0 || woken < count)) {
        next = current->next;
        
        // Unblock the task
        unblock_task(current->pid);
        
        // Remove from queue
        if (queue->head == current) {
            queue->head = next;
        }
        
        kfree(current);
        woken++;
        
        current = next;
    }
    
    // Update tail if all waiters were woken
    if (queue->head == NULL) {
        queue->tail = NULL;
    }
    
    queue->waiter_count -= woken;
}

/*
 * Check wait queue for timed-out waiters
 */
static void check_timeouts(wait_queue_t* queue) {
    waiter_t* prev = NULL;
    waiter_t* current = queue->head;
    waiter_t* next;
    uint64_t current_tick = get_tick_count();
    
    while (current) {
        next = current->next;
        
        // Check if the waiter has timed out
        if (current->timeout > 0 && 
            (current_tick - current->wait_start_time) >= current->timeout) {
            
            // Unblock the task
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
        } else {
            prev = current;
        }
        
        current = next;
    }
}

/*
 * Find a free mutex slot
 */
static mutex_t find_free_mutex(void) {
    for (uint32_t i = 0; i < MAX_WAIT_QUEUES; i++) {
        if (mutex_pool[i].header.type == 0) {
            return &mutex_pool[i];
        }
    }
    return NULL;
}

/*
 * Find a free semaphore slot
 */
static semaphore_t find_free_semaphore(void) {
    for (uint32_t i = 0; i < MAX_WAIT_QUEUES; i++) {
        if (semaphore_pool[i].header.type == 0) {
            return &semaphore_pool[i];
        }
    }
    return NULL;
}

/*
 * Create a mutex
 */
mutex_t create_mutex(const char* name) {
    mutex_t mutex = find_free_mutex();
    if (!mutex) {
        kernel_printf("Failed to allocate mutex: no free slots\n");
        return NULL;
    }
    
    // Initialize mutex
    mutex->header.type = IPC_TYPE_MUTEX;
    strncpy(mutex->header.name, name, MAX_IPC_NAME_LENGTH - 1);
    mutex->header.name[MAX_IPC_NAME_LENGTH - 1] = '\0';
    mutex->header.ref_count = 1;
    mutex->header.destroy_fn = destroy_mutex;
    
    mutex->owner = 0;
    mutex->lock_count = 0;
    
    // Initialize wait queue
    init_wait_queue(&mutex->wait_queue, name, WAIT_QUEUE_MUTEX, mutex);
    
    mutex_count++;
    return mutex;
}

/*
 * Destroy a mutex
 */
void destroy_mutex(void* mutex_ptr) {
    mutex_t mutex = (mutex_t)mutex_ptr;
    
    if (!mutex || mutex->header.type != IPC_TYPE_MUTEX) {
        return;
    }
    
    // Wake up all waiters
    wake_waiters(&mutex->wait_queue, 0);
    
    // Clear the mutex
    memset(mutex, 0, sizeof(struct mutex));
    
    mutex_count--;
}

/*
 * Lock a mutex
 */
int mutex_lock(mutex_t mutex) {
    if (!mutex || mutex->header.type != IPC_TYPE_MUTEX) {
        return -1;
    }
    
    pid_t current_pid = get_current_pid();
    
    // Disable interrupts for atomic operation
    __asm__ volatile("cli");
    
    // If mutex is already owned by current task, increment lock count (recursive mutex)
    if (mutex->owner == current_pid) {
        mutex->lock_count++;
        __asm__ volatile("sti");
        return 0;
    }
    
    // If mutex is unlocked, take ownership
    if (mutex->owner == 0) {
        mutex->owner = current_pid;
        mutex->lock_count = 1;
        __asm__ volatile("sti");
        return 0;
    }
    
    // Mutex is locked by another task, so block
    add_waiter(&mutex->wait_queue, current_pid, 0);
    
    // Re-enable interrupts before blocking
    __asm__ volatile("sti");
    
    // Block the current task
    block_task(current_pid);
    
    // When we're unblocked, the mutex should be ours
    mutex->owner = current_pid;
    mutex->lock_count = 1;
    
    return 0;
}

/*
 * Try to lock a mutex (non-blocking)
 */
int mutex_trylock(mutex_t mutex) {
    if (!mutex || mutex->header.type != IPC_TYPE_MUTEX) {
        return -1;
    }
    
    pid_t current_pid = get_current_pid();
    
    // Disable interrupts for atomic operation
    __asm__ volatile("cli");
    
    // If mutex is already owned by current task, increment lock count
    if (mutex->owner == current_pid) {
        mutex->lock_count++;
        __asm__ volatile("sti");
        return 0;
    }
    
    // If mutex is unlocked, take ownership
    if (mutex->owner == 0) {
        mutex->owner = current_pid;
        mutex->lock_count = 1;
        __asm__ volatile("sti");
        return 0;
    }
    
    // Mutex is locked by another task
    __asm__ volatile("sti");
    return -1;
}

/*
 * Unlock a mutex
 */
int mutex_unlock(mutex_t mutex) {
    if (!mutex || mutex->header.type != IPC_TYPE_MUTEX) {
        return -1;
    }
    
    pid_t current_pid = get_current_pid();
    
    // Disable interrupts for atomic operation
    __asm__ volatile("cli");
    
    // Check if we own the mutex
    if (mutex->owner != current_pid) {
        __asm__ volatile("sti");
        return -1;
    }
    
    // Decrement lock count (for recursive mutexes)
    mutex->lock_count--;
    
    // If lock count reaches 0, release the mutex
    if (mutex->lock_count == 0) {
        mutex->owner = 0;
        
        // Wake up one waiter if any
        if (mutex->wait_queue.waiter_count > 0) {
            wake_waiters(&mutex->wait_queue, 1);
        }
    }
    
    __asm__ volatile("sti");
    return 0;
}

/*
 * Create a semaphore
 */
semaphore_t create_semaphore(const char* name, uint32_t initial_value) {
    semaphore_t sem = find_free_semaphore();
    if (!sem) {
        kernel_printf("Failed to allocate semaphore: no free slots\n");
        return NULL;
    }
    
    // Initialize semaphore
    sem->header.type = IPC_TYPE_SEMAPHORE;
    strncpy(sem->header.name, name, MAX_IPC_NAME_LENGTH - 1);
    sem->header.name[MAX_IPC_NAME_LENGTH - 1] = '\0';
    sem->header.ref_count = 1;
    sem->header.destroy_fn = destroy_semaphore;
    
    sem->value = initial_value;
    sem->max_value = 0x7FFFFFFF; // INT32_MAX
    
    // Initialize wait queue
    init_wait_queue(&sem->wait_queue, name, WAIT_QUEUE_SEMAPHORE, sem);
    
    semaphore_count++;
    return sem;
}

/*
 * Destroy a semaphore
 */
void destroy_semaphore(void* sem_ptr) {
    semaphore_t sem = (semaphore_t)sem_ptr;
    
    if (!sem || sem->header.type != IPC_TYPE_SEMAPHORE) {
        return;
    }
    
    // Wake up all waiters
    wake_waiters(&sem->wait_queue, 0);
    
    // Clear the semaphore
    memset(sem, 0, sizeof(struct semaphore));
    
    semaphore_count--;
}

/*
 * Wait on a semaphore (blocking)
 */
int semaphore_wait(semaphore_t sem) {
    if (!sem || sem->header.type != IPC_TYPE_SEMAPHORE) {
        return -1;
    }
    
    pid_t current_pid = get_current_pid();
    
    // Disable interrupts for atomic operation
    __asm__ volatile("cli");
    
    // If semaphore value is greater than 0, decrement and return
    if (sem->value > 0) {
        sem->value--;
        __asm__ volatile("sti");
        return 0;
    }
    
    // Semaphore is at 0, need to block
    add_waiter(&sem->wait_queue, current_pid, 0);
    
    // Re-enable interrupts before blocking
    __asm__ volatile("sti");
    
    // Block the current task
    block_task(current_pid);
    
    // When we're unblocked, the semaphore has been decremented for us
    return 0;
}

/*
 * Try to wait on a semaphore (non-blocking)
 */
int semaphore_trywait(semaphore_t sem) {
    if (!sem || sem->header.type != IPC_TYPE_SEMAPHORE) {
        return -1;
    }
    
    // Disable interrupts for atomic operation
    __asm__ volatile("cli");
    
    // If semaphore value is greater than 0, decrement and return success
    if (sem->value > 0) {
        sem->value--;
        __asm__ volatile("sti");
        return 0;
    }
    
    // Semaphore is at 0, cannot wait
    __asm__ volatile("sti");
    return -1;
}

/*
 * Post to a semaphore (increment)
 */
int semaphore_post(semaphore_t sem) {
    if (!sem || sem->header.type != IPC_TYPE_SEMAPHORE) {
        return -1;
    }
    
    // Disable interrupts for atomic operation
    __asm__ volatile("cli");
    
    // Check if we're at maximum value
    if (sem->value == sem->max_value) {
        __asm__ volatile("sti");
        return -1;
    }
    
    // If there are waiters, wake one instead of incrementing
    if (sem->wait_queue.waiter_count > 0) {
        wake_waiters(&sem->wait_queue, 1);
    } else {
        // No waiters, just increment the value
        sem->value++;
    }
    
    __asm__ volatile("sti");
    return 0;
}

/*
 * Get the current value of a semaphore
 */
int semaphore_getvalue(semaphore_t sem, int* value) {
    if (!sem || sem->header.type != IPC_TYPE_SEMAPHORE || !value) {
        return -1;
    }
    
    // Disable interrupts for atomic read
    __asm__ volatile("cli");
    *value = sem->value;
    __asm__ volatile("sti");
    
    return 0;
}

/*
 * Periodic check for timeouts in all wait queues
 * (Called from timer interrupt handler)
 */
void check_all_timeouts(void) {
    // Check mutex timeouts
    for (uint32_t i = 0; i < MAX_WAIT_QUEUES; i++) {
        if (mutex_pool[i].header.type == IPC_TYPE_MUTEX) {
            check_timeouts(&mutex_pool[i].wait_queue);
        }
    }
    
    // Check semaphore timeouts
    for (uint32_t i = 0; i < MAX_WAIT_QUEUES; i++) {
        if (semaphore_pool[i].header.type == IPC_TYPE_SEMAPHORE) {
            check_timeouts(&semaphore_pool[i].wait_queue);
        }
    }
    
    // Other types of wait queues will be checked as they're implemented
}

/*
 * Initialize the IPC subsystem
 */
void init_ipc(void) {
    kernel_printf("Initializing IPC subsystem...\n");
    
    // Initialize mutex pool
    memset(mutex_pool, 0, sizeof(mutex_pool));
    mutex_count = 0;
    
    // Initialize semaphore pool
    memset(semaphore_pool, 0, sizeof(semaphore_pool));
    semaphore_count = 0;
    
    // Create initial kernel synchronization objects
    mutex_t kernel_mutex = create_mutex("kernel_mutex");
    if (!kernel_mutex) {
        kernel_panic("Failed to create kernel mutex");
    }
    
    semaphore_t kernel_sem = create_semaphore("kernel_semaphore", 1);
    if (!kernel_sem) {
        kernel_panic("Failed to create kernel semaphore");
    }
    
    kernel_printf("IPC subsystem initialized: %d mutexes, %d semaphores\n", 
                 mutex_count, semaphore_count);
}
