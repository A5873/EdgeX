/*
 * EdgeX OS - Task Scheduler Implementation
 *
 * This file implements the task scheduling subsystem, including
 * task creation, context switching, and the scheduling algorithm.
 */

#include <edgex/kernel.h>
#include <edgex/scheduler.h>
#include <edgex/memory.h>
#include <edgex/interrupt.h>

/* Default kernel stack size for tasks (64KB) */
#define DEFAULT_KERNEL_STACK_SIZE (64 * 1024)

/* Default time slice in timer ticks */
#define DEFAULT_TIME_SLICE 10

/* Scheduler state */
static struct {
    task_t* current_task;         /* Currently running task */
    task_t* idle_task_tcb;        /* Idle task TCB */
    task_t* task_list_head;       /* Head of all tasks list */
    
    /* Task queues */
    task_t* ready_queue[5];       /* Ready queues by priority (indexed by task_priority_t) */
    task_t* blocked_queue;        /* Queue of blocked tasks */
    task_t* sleeping_queue;       /* Queue of sleeping tasks */
    
    /* Stats and counters */
    uint64_t tick_count;          /* Number of timer ticks since boot */
    pid_t next_pid;               /* Next available PID */
    uint32_t task_count;          /* Total number of tasks */
    uint32_t ready_count;         /* Number of tasks in ready state */
    uint32_t blocked_count;       /* Number of tasks in blocked state */
    uint32_t sleeping_count;      /* Number of tasks in sleeping state */
    
    /* Scheduling flags */
    bool scheduler_running;       /* Is the scheduler actively running? */
    bool preemption_enabled;      /* Is preemptive multitasking enabled? */
    bool in_critical_section;     /* Are we in a critical section? */
} scheduler;

/* Forward declarations */
static void idle_task_function(void);
static void timer_tick_handler(cpu_context_t* context);
static void yield_handler(cpu_context_t* context);
static void switch_to_task(task_t* task);
static void schedule_next_task(void);
static void add_task_to_ready_queue(task_t* task);
static void remove_task_from_queue(task_t** queue, task_t* task);
static task_t* get_next_ready_task(void);
static task_t* create_task(const char* name, void (*entry_point)(void), 
                           task_priority_t priority, uint32_t flags);

/*
 * Assembly for context switching
 */
extern void context_switch(task_context_t** old_context, task_context_t* new_context);

__asm__(
    ".global context_switch\n"
    ".type context_switch, @function\n"
    "context_switch:\n"
    "    # Save current context\n"
    "    pushq %rbp\n"
    "    movq %rsp, %rbp\n"
    
    "    # Save all registers to the stack\n"
    "    pushq %rax\n"
    "    pushq %rbx\n"
    "    pushq %rcx\n"
    "    pushq %rdx\n"
    "    pushq %rsi\n"
    "    pushq %rdi\n"
    "    pushq %r8\n"
    "    pushq %r9\n"
    "    pushq %r10\n"
    "    pushq %r11\n"
    "    pushq %r12\n"
    "    pushq %r13\n"
    "    pushq %r14\n"
    "    pushq %r15\n"
    
    "    # Get RFLAGS and save it\n"
    "    pushfq\n"
    
    "    # Save stack pointer to old context\n"
    "    movq (%rdi), %rax\n"     /* rax = *old_context */
    "    movq %rsp, (%rax)\n"     /* *old_context->rsp = rsp */
    
    "    # Load new context\n"
    "    movq %rsi, %rsp\n"       /* rsp = new_context */
    
    "    # Restore RFLAGS\n"
    "    popfq\n"
    
    "    # Restore registers\n"
    "    popq %r15\n"
    "    popq %r14\n"
    "    popq %r13\n"
    "    popq %r12\n"
    "    popq %r11\n"
    "    popq %r10\n"
    "    popq %r9\n"
    "    popq %r8\n"
    "    popq %rdi\n"
    "    popq %rsi\n"
    "    popq %rdx\n"
    "    popq %rcx\n"
    "    popq %rbx\n"
    "    popq %rax\n"
    
    "    # Return\n"
    "    leave\n"
    "    ret\n"
);

/*
 * Enter a critical section - disable interrupts and set flag
 */
static inline void enter_critical(void) {
    __asm__ volatile("cli");
    scheduler.in_critical_section = true;
}

/*
 * Exit a critical section - clear flag and enable interrupts
 */
static inline void exit_critical(void) {
    scheduler.in_critical_section = false;
    __asm__ volatile("sti");
}

/*
 * Get current task pointer
 */
task_t* get_current_task(void) {
    return scheduler.current_task;
}

/*
 * Get current task PID
 */
pid_t get_current_pid(void) {
    if (scheduler.current_task) {
        return scheduler.current_task->pid;
    }
    return PID_INVALID;
}

/*
 * Get system tick count
 */
uint64_t get_tick_count(void) {
    return scheduler.tick_count;
}

/*
 * Add a task to a queue (at the end)
 */
static void add_task_to_queue(task_t** queue, task_t* task) {
    if (!task) {
        return;
    }
    
    // Reset task's queue pointers
    task->next = NULL;
    task->prev = NULL;
    
    // If queue is empty, this becomes the first task
    if (*queue == NULL) {
        *queue = task;
        return;
    }
    
    // Find the last task in the queue
    task_t* current = *queue;
    while (current->next != NULL) {
        current = current->next;
    }
    
    // Link the new task at the end
    current->next = task;
    task->prev = current;
}

/*
 * Remove a task from a queue
 */
static void remove_task_from_queue(task_t** queue, task_t* task) {
    if (!task || !(*queue)) {
        return;
    }
    
    // If task is at the head of the queue
    if (*queue == task) {
        *queue = task->next;
        if (*queue) {
            (*queue)->prev = NULL;
        }
    } else {
        // Task is in the middle or at the end
        if (task->prev) {
            task->prev->next = task->next;
        }
        if (task->next) {
            task->next->prev = task->prev;
        }
    }
    
    // Clear the task's queue pointers
    task->next = NULL;
    task->prev = NULL;
}

/*
 * Add a task to the ready queue based on its priority
 */
static void add_task_to_ready_queue(task_t* task) {
    if (!task) {
        return;
    }
    
    task_priority_t priority = task->priority;
    
    // Validate priority
    if (priority > TASK_PRIORITY_REALTIME) {
        priority = TASK_PRIORITY_NORMAL;
    }
    
    // Add to appropriate ready queue
    add_task_to_queue(&scheduler.ready_queue[priority], task);
    
    // Update task state and counters
    task->state = TASK_STATE_READY;
    scheduler.ready_count++;
}

/*
 * Get the highest priority task from the ready queues
 */
static task_t* get_next_ready_task(void) {
    // Start from highest priority queue
    for (int priority = TASK_PRIORITY_REALTIME; priority >= TASK_PRIORITY_IDLE; priority--) {
        if (scheduler.ready_queue[priority]) {
            // Get the first task in this priority queue
            task_t* task = scheduler.ready_queue[priority];
            
            // Remove it from the ready queue
            remove_task_from_queue(&scheduler.ready_queue[priority], task);
            
            // Update counters
            scheduler.ready_count--;
            
            return task;
        }
    }
    
    // No tasks ready, return idle task
    return scheduler.idle_task_tcb;
}

/*
 * Create a new task stack frame for initial execution
 */
static task_context_t* setup_initial_stack(task_t* task, void (*entry_point)(void)) {
    // Calculate where the stack top would be
    uintptr_t stack_top = (uintptr_t)task->kernel_stack + task->kernel_stack_size;
    
    // Align stack to 16-byte boundary (x86_64 ABI requirement)
    stack_top &= ~15ULL;
    
    // Leave space for the task context
    stack_top -= sizeof(task_context_t);
    
    // Initialize the task context
    task_context_t* context = (task_context_t*)stack_top;
    
    // Zero out the context
    memset(context, 0, sizeof(task_context_t));
    
    // Set up initial register values
    context->rip = (uint64_t)entry_point;    // Entry point
    context->rflags = 0x202;                 // IF flag set (interrupts enabled)
    context->cs = 0x08;                      // Kernel code segment
    context->ss = 0x10;                      // Kernel data segment
    context->rsp = stack_top;                // Set stack pointer to just below the context
    
    return context;
}

/*
 * Create a new task
 */
static task_t* create_task(const char* name, void (*entry_point)(void), 
                           task_priority_t priority, uint32_t flags) {
    enter_critical();
    
    // Allocate and zero TCB
    task_t* task = (task_t*)kmalloc(sizeof(task_t));
    if (!task) {
        kernel_printf("Failed to allocate memory for task.\n");
        exit_critical();
        return NULL;
    }
    memset(task, 0, sizeof(task_t));
    
    // Allocate stack
    task->kernel_stack_size = DEFAULT_KERNEL_STACK_SIZE;
    task->kernel_stack = kmalloc(task->kernel_stack_size);
    if (!task->kernel_stack) {
        kernel_printf("Failed to allocate kernel stack for task.\n");
        kfree(task);
        exit_critical();
        return NULL;
    }
    
    // Initialize the task fields
    task->pid = scheduler.next_pid++;
    strncpy(task->name, name, sizeof(task->name) - 1);
    task->state = TASK_STATE_READY;
    task->priority = priority;
    task->flags = flags;
    task->time_slice = DEFAULT_TIME_SLICE;
    task->remaining_ticks = task->time_slice;
    
    // For now, all tasks use the kernel's page directory
    task->page_dir = get_kernel_page_directory();
    
    // Setup initial stack frame and context
    task->context = setup_initial_stack(task, entry_point);
    
    // Add to the global task list
    add_task_to_queue(&scheduler.task_list_head, task);
    scheduler.task_count++;
    
    // Add to the ready queue
    add_task_to_ready_queue(task);
    
    kernel_printf("Created task %s with PID %d\n", task->name, task->pid);
    
    exit_critical();
    return task;
}

/*
 * Create a kernel task
 */
pid_t create_kernel_task(const char* name, void (*entry_point)(void), task_priority_t priority) {
    task_t* task = create_task(name, entry_point, priority, TASK_FLAG_KERNEL);
    return task ? task->pid : PID_INVALID;
}

/*
 * Create a user task (simplified, doesn't setup user space yet)
 */
pid_t create_user_task(const char* name, void (*entry_point)(void), task_priority_t priority) {
    // For now, user tasks are created like kernel tasks 
    // but with the USER flag set
    task_t* task = create_task(name, entry_point, priority, TASK_FLAG_USER);
    return task ? task->pid : PID_INVALID;
}

/*
 * Find a task by PID
 */
static task_t* find_task_by_pid(pid_t pid) {
    task_t* task = scheduler.task_list_head;
    while (task) {
        if (task->pid == pid) {
            return task;
        }
        task = task->next;
    }
    return NULL;
}

/*
 * Terminate a task
 */
void terminate_task(pid_t pid) {
    enter_critical();
    
    // Can't terminate PID 0 or 1 (kernel tasks)
    if (pid <= PID_KERNEL) {
        exit_critical();
        return;
    }
    
    // Find the task
    task_t* task = find_task_by_pid(pid);
    if (!task) {
        exit_critical();
        return;
    }
    
    // Mark task as terminated
    task->state = TASK_STATE_TERMINATED;
    
    // Remove from any queue
    for (int i = 0; i < 5; i++) {
        remove_task_from_queue(&scheduler.ready_queue[i], task);
    }
    remove_task_from_queue(&scheduler.blocked_queue, task);
    remove_task_from_queue(&scheduler.sleeping_queue, task);
    
    // Clean up task resources
    // (For now, we just mark it as terminated but don't free its memory
    // since we might need to access its exit status or other data)
    
    // If terminating current task, schedule next task
    if (scheduler.current_task == task) {
        schedule_next_task();
    }
    
    exit_critical();
}

/*
 * Exit current task
 */
void exit_task(void) {
    if (scheduler.current_task) {
        terminate_task(scheduler.current_task->pid);
    }
    // Should never reach here if current task, but just in case
    while (1) {
        __asm__ volatile("hlt");
    }
}

/*
 * Switch to a specific task
 */
static void switch_to_task(task_t* task) {
    if (!task) {
        return;
    }
    
    // If already running this task, do nothing
    if (scheduler.current_task == task) {
        return;
    }
    
    // Update task state
    if (task->state != TASK_STATE_RUNNING) {
        task->state = TASK_STATE_RUNNING;
    }
    
    // Reset time slice
    task->remaining_ticks = task->time_slice;
    
    // Save old task pointer
    task_t* old_task = scheduler.current_task;
    
    // Update current task
    scheduler.current_task = task;
    
    // If no previous task (first run), just load the new context
    if (!old_task) {
        // Switch to the new task's page directory if needed
        if (task->page_dir) {
            switch_page_directory(task->page_dir);
        }
        
        // Load the initial context
        __asm__ volatile(
            "movq %0, %%rsp\n"   // Load stack pointer
            "popq %%r15\n"       // Restore registers
            "popq %%r14\n"
            "popq %%r13\n"
            "popq %%r12\n"
            "popq %%r11\n"
            "popq %%r10\n"
            "popq %%r9\n"
            "popq %%r8\n"
            "popq %%rdi\n"
            "popq %%rsi\n"
            "popq %%rdx\n"
            "popq %%rcx\n"
            "popq %%rbx\n"
            "popq %%rax\n"
            "iretq\n"            // Return to new task
            : : "g"(task->context)
        );
    } else {
        // Save old task state if it's still active
        if (old_task->state != TASK_STATE_TERMINATED) {
            if (old_task->state == TASK_STATE_RUNNING) {
                old_task->state = TASK_STATE_READY;
                add_task_to_ready_queue(old_task);
            }
        }
        
        // Switch to the new task's page directory if needed
        if (task->page_dir && old_task->page_dir != task->page_dir) {
            switch_page_directory(task->page_dir);
        }
        
        // Perform context switch
        context_switch(&old_task->context, task->context);
    }
}

/*
 * Schedule the next task to run
 */
static void schedule_next_task(void) {
    // If scheduler is not running yet, do nothing
    if (!scheduler.scheduler_running) {
        return;
    }
    
    // Get the next ready task
    task_t* next_task = get_next_ready_task();
    
    // Switch to the next task
    switch_to_task(next_task);
}

/*
 * Main scheduling function - pick next task and switch to it
 */
void schedule(void) {
    enter_critical();
    
    // Check if we can actually schedule
    if (!scheduler.scheduler_running || scheduler.in_critical_section) {
        exit_critical();
        return;
    }
    
    // Schedule the next task
    schedule_next_task();
    
    exit_critical();
}

/*
 * Yield - voluntarily give up CPU time
 */
void yield(void) {
    // Just trigger a schedule
    schedule();
}

/*
 * Yield interrupt handler
 */
static void yield_handler(cpu_context_t* context) {
    (void)context; // Unused parameter
    
    // The handler simply calls schedule
    schedule();
}

/*
 * Put the current task to sleep for the specified number of milliseconds
 */
void sleep_task(uint64_t milliseconds) {
    enter_critical();
    
    // Get current task
    task_t* task = scheduler.current_task;
    if (!task || task == scheduler.idle_task_tcb) {
        exit_critical();
        return;
    }
    
    // Calculate wake tick
    // Assuming a 1000Hz timer (1ms per tick), otherwise scale accordingly
    uint64_t wake_tick = scheduler.tick_count + milliseconds;
    task->wake_tick = wake_tick;
    
    // Update task state
    task->state = TASK_STATE_SLEEPING;
    
    // Remove from any queue and add to sleeping queue
    for (int i = 0; i < 5; i++) {
        remove_task_from_queue(&scheduler.ready_queue[i], task);
    }
    add_task_to_queue(&scheduler.sleeping_queue, task);
    scheduler.sleeping_count++;
    
    // Schedule another task
    schedule_next_task();
    
    exit_critical();
}

/*
 * Wake up a sleeping task
 */
void wake_task(pid_t pid) {
    enter_critical();
    
    // Find the task
    task_t* task = find_task_by_pid(pid);
    if (!task || task->state != TASK_STATE_SLEEPING) {
        exit_critical();
        return;
    }
    
    // Remove from sleeping queue
    remove_task_from_queue(&scheduler.sleeping_queue, task);
    scheduler.sleeping_count--;
    
    // Add to ready queue
    add_task_to_ready_queue(task);
    
    exit_critical();
}

/*
 * Block a task (waiting for an event)
 */
void block_task(pid_t pid) {
    enter_critical();
    
    // Find the task
    task_t* task = find_task_by_pid(pid);
    if (!task || task == scheduler.idle_task_tcb) {
        exit_critical();
        return;
    }
    
    // Update task state
    task->state = TASK_STATE_BLOCKED;
    
    // Remove from any queue and add to blocked queue
    for (int i = 0; i < 5; i++) {
        remove_task_from_queue(&scheduler.ready_queue[i], task);
    }
    add_task_to_queue(&scheduler.blocked_queue, task);
    scheduler.blocked_count++;
    
    // If blocking current task, schedule next task
    if (scheduler.current_task == task) {
        schedule_next_task();
    }
    
    exit_critical();
}

/*
 * Unblock a task (resume after an event)
 */
void unblock_task(pid_t pid) {
    enter_critical();
    
    // Find the task
    task_t* task = find_task_by_pid(pid);
    if (!task || task->state != TASK_STATE_BLOCKED) {
        exit_critical();
        return;
    }
    
    // Remove from blocked queue
    remove_task_from_queue(&scheduler.blocked_queue, task);
    scheduler.blocked_count--;
    
    // Add to ready queue
    add_task_to_ready_queue(task);
    
    exit_critical();
}

/*
 * Check sleeping tasks and wake up any that have reached their wake time
 */
static void check_sleeping_tasks(void) {
    task_t* task = scheduler.sleeping_queue;
    task_t* next;
    
    while (task) {
        next = task->next; // Save next pointer before potentially removing from queue
        
        // Check if it's time to wake up the task
        if (scheduler.tick_count >= task->wake_tick) {
            // Remove from sleeping queue
            remove_task_from_queue(&scheduler.sleeping_queue, task);
            scheduler.sleeping_count--;
            
            // Add to ready queue
            add_task_to_ready_queue(task);
        }
        
        task = next;
    }
}

/*
 * Timer tick handler - called on each timer interrupt
 */
static void timer_tick_handler(cpu_context_t* context) {
    (void)context; // Unused parameter
    
    // Increment tick count
    scheduler.tick_count++;
    
    // Check sleeping tasks
    if (scheduler.sleeping_count > 0) {
        check_sleeping_tasks();
    }
    
    // If preemption is not enabled, just return
    if (!scheduler.preemption_enabled) {
        return;
    }
    
    // Decrease time slice counter for current task
    if (scheduler.current_task && 
        scheduler.current_task != scheduler.idle_task_tcb &&
        scheduler.current_task->state == TASK_STATE_RUNNING) {
        
        scheduler.current_task->total_ticks++;
        
        if (scheduler.current_task->remaining_ticks > 0) {
            scheduler.current_task->remaining_ticks--;
        }
        
        // If time slice expired, schedule next task
        if (scheduler.current_task->remaining_ticks == 0) {
            schedule();
        }
    }
}

/*
 * Idle task function - runs when no other tasks are ready
 */
static void idle_task_function(void) {
    while (1) {
        // Halt the CPU until next interrupt (save power)
        __asm__ volatile("hlt");
    }
}

/*
 * Initialize the scheduler
 */
void init_scheduler(void) {
    // Zero out scheduler state
    memset(&scheduler, 0, sizeof(scheduler));
    
    // Initialize PID assignment
    scheduler.next_pid = PID_KERNEL + 1; // Start at 2 (kernel is 1)
    
    // Create the idle task
    scheduler.idle_task_tcb = create_task("idle", idle_task_function, 
                                          TASK_PRIORITY_IDLE, TASK_FLAG_KERNEL | TASK_FLAG_IDLE);
    
    if (!scheduler.idle_task_tcb) {
        kernel_panic("Failed to create idle task!");
        return;
    }
    
    // Register interrupt handlers
    register_irq_handler(IRQ_TIMER, timer_tick_handler);
    register_isr_handler(INT_VECTOR_YIELD, yield_handler);
    
    // Enable timer IRQ
    enable_irq(IRQ_TIMER);
    
    // Mark scheduler as running
    scheduler.scheduler_running = true;
    
    // Enable preemption
    scheduler.preemption_enabled = true;
    
    kernel_printf("Scheduler initialized successfully\n");
    
    // Start the idle task
    switch_to_task(scheduler.idle_task_tcb);
}
