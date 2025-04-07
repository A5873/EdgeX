/*
 * EdgeX OS - Kernel Initialization
 *
 * This file contains the main kernel entry point and initialization code.
 * It sets up core kernel subsystems and starts the scheduler.
 */

#include <edgex/kernel.h>
#include <edgex/memory.h>
#include <edgex/interrupt.h>
#include <edgex/scheduler.h>

/* PIT (Programmable Interval Timer) ports */
#define PIT_DATA_PORT_0    0x40
#define PIT_DATA_PORT_1    0x41
#define PIT_DATA_PORT_2    0x42
#define PIT_COMMAND_PORT   0x43

/* PIT command register bits */
#define PIT_CHANNEL_0      0x00    /* Channel 0 select */
#define PIT_ACCESS_BOTH    0x30    /* Access mode: lobyte/hibyte */
#define PIT_MODE_SQUARE    0x06    /* Mode 3: square wave generator */
#define PIT_MODE_ONESHOT   0x00    /* Mode 0: one-shot mode */
#define PIT_MODE_RATE      0x04    /* Mode 2: rate generator */

/* PIT timing values */
#define PIT_FREQUENCY      1193182 /* Base frequency of the PIT in Hz */
#define TIMER_FREQUENCY    1000    /* Desired timer frequency in Hz (1ms per tick) */
#define PIT_DIVISOR        (PIT_FREQUENCY / TIMER_FREQUENCY)

/* Forward declarations for test tasks */
static void test_task_1(void);
static void test_task_2(void);
static void test_task_3(void);

/*
 * Initialize PIT for scheduling timer
 */
static void init_pit(void) {
    kernel_printf("Initializing PIT...\n");
    
    /* Calculate divisor for desired frequency */
    uint16_t divisor = PIT_DIVISOR;
    
    /* Set command byte: channel 0, lobyte/hibyte access, square wave mode */
    outb(PIT_COMMAND_PORT, PIT_CHANNEL_0 | PIT_ACCESS_BOTH | PIT_MODE_SQUARE);
    
    /* Set divisor */
    outb(PIT_DATA_PORT_0, divisor & 0xFF);         /* Low byte */
    outb(PIT_DATA_PORT_0, (divisor >> 8) & 0xFF);  /* High byte */
    
    kernel_printf("PIT initialized at %d Hz\n", TIMER_FREQUENCY);
}

/*
 * Initialize kernel subsystems
 */
static void init_kernel(void) {
    /* Initialize memory management subsystem */
    kernel_printf("Initializing memory management...\n");
    init_memory();
    
    /* Initialize interrupt subsystem */
    kernel_printf("Initializing interrupt handling...\n");
    init_interrupts();
    
    /* Initialize timer */
    init_pit();
    
    /* Initialize scheduler */
    kernel_printf("Initializing task scheduler...\n");
    init_scheduler();
    
    /* Create test tasks */
    kernel_printf("Creating test tasks...\n");
    pid_t pid1 = create_kernel_task("test1", test_task_1, TASK_PRIORITY_NORMAL);
    pid_t pid2 = create_kernel_task("test2", test_task_2, TASK_PRIORITY_LOW);
    pid_t pid3 = create_kernel_task("test3", test_task_3, TASK_PRIORITY_HIGH);
    
    kernel_printf("Created test tasks with PIDs: %d, %d, %d\n", pid1, pid2, pid3);
}

/*
 * Test task 1 - Normal priority
 */
static void test_task_1(void) {
    int counter = 0;
    while (1) {
        kernel_printf("Task 1 (Normal): %d\n", counter++);
        
        // Sleep for a bit
        sleep_task(1000);
    }
}

/*
 * Test task 2 - Low priority
 */
static void test_task_2(void) {
    int counter = 0;
    while (1) {
        kernel_printf("Task 2 (Low): %d\n", counter++);
        
        // Sleep for a bit
        sleep_task(1500);
    }
}

/*
 * Test task 3 - High priority
 */
static void test_task_3(void) {
    int counter = 0;
    while (1) {
        kernel_printf("Task 3 (High): %d\n", counter++);
        
        // Sleep for a bit
        sleep_task(500);
        
        // Every 5 iterations, yield to demonstrate cooperative multitasking
        if (counter % 5 == 0) {
            kernel_printf("Task 3: voluntarily yielding CPU\n");
            yield();
        }
    }
}

/*
 * Print OS banner
 */
static void print_banner(void) {
    kernel_printf("\n");
    kernel_printf("==========================================\n");
    kernel_printf("        EdgeX Operating System            \n");
    kernel_printf("          Version 0.1.0-alpha             \n");
    kernel_printf("==========================================\n");
    kernel_printf("\n");
}

/*
 * Main kernel entry point
 * This function is called from the boot assembly code
 */
void kernel_main(void) {
    /* Basic early initialization (printing, etc.) happens before kernel_main() */
    
    /* Print banner */
    print_banner();
    
    /* Initialize kernel subsystems */
    kernel_printf("Starting kernel initialization...\n");
    init_kernel();
    
    /* 
     * The scheduler has been started, and it will automatically switch to 
     * the next ready task or the idle task if no tasks are ready.
     * 
     * This function will never return because the scheduler takes over.
     * If it somehow does return, panic the kernel.
     */
    kernel_panic("kernel_main returned unexpectedly!");
}

