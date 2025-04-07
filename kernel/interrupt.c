/*
 * EdgeX OS - Interrupt Handling Subsystem
 * 
 * This file implements the interrupt handling system for EdgeX OS.
 * It includes IDT setup, exception handling, PIC programming, and
 * IRQ management.
 */

#include <edgex/kernel.h>
#include <edgex/interrupt.h>

/* IDT and IDT register */
static idt_entry_t idt[IDT_ENTRIES];
static idtr_t idtr;

/* Handler tables */
static isr_handler_t exception_handlers[32];
static irq_handler_t irq_handlers[16];
static isr_handler_t isr_handlers[IDT_ENTRIES];

/* IRQ mask tracking */
static uint16_t irq_mask = 0xFFFF; /* All IRQs masked initially */

/* Default code segment (set in boot code) */
#define KERNEL_CODE_SEGMENT 0x08

/* Forward declarations for assembly stubs */
extern void isr_stub_0(void);
extern void isr_stub_1(void);
extern void isr_stub_2(void);
extern void isr_stub_3(void);
extern void isr_stub_4(void);
extern void isr_stub_5(void);
extern void isr_stub_6(void);
extern void isr_stub_7(void);
extern void isr_stub_8(void);
extern void isr_stub_9(void);
extern void isr_stub_10(void);
extern void isr_stub_11(void);
extern void isr_stub_12(void);
extern void isr_stub_13(void);
extern void isr_stub_14(void);
extern void isr_stub_15(void);
extern void isr_stub_16(void);
extern void isr_stub_17(void);
extern void isr_stub_18(void);
extern void isr_stub_19(void);
extern void isr_stub_20(void);
extern void isr_stub_21(void);
extern void isr_stub_22(void);
extern void isr_stub_23(void);
extern void isr_stub_24(void);
extern void isr_stub_25(void);
extern void isr_stub_26(void);
extern void isr_stub_27(void);
extern void isr_stub_28(void);
extern void isr_stub_29(void);
extern void isr_stub_30(void);
extern void isr_stub_31(void);

extern void irq_stub_0(void);
extern void irq_stub_1(void);
extern void irq_stub_2(void);
extern void irq_stub_3(void);
extern void irq_stub_4(void);
extern void irq_stub_5(void);
extern void irq_stub_6(void);
extern void irq_stub_7(void);
extern void irq_stub_8(void);
extern void irq_stub_9(void);
extern void irq_stub_10(void);
extern void irq_stub_11(void);
extern void irq_stub_12(void);
extern void irq_stub_13(void);
extern void irq_stub_14(void);
extern void irq_stub_15(void);

extern void isr_stub_syscall(void);
extern void isr_stub_yield(void);

/* Array of ISR stub addresses, indexed by vector number */
static void* isr_stubs[IDT_ENTRIES] = {
    /* CPU Exceptions (0-31) */
    [0x00] = isr_stub_0,    [0x01] = isr_stub_1,    [0x02] = isr_stub_2,    [0x03] = isr_stub_3,
    [0x04] = isr_stub_4,    [0x05] = isr_stub_5,    [0x06] = isr_stub_6,    [0x07] = isr_stub_7,
    [0x08] = isr_stub_8,    [0x09] = isr_stub_9,    [0x0A] = isr_stub_10,   [0x0B] = isr_stub_11,
    [0x0C] = isr_stub_12,   [0x0D] = isr_stub_13,   [0x0E] = isr_stub_14,   [0x0F] = isr_stub_15,
    [0x10] = isr_stub_16,   [0x11] = isr_stub_17,   [0x12] = isr_stub_18,   [0x13] = isr_stub_19,
    [0x14] = isr_stub_20,   [0x15] = isr_stub_21,   [0x16] = isr_stub_22,   [0x17] = isr_stub_23,
    [0x18] = isr_stub_24,   [0x19] = isr_stub_25,   [0x1A] = isr_stub_26,   [0x1B] = isr_stub_27,
    [0x1C] = isr_stub_28,   [0x1D] = isr_stub_29,   [0x1E] = isr_stub_30,   [0x1F] = isr_stub_31,
    
    /* IRQs (32-47) */
    [0x20] = irq_stub_0,    [0x21] = irq_stub_1,    [0x22] = irq_stub_2,    [0x23] = irq_stub_3,
    [0x24] = irq_stub_4,    [0x25] = irq_stub_5,    [0x26] = irq_stub_6,    [0x27] = irq_stub_7,
    [0x28] = irq_stub_8,    [0x29] = irq_stub_9,    [0x2A] = irq_stub_10,   [0x2B] = irq_stub_11,
    [0x2C] = irq_stub_12,   [0x2D] = irq_stub_13,   [0x2E] = irq_stub_14,   [0x2F] = irq_stub_15,
    
    /* Software interrupts */
    [0x80] = isr_stub_syscall,
    [0x81] = isr_stub_yield
};

/*
 * Names of CPU exceptions, used for error reporting
 */
static const char* exception_names[] = {
    [0x00] = "Divide Error",
    [0x01] = "Debug Exception",
    [0x02] = "NMI Interrupt",
    [0x03] = "Breakpoint",
    [0x04] = "Overflow",
    [0x05] = "BOUND Range Exceeded",
    [0x06] = "Invalid Opcode",
    [0x07] = "Device Not Available",
    [0x08] = "Double Fault",
    [0x09] = "Coprocessor Segment Overrun",
    [0x0A] = "Invalid TSS",
    [0x0B] = "Segment Not Present",
    [0x0C] = "Stack Fault",
    [0x0D] = "General Protection",
    [0x0E] = "Page Fault",
    [0x0F] = "Reserved",
    [0x10] = "x87 FPU Error",
    [0x11] = "Alignment Check",
    [0x12] = "Machine Check",
    [0x13] = "SIMD Floating-Point Exception",
    [0x14] = "Virtualization Exception",
    [0x15] = "Reserved",
    [0x16] = "Reserved",
    [0x17] = "Reserved",
    [0x18] = "Reserved",
    [0x19] = "Reserved",
    [0x1A] = "Reserved",
    [0x1B] = "Reserved",
    [0x1C] = "Reserved",
    [0x1D] = "Reserved",
    [0x1E] = "Security Exception",
    [0x1F] = "Reserved"
};

/*
 * Assembly code for interrupt stub entry points
 * This will be included directly in the object file
 */
__asm__(
    "# Macro for ISR stubs without error code\n"
    ".macro ISR_NOERR num\n"
    ".global isr_stub_\\num\n"
    ".type isr_stub_\\num, @function\n"
    "isr_stub_\\num:\n"
    "    pushq $0              # Push dummy error code\n"
    "    pushq $\\num          # Push interrupt number\n"
    "    jmp isr_common_stub   # Jump to common handler\n"
    ".endm\n"
    "\n"
    "# Macro for ISR stubs with error code\n"
    ".macro ISR_ERR num\n"
    ".global isr_stub_\\num\n"
    ".type isr_stub_\\num, @function\n"
    "isr_stub_\\num:\n"
    "    # Error code is already pushed by CPU\n"
    "    pushq $\\num          # Push interrupt number\n"
    "    jmp isr_common_stub   # Jump to common handler\n"
    ".endm\n"
    "\n"
    "# Macro for IRQ stubs\n"
    ".macro IRQ_STUB num irq\n"
    ".global irq_stub_\\num\n"
    ".type irq_stub_\\num, @function\n"
    "irq_stub_\\num:\n"
    "    pushq $0              # Push dummy error code\n"
    "    pushq $\\irq          # Push interrupt number\n"
    "    jmp irq_common_stub   # Jump to common handler\n"
    ".endm\n"
    "\n"
    "# Exception stubs (0-31)\n"
    "ISR_NOERR 0\n"
    "ISR_NOERR 1\n"
    "ISR_NOERR 2\n"
    "ISR_NOERR 3\n"
    "ISR_NOERR 4\n"
    "ISR_NOERR 5\n"
    "ISR_NOERR 6\n"
    "ISR_NOERR 7\n"
    "ISR_ERR   8\n"
    "ISR_NOERR 9\n"
    "ISR_ERR   10\n"
    "ISR_ERR   11\n"
    "ISR_ERR   12\n"
    "ISR_ERR   13\n"
    "ISR_ERR   14\n"
    "ISR_NOERR 15\n"
    "ISR_NOERR 16\n"
    "ISR_ERR   17\n"
    "ISR_NOERR 18\n"
    "ISR_NOERR 19\n"
    "ISR_NOERR 20\n"
    "ISR_NOERR 21\n"
    "ISR_NOERR 22\n"
    "ISR_NOERR 23\n"
    "ISR_NOERR 24\n"
    "ISR_NOERR 25\n"
    "ISR_NOERR 26\n"
    "ISR_NOERR 27\n"
    "ISR_NOERR 28\n"
    "ISR_NOERR 29\n"
    "ISR_ERR   30\n"
    "ISR_NOERR 31\n"
    "\n"
    "# IRQ stubs (32-47)\n"
    "IRQ_STUB 0  32\n"
    "IRQ_STUB 1  33\n"
    "IRQ_STUB 2  34\n"
    "IRQ_STUB 3  35\n"
    "IRQ_STUB 4  36\n"
    "IRQ_STUB 5  37\n"
    "IRQ_STUB 6  38\n"
    "IRQ_STUB 7  39\n"
    "IRQ_STUB 8  40\n"
    "IRQ_STUB 9  41\n"
    "IRQ_STUB 10 42\n"
    "IRQ_STUB 11 43\n"
    "IRQ_STUB 12 44\n"
    "IRQ_STUB 13 45\n"
    "IRQ_STUB 14 46\n"
    "IRQ_STUB 15 47\n"
    "\n"
    "# Syscall stub\n"
    ".global isr_stub_syscall\n"
    ".type isr_stub_syscall, @function\n"
    "isr_stub_syscall:\n"
    "    pushq $0              # Push dummy error code\n"
    "    pushq $0x80           # Push interrupt number\n"
    "    jmp isr_common_stub   # Jump to common handler\n"
    "\n"
    "# Yield stub\n"
    ".global isr_stub_yield\n"
    ".type isr_stub_yield, @function\n"
    "isr_stub_yield:\n"
    "    pushq $0              # Push dummy error code\n"
    "    pushq $0x81           # Push interrupt number\n"
    "    jmp isr_common_stub   # Jump to common handler\n"
    "\n"
    "# Common ISR stub (for CPU exceptions)\n"
    ".type isr_common_stub, @function\n"
    "isr_common_stub:\n"
    "    # Save all general purpose registers\n"
    "    pushq %rax\n"
    "    pushq %rbx\n"
    "    pushq %rcx\n"
    "    pushq %rdx\n"
    "    pushq %rsi\n"
    "    pushq %rdi\n"
    "    pushq %rbp\n"
    "    pushq %r8\n"
    "    pushq %r9\n"
    "    pushq %r10\n"
    "    pushq %r11\n"
    "    pushq %r12\n"
    "    pushq %r13\n"
    "    pushq %r14\n"
    "    pushq %r15\n"
    "\n"
    "    # Call the C exception handler with the stack pointer as argument\n"
    "    movq %rsp, %rdi\n"
    "    call handle_exception\n"
    "\n"
    "    # Restore all registers\n"
    "    popq %r15\n"
    "    popq %r14\n"
    "    popq %r13\n"
    "    popq %r12\n"
    "    popq %r11\n"
    "    popq %r10\n"
    "    popq %r9\n"
    "    popq %r8\n"
    "    popq %rbp\n"
    "    popq %rdi\n"
    "    popq %rsi\n"
    "    popq %rdx\n"
    "    popq %rcx\n"
    "    popq %rbx\n"
    "    popq %rax\n"
    "\n"
    "    # Remove error code and interrupt number\n"
    "    addq $16, %rsp\n"
    "\n"
    "    # Return from interrupt\n"
    "    iretq\n"
    "\n"
    "# Common IRQ stub (for hardware interrupts)\n"
    ".type irq_common_stub, @function\n"
    "irq_common_stub:\n"
    "    # Save all general purpose registers\n"
    "    pushq %rax\n"
    "    pushq %rbx\n"
    "    pushq %rcx\n"
    "    pushq %rdx\n"
    "    pushq %rsi\n"
    "    pushq %rdi\n"
    "    pushq %rbp\n"
    "    pushq %r8\n"
    "    pushq %r9\n"
    "    pushq %r10\n"
    "    pushq %r11\n"
    "    pushq %r12\n"
    "    pushq %r13\n"
    "    pushq %r14\n"
    "    pushq %r15\n"
    "\n"
    "    # Call the C IRQ handler with the stack pointer as argument\n"
    "    movq %rsp, %rdi\n"
    "    call handle_irq\n"
    "\n"
    "    # Restore all registers\n"
    "    popq %r15\n"
    "    popq %r14\n"
    "    popq %r13\n"
    "    popq %r12\n"
    "    popq %r11\n"
    "    popq %r10\n"
    "    popq %r9\n"
    "    popq %r8\n"
    "    popq %rbp\n"
    "    popq %rdi\n"
    "    popq %rsi\n"
    "    popq %rdx\n"
    "    popq %rcx\n"
    "    popq %rbx\n"
    "    popq %rax\n"
    "\n"
    "    # Remove error code and interrupt number\n"
    "    addq $16, %rsp\n"
    "\n"
    "    # Return from interrupt\n"
    "    iretq\n"
);

/*
 * Create an IDT entry
 */
static void set_idt_entry(uint8_t vector, void* handler, uint8_t attr) {
    uint64_t handler_addr = (uint64_t)handler;
    
    idt[vector].offset_low = handler_addr & 0xFFFF;
    idt[vector].selector = KERNEL_CODE_SEGMENT;
    idt[vector].ist = 0;
    idt[vector].attr = attr;
    idt[vector].offset_mid = (handler_addr >> 16) & 0xFFFF;
    idt[vector].offset_high = (handler_addr >> 32) & 0xFFFFFFFF;
    idt[vector].reserved = 0;
}

/*
 * Initialize the Interrupt Descriptor Table
 * Sets up entries for CPU exceptions, IRQs, and software interrupts
 */
static void init_idt(void) {
    // Initialize the IDT register
    idtr.limit = sizeof(idt) - 1;
    idtr.base = (uint64_t)&idt;

    // Set up exception handlers (0-31)
    for (int i = 0; i < 32; i++) {
        set_idt_entry(i, isr_stubs[i], IDT_ATTR_INTERRUPT_KERNEL);
    }

    // Set up IRQ handlers (32-47)
    for (int i = 0; i < 16; i++) {
        set_idt_entry(INT_VECTOR_IRQ(i), isr_stubs[INT_VECTOR_IRQ(i)], IDT_ATTR_INTERRUPT_KERNEL);
    }

    // Set up syscall handler
    set_idt_entry(INT_VECTOR_SYSCALL, isr_stubs[INT_VECTOR_SYSCALL], IDT_ATTR_TRAP_USER);

    // Set up yield handler
    set_idt_entry(INT_VECTOR_YIELD, isr_stubs[INT_VECTOR_YIELD], IDT_ATTR_TRAP_KERNEL);

    // Load the IDT
    __asm__ volatile("lidt %0" : : "m"(idtr));
}

/*
 * I/O port functions
 */
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

/*
 * Reprogram the 8259 PICs with specified base vectors
 */
void reprogram_pic(uint8_t pic1_base, uint8_t pic2_base) {
    // Save masks
    uint8_t pic1_mask = inb(PIC1_DATA);
    uint8_t pic2_mask = inb(PIC2_DATA);

    // Initialize PICs with ICW1
    outb(PIC1_COMMAND, PIC_INIT);
    outb(PIC2_COMMAND, PIC_INIT);

    // ICW2: Set base vectors
    outb(PIC1_DATA, pic1_base);
    outb(PIC2_DATA, pic2_base);

    // ICW3: Tell master PIC that slave is at IRQ2
    outb(PIC1_DATA, 1 << 2);
    // ICW3: Tell slave PIC its cascade identity
    outb(PIC2_DATA, 2);

    // ICW4: Set 8086 mode
    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);

    // Restore masks
    outb(PIC1_DATA, pic1_mask);
    outb(PIC2_DATA, pic2_mask);
}

/*
 * Initialize the 8259 PIC
 */
static void init_pic(void) {
    // Remap the PICs so that IRQs start at INT_VECTOR_IRQ_BASE (0x20)
    reprogram_pic(INT_VECTOR_IRQ_BASE, INT_VECTOR_IRQ_BASE + 8);
    
    // Mask all IRQs initially
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

/*
 * Enable a specific IRQ
 */
void enable_irq(uint8_t irq) {
    uint16_t port;
    uint8_t value;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }

    value = inb(port) & ~(1 << irq);
    outb(port, value);

    // Update our tracking mask
    if (irq < 8) {
        irq_mask &= ~(1 << irq);
    } else {
        irq_mask &= ~(1 << (irq + 8));
        
        // Make sure IRQ2 (cascade) is also enabled on master PIC
        if (irq_mask & 0xFF00) {
            outb(PIC1_DATA, inb(PIC1_DATA) & ~(1 << 2));
        }
    }
}

/*
 * Disable a specific IRQ
 */
void disable_irq(uint8_t irq) {
    uint16_t port;
    uint8_t value;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }

    value = inb(port) | (1 << irq);
    outb(port, value);

    // Update our tracking mask
    if (irq < 8) {
        irq_mask |= (1 << irq);
    } else {
        irq_mask |= (1 << (irq + 8));
    }
}

/*
 * Mask all IRQs
 */
void mask_all_irqs(void) {
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
    irq_mask = 0xFFFF;
}

/*
 * Unmask all IRQs
 */
void unmask_all_irqs(void) {
    outb(PIC1_DATA, 0x00);
    outb(PIC2_DATA, 0x00);
    irq_mask = 0x0000;
}

/*
 * Send End-Of-Interrupt signal to the PIC
 */
void send_eoi(uint8_t irq) {
    if (irq >= 8) {
        // Send EOI to slave PIC
        outb(PIC2_COMMAND, PIC_EOI);
    }
    // Send EOI to master PIC
    outb(PIC1_COMMAND, PIC_EOI);
}

/*
 * Enable interrupts (clear interrupt flag)
 */
static inline void enable_interrupts(void) {
    __asm__ volatile("sti");
}

/*
 * Disable interrupts (set interrupt flag)
 */
static inline void disable_interrupts(void) {
    __asm__ volatile("cli");
}

/*
 * Default exception handler
 */
static void default_exception_handler(cpu_context_t* context) {
    kernel_panic("Unhandled CPU exception %u (%s) at RIP=%p, error code=%lx\n",
                 context->int_num,
                 context->int_num < 32 ? exception_names[context->int_num] : "Unknown",
                 (void*)context->rip,
                 context->error_code);
}

/*
 * Special handler for page fault exception
 */
static void page_fault_handler(cpu_context_t* context) {
    // Get the faulting address from CR2 register
    uint64_t fault_addr;
    __asm__ volatile("mov %%cr2, %0" : "=r"(fault_addr));

    // Page fault error code:
    // Bit 0: Present - 0 = non-present page, 1 = protection violation
    // Bit 1: Write - 0 = read, 1 = write
    // Bit 2: User - 0 = supervisor, 1 = user
    // Bit 3: Reserved write - 1 = caused by reserved bits set to 1 in a page directory
    // Bit 4: Instruction fetch - 1 = instruction fetch

    // Determine the type of page fault
    const char* type_str;
    if (!(context->error_code & 1)) {
        type_str = "non-present page";
    } else {
        type_str = "page protection violation";
    }

    // Determine access type
    const char* access_str;
    if (context->error_code & 2) {
        access_str = "write";
    } else if (context->error_code & 16) {
        access_str = "instruction fetch";
    } else {
        access_str = "read";
    }

    // Determine privilege level
    const char* mode_str = (context->error_code & 4) ? "user" : "supervisor";

    kernel_panic("Page fault: %s during %s in %s mode at address %p\n"
                 "Instruction pointer: %p, Error code: 0x%lx\n",
                 type_str, access_str, mode_str, (void*)fault_addr,
                 (void*)context->rip, context->error_code);
}

/*
 * C handler for CPU exceptions
 * Called from assembly with the CPU context on the stack
 */
void handle_exception(cpu_context_t* context) {
    uint8_t exception_num = context->int_num;
    
    if (exception_num < 32) {
        if (exception_handlers[exception_num] != NULL) {
            // Call registered handler for this exception
            exception_handlers[exception_num](context);
        } else {
            // No handler registered, use default
            default_exception_handler(context);
        }
    } else {
        // This should never happen - it means the wrong handler was called
        kernel_panic("Invalid exception number: %u in handle_exception\n", exception_num);
    }
}

/*
 * C handler for hardware interrupts (IRQs)
 * Called from assembly with the CPU context on the stack
 */
void handle_irq(cpu_context_t* context) {
    uint8_t interrupt_num = context->int_num;
    
    // Calculate the IRQ number (IRQs start at INT_VECTOR_IRQ_BASE)
    uint8_t irq = interrupt_num - INT_VECTOR_IRQ_BASE;
    
    // Validate IRQ number
    if (irq >= 16) {
        kernel_panic("Invalid IRQ number: %u in handle_irq\n", irq);
        return;
    }
    
    // Call the registered handler if exists
    if (irq_handlers[irq] != NULL) {
        irq_handlers[irq](context);
    } else {
        kernel_printf("Warning: Unhandled IRQ %u\n", irq);
    }
    
    // Send end-of-interrupt signal to the PIC
    send_eoi(irq);
}

/*
 * C handler for software interrupts
 * Called from assembly with the CPU context on the stack
 */
void handle_isr(cpu_context_t* context) {
    uint8_t vector = context->int_num;
    
    // Call the registered handler if exists
    if (isr_handlers[vector] != NULL) {
        isr_handlers[vector](context);
    } else {
        kernel_printf("Warning: Unhandled ISR %u\n", vector);
    }
}

/*
 * Register an exception handler for a specific CPU exception
 */
void register_exception_handler(uint8_t vector, isr_handler_t handler) {
    if (vector < 32) {
        exception_handlers[vector] = handler;
    } else {
        kernel_printf("Error: Invalid exception vector: %u\n", vector);
    }
}

/*
 * Register an IRQ handler for a specific hardware interrupt
 */
void register_irq_handler(uint8_t irq, irq_handler_t handler) {
    if (irq < 16) {
        irq_handlers[irq] = handler;
    } else {
        kernel_printf("Error: Invalid IRQ: %u\n", irq);
    }
}

/*
 * Register a general ISR handler for a specific interrupt vector
 */
void register_isr_handler(uint8_t vector, isr_handler_t handler) {
    if (vector < IDT_ENTRIES) {
        isr_handlers[vector] = handler;
    } else {
        kernel_printf("Error: Invalid ISR vector: %u\n", vector);
    }
}

/*
 * Setup special exception handlers
 * Currently only configures a special page fault handler
 */
static void setup_special_handlers(void) {
    // Register page fault handler
    register_exception_handler(INT_VECTOR_PAGE_FAULT, page_fault_handler);
}

/*
 * Initialize the entire interrupt subsystem
 * This function should be called early in the boot process
 */
void init_interrupts(void) {
    // Disable interrupts during initialization
    disable_interrupts();
    
    // Zero out all handler tables
    for (int i = 0; i < 32; i++) {
        exception_handlers[i] = NULL;
    }
    
    for (int i = 0; i < 16; i++) {
        irq_handlers[i] = NULL;
    }
    
    for (int i = 0; i < IDT_ENTRIES; i++) {
        isr_handlers[i] = NULL;
    }
    
    // Initialize IDT with interrupt stubs
    init_idt();
    
    // Initialize PIC
    init_pic();
    
    // Setup special exception handlers (like page fault handler)
    setup_special_handlers();
    
    // Keep all IRQs masked initially
    mask_all_irqs();
    
    kernel_printf("Interrupt subsystem initialized successfully\n");
    
    // Now it's safe to enable interrupts
    enable_interrupts();
}
