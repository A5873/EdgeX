/*
 * EdgeX OS - Task Scheduler
 *
 * This file defines the interface for the task scheduling subsystem,
 * including task structures, state transitions, and scheduling operations.
 */

#ifndef EDGEX_SCHEDULER_H
#define EDGEX_SCHEDULER_H

#include <edgex/kernel.h>
#include <edgex/interrupt.h>
#include <edgex/memory.h>

/* Task/Process ID type */
typedef uint32_t pid_t;

/* Special PID values */
#define PID_INVALID 0
#define PID_KERNEL  1

/* Task states */
typedef enum {
    TASK_STATE_READY = 0,    /* Task is ready to run */
    TASK_STATE_RUNNING,      /* Task is currently running */
    TASK_STATE_BLOCKED,      /* Task is blocked waiting for a resource */
    TASK_STATE_SLEEPING,     /* Task is sleeping for a specific duration */
    TASK_STATE_TERMINATED    /* Task has terminated execution */
} task_state_t;

/* Task priorities */
typedef enum {
    TASK_PRIORITY_IDLE = 0,      /* Lowest priority, idle task */
    TASK_PRIORITY_LOW = 1,       /* Low priority tasks */
    TASK_PRIORITY_NORMAL = 2,    /* Normal priority */
    TASK_PRIORITY_HIGH = 3,      /* High priority */
    TASK_PRIORITY_REALTIME = 4   /* Highest priority, real-time */
} task_priority_t;

/* Task flags */
#define TASK_FLAG_KERNEL     (1 << 0)  /* Kernel task (runs in kernel mode) */
#define TASK_FLAG_USER       (1 << 1)  /* User task (runs in user mode) */
#define TASK_FLAG_IDLE       (1 << 2)  /* Idle task (runs when nothing else can) */

/* Task context structure - CPU state saved during context switch */
typedef struct {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} task_context_t;

/*
 * Task Control Block (TCB) structure
 * Contains all information about a task
 */
typedef struct task {
    /* Task identification */
    pid_t pid;                    /* Process ID */
    char name[32];                /* Task name */
    
    /* Task state */
    task_state_t state;           /* Current task state */
    task_priority_t priority;     /* Task priority */
    uint32_t flags;               /* Task flags */
    
    /* Memory management */
    void* kernel_stack;           /* Kernel stack pointer */
    uint64_t kernel_stack_size;   /* Size of kernel stack */
    page_directory_t* page_dir;   /* Page directory for this task */
    
    /* CPU state */
    task_context_t* context;      /* Saved CPU context during context switch */
    
    /* Scheduling information */
    uint64_t time_slice;          /* Time slice in timer ticks */
    uint64_t remaining_ticks;     /* Remaining ticks in current time slice */
    uint64_t total_ticks;         /* Total ticks consumed by this task */
    uint64_t wake_tick;           /* Tick when sleeping task should wake up */
    
    /* Linked list pointers for task queue */
    struct task* next;            /* Next task in queue */
    struct task* prev;            /* Previous task in queue */
} task_t;

/*
 * Scheduler function declarations
 */

/* Initialization */
void init_scheduler(void);

/* Task management */
pid_t create_kernel_task(const char* name, void (*entry_point)(void), task_priority_t priority);
pid_t create_user_task(const char* name, void (*entry_point)(void), task_priority_t priority);
void terminate_task(pid_t pid);
void exit_task(void);

/* Scheduling operations */
void schedule(void);
void yield(void);
task_t* get_current_task(void);
pid_t get_current_pid(void);

/* Task state changes */
void sleep_task(uint64_t milliseconds);
void wake_task(pid_t pid);
void block_task(pid_t pid);
void unblock_task(pid_t pid);
void suspend_task(pid_t pid);
void resume_task(pid_t pid);

/* Timer-related functions */
void timer_tick(void);
uint64_t get_tick_count(void);

/* Idle task */
void idle_task(void);

#endif /* EDGEX_SCHEDULER_H */
