/* EdgeX OS - x86_64 Boot Code */

.set MULTIBOOT2_MAGIC, 0xE85250D6        /* Multiboot2 Magic Number */
.set MULTIBOOT_ARCHITECTURE, 0           /* Protected Mode i386 */
.set HEADER_LENGTH, multiboot_header_end - multiboot_header
.set CHECKSUM, -(MULTIBOOT2_MAGIC + MULTIBOOT_ARCHITECTURE + HEADER_LENGTH)

/* Page size and constants */
.set PAGE_SIZE, 0x1000
.set PAGE_PRESENT, 0x1
.set PAGE_WRITE, 0x2
.set PAGE_HUGE, 0x80
.set KERNEL_PAGE_FLAGS, (PAGE_PRESENT | PAGE_WRITE)
.set KERNEL_HUGE_PAGE_FLAGS, (PAGE_PRESENT | PAGE_WRITE | PAGE_HUGE)

/* CR0 Flags */
.set CR0_PE, 0x1                   /* Protected Mode Enable */
.set CR0_PG, 0x80000000            /* Paging Enable */

/* CR4 Flags */
.set CR4_PAE, 0x20                 /* Physical Address Extension */
.set CR4_PGE, 0x80                 /* Page Global Enable */

/* EFER MSR */
.set MSR_EFER, 0xC0000080
.set EFER_LME, 0x100               /* Long Mode Enable */

/* Code and data segments */
.set GDT_CODE_SEG, 0x08
.set GDT_DATA_SEG, 0x10

/* Multiboot2 header - must be 8-byte aligned */
.section .multiboot
.align 8
multiboot_header:
    .long MULTIBOOT2_MAGIC
    .long MULTIBOOT_ARCHITECTURE
    .long HEADER_LENGTH
    .long CHECKSUM

    /* End tag */
    .short 0    /* type */
    .short 0    /* flags */
    .long 8     /* size */
multiboot_header_end:

/* Boot entry point */
.section .boot, "ax"
.global _start
.code32
_start:
    /* Disable interrupts */
    cli

    /* Save multiboot info pointer (in ebx) */
    movl %ebx, multiboot_info

    /* Check for multiboot2 magic number in eax */
    cmpl $0x36D76289, %eax
    jne boot_error

    /* Set up initial stack */
    movl $boot_stack_top, %esp
    movl $boot_stack_top, %ebp

    /* Load GDT for Long Mode */
    lgdt (gdtr)

    /* Enable PAE */
    movl %cr4, %eax
    orl $CR4_PAE, %eax
    movl %eax, %cr4

    /* Set up page tables */
    call setup_paging

    /* Enable Long Mode */
    movl $MSR_EFER, %ecx
    rdmsr
    orl $EFER_LME, %eax
    wrmsr

    /* Enable paging and protected mode */
    movl %cr0, %eax
    orl $(CR0_PE | CR0_PG), %eax
    movl %eax, %cr0

    /* Jump to 64-bit code */
    ljmp $GDT_CODE_SEG, $long_mode_start

boot_error:
    /* Something went wrong, halt the CPU */
    hlt
    jmp boot_error

/* Setup paging for higher-half kernel */
setup_paging:
    /* PML4 - Zero it out first */
    mov $pml4, %edi
    xor %eax, %eax
    movl $1024, %ecx
    rep stosl

    /* Set up recursive mapping in PML4 */
    movl $pml4, %eax
    orl $KERNEL_PAGE_FLAGS, %eax
    movl %eax, pml4 + 511 * 8

    /* Point first PML4 entry to PDPT1 (identity mapping) */
    movl $pdpt1, %eax
    orl $KERNEL_PAGE_FLAGS, %eax
    movl %eax, pml4

    /* Point 511th PML4 entry to PDPT2 (-2GB mapping) */
    movl $pdpt2, %eax
    orl $KERNEL_PAGE_FLAGS, %eax
    movl %eax, pml4 + 510 * 8

    /* Set up PDPT1 (identity mapping) - Only map first 1GB */
    movl $pd, %eax
    orl $KERNEL_PAGE_FLAGS, %eax
    movl %eax, pdpt1

    /* Set up PDPT2 (kernel mapping) - 1GB using 2MB pages */
    movl $pd, %eax
    orl $KERNEL_PAGE_FLAGS, %eax
    movl %eax, pdpt2 + 510 * 8

    /* Map first 1GB of memory using 2MB pages */
    movl $pd, %edi
    movl $0, %esi            /* Physical address */
    movl $512, %ecx          /* 512 entries (1GB) */

setup_pd_entry:
    movl %esi, %eax
    orl $KERNEL_HUGE_PAGE_FLAGS, %eax
    movl %eax, (%edi)
    movl $0, 4(%edi)         /* Upper 32 bits are 0 */
    
    /* Next entry */
    addl $8, %edi
    addl $0x200000, %esi     /* 2MB per page */
    decl %ecx
    jnz setup_pd_entry

    /* Load PML4 address into CR3 */
    movl $pml4, %eax
    movl %eax, %cr3
    ret

/* 64-bit entry point */
.code64
long_mode_start:
    /* Reload segments */
    mov $GDT_DATA_SEG, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    /* Set up the real stack in the BSS section */
    movabs $_stack_top, %rsp
    movabs $_stack_top, %rbp

    /* Clear direction flag */
    cld

    /* Jump to the C kernel entry point */
    movabs $kernel_main, %rax
    call *%rax

    /* Should not return, but if it does, halt */
halt:
    cli
    hlt
    jmp halt

/* Global Descriptor Table */
.align 16
gdt:
    .quad 0x0000000000000000    /* Null Descriptor */
    .quad 0x00AF9A000000FFFF    /* Code Segment (64-bit) */
    .quad 0x00AF92000000FFFF    /* Data Segment */
gdt_end:

.align 16
gdtr:
    .word gdt_end - gdt - 1     /* GDT size - 1 */
    .quad gdt                   /* GDT address */

/* Temporary boot stack - 16KB */
.align 16
boot_stack_bottom:
    .skip 16384
boot_stack_top:

/* Multiboot info pointer saved from boot */
.align 8
multiboot_info:
    .quad 0

/* Page Tables */
.align 4096
pml4:
    .skip 4096
pdpt1:
    .skip 4096
pdpt2:
    .skip 4096
pd:
    .skip 4096

/* Export symbols for use in C code */
.global multiboot_info
