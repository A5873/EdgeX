/*
 * EdgeX OS - Interrupt Handling Subsystem
 * 
 * This file contains the declarations for the interrupt handling
 * subsystem, including IDT setup, ISR registration, and exception handling.
 */

#ifndef EDGEX_INTERRUPT_H
#define EDGEX_INTERRUPT_H

#include <edgex/kernel.h>

/* Interrupt vector numbers */
#define INT_VECTOR_DIVIDE_ERROR        0x00
#define INT_VECTOR_DEBUG               0x01
#define INT_VECTOR_NMI                 0x02
#define INT_VECTOR_BREAKPOINT          0x03
#define INT_VECTOR_OVERFLOW            0x04
#define INT_VECTOR_BOUND_RANGE         0x05
#define INT_VECTOR_INVALID_OPCODE      0x06
#define INT_VECTOR_DEVICE_NOT_AVAIL    0x07
#define INT_VECTOR_DOUBLE_FAULT        0x08
#define INT_VECTOR_COPROCESSOR_SEG     0x09
#define INT_VECTOR_INVALID_TSS         0x0A
#define INT_VECTOR_SEGMENT_NOT_PRESENT 0x0B
#define INT_VECTOR_STACK_FAULT         0x0C
#define INT_VECTOR_GENERAL_PROTECTION  0x0D
#define INT_VECTOR_PAGE_FAULT          0x0E
#define INT_VECTOR_X87_FPU_ERROR       0x10
#define INT_VECTOR_ALIGNMENT_CHECK     0x11
#define INT_VECTOR_MACHINE_CHECK       0x12
#define INT_VECTOR_SIMD_FP_EXCEPTION   0x13
#define INT_VECTOR_VIRTUALIZATION      0x14
#define INT_VECTOR_SECURITY_EXCEPTION  0x1E

/* IRQ vector offsets - IRQs start at 0x20 */
#define INT_VECTOR_IRQ_BASE            0x20
#define INT_VECTOR_IRQ(n)              (INT_VECTOR_IRQ_BASE + (n))

/* PIC IRQ numbers */
#define IRQ_TIMER                      0
#define IRQ_KEYBOARD                   1
#define IRQ_CASCADE                    2
#define IRQ_COM2                       3
#define IRQ_COM1                       4
#define IRQ_LPT2                       5
#define IRQ_FLOPPY                     6
#define IRQ_LPT1                       7
#define IRQ_RTC                        8
#define IRQ_ACPI                       9
#define IRQ_AVAILABLE1                 10
#define IRQ_AVAILABLE2                 11
#define IRQ_PS2_MOUSE                  12
#define IRQ_FPU                        13
#define IRQ_PRIMARY_ATA                14
#define IRQ_SECONDARY_ATA              15

/* Software interrupts - starts at 0x80 */
#define INT_VECTOR_SYSCALL             0x80
#define INT_VECTOR_YIELD               0x81

/* Maximum number of interrupt vectors */
#define IDT_ENTRIES                    256

/* Interrupt gate types */
#define IDT_GATE_TYPE_TASK             0x5
#define IDT_GATE_TYPE_INTERRUPT        0xE
#define IDT_GATE_TYPE_TRAP             0xF

/* Descriptor privilege levels */
#define DPL_KERNEL                     0
#define DPL_USER                       3

/* IDT gate flags */
#define IDT_FLAG_PRESENT               (1 << 7)
#define IDT_FLAG_DPL(dpl)              ((dpl) << 5)
#define IDT_FLAG_TYPE(type)            (type)

/* Combined gate attributes */
#define IDT_ATTR_INTERRUPT_KERNEL      (IDT_FLAG_PRESENT | IDT_FLAG_DPL(DPL_KERNEL) | IDT_FLAG_TYPE(IDT_GATE_TYPE_INTERRUPT))
#define IDT_ATTR_INTERRUPT_USER        (IDT_FLAG_PRESENT | IDT_FLAG_DPL(DPL_USER) | IDT_FLAG_TYPE(IDT_GATE_TYPE_INTERRUPT))
#define IDT_ATTR_TRAP_KERNEL           (IDT_FLAG_PRESENT | IDT_FLAG_DPL(DPL_KERNEL) | IDT_FLAG_TYPE(IDT_GATE_TYPE_TRAP))
#define IDT_ATTR_TRAP_USER             (IDT_FLAG_PRESENT | IDT_FLAG_DPL(DPL_USER) | IDT_FLAG_TYPE(IDT_GATE_TYPE_TRAP))

/* 8259 PIC ports */
#define PIC1_COMMAND                   0x20
#define PIC1_DATA                      0x21
#define PIC2_COMMAND                   0xA0
#define PIC2_DATA                      0xA1

/* PIC commands */
#define PIC_EOI                        0x20
#define PIC_INIT                       0x11

/* IDT descriptor structure */
typedef struct __attribute__((packed)) {
    uint16_t offset_low;     /* Offset bits 0-15 */
    uint16_t selector;       /* Code segment selector */
    uint8_t  ist;            /* Interrupt stack table offset (bits 0-2) */
    uint8_t  attr;           /* Type and attributes */
    uint16_t offset_mid;     /* Offset bits 16-31 */
    uint32_t offset_high;    /* Offset bits 32-63 */
    uint32_t reserved;       /* Reserved, must be 0 */
} idt_entry_t;

/* IDTR structure for loading the IDT */
typedef struct __attribute__((packed)) {
    uint16_t limit;          /* Size of IDT - 1 */
    uint64_t base;           /* Base address of IDT */
} idtr_t;

/* CPU context saved by interrupt handlers */
typedef struct {
    /* Manually saved registers */
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
    
    /* Automatically saved by CPU */
    uint64_t int_num;        /* Interrupt vector number */
    uint64_t error_code;     /* Error code (or 0) */
    uint64_t rip;            /* Instruction pointer */
    uint64_t cs;             /* Code segment */
    uint64_t rflags;         /* CPU flags */
    uint64_t rsp;            /* Stack pointer */
    uint64_t ss;             /* Stack segment */
} cpu_context_t;

/* IRQ handler function type */
typedef void (*irq_handler_t)(cpu_context_t* context);

/* General ISR handler function type */
typedef void (*isr_handler_t)(cpu_context_t* context);

/* Initialize the interrupt subsystem */
void init_interrupts(void);

/* ISR handler registration */
void register_exception_handler(uint8_t vector, isr_handler_t handler);
void register_irq_handler(uint8_t irq, irq_handler_t handler);
void register_isr_handler(uint8_t vector, isr_handler_t handler);

/* Enable or disable IRQs */
void enable_irq(uint8_t irq);
void disable_irq(uint8_t irq);
void mask_all_irqs(void);
void unmask_all_irqs(void);

/* Send EOI to PIC */
void send_eoi(uint8_t irq);

/* Reprogram the PIC with specified IRQ base */
void reprogram_pic(uint8_t pic1_base, uint8_t pic2_base);

#endif /* EDGEX_INTERRUPT_H */

