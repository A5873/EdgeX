/* EdgeX OS - Linker Script for x86_64 */

ENTRY(_start)

/* Higher half kernel address */
KERNEL_VMA = 0xFFFFFFFF80000000;

SECTIONS
{
    /* Place the kernel at 1MB physical, but map it to higher half virtual */
    . = 1M;

    _kernel_physical_start = .;

    /* Multiboot header needs to be early in the image */
    .boot ALIGN(4K) : 
    {
        KEEP(*(.multiboot))
        *(.boot)
    }

    /* Add a mapping offset to get us to higher half */
    . += KERNEL_VMA;

    _kernel_virtual_start = .;

    /* Code section */
    .text ALIGN(4K) : AT(ADDR(.text) - KERNEL_VMA)
    {
        _text_start = .;
        *(.text)
        *(.text.*)
        _text_end = .;
    }

    /* Read-only data */
    .rodata ALIGN(4K) : AT(ADDR(.rodata) - KERNEL_VMA)
    {
        _rodata_start = .;
        *(.rodata)
        *(.rodata.*)
        _rodata_end = .;
    }

    /* Read-write data (initialized) */
    .data ALIGN(4K) : AT(ADDR(.data) - KERNEL_VMA)
    {
        _data_start = .;
        *(.data)
        *(.data.*)
        _data_end = .;
    }

    /* Read-write data (uninitialized) */
    .bss ALIGN(4K) : AT(ADDR(.bss) - KERNEL_VMA)
    {
        _bss_start = .;
        *(COMMON)
        *(.bss)
        *(.bss.*)
        
        /* Reserve 64KB for the initial stack */
        . = ALIGN(16);
        _stack_bottom = .;
        . += 64K;
        _stack_top = .;
        
        _bss_end = .;
    }

    /* Discard unnecessary sections */
    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note.*)
    }

    _kernel_virtual_end = .;
    _kernel_physical_end = . - KERNEL_VMA;
}

