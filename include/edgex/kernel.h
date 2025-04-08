/*
 * EdgeX OS - Core Kernel Header
 * 
 * This file contains essential type definitions, structures,
 * and function prototypes used throughout the kernel.
 */

#ifndef EDGEX_KERNEL_H
#define EDGEX_KERNEL_H

/* Include standard types when in unit test mode */
#ifdef UNIT_TEST
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#define UNIT_TEST_DEFINITION 1
#else
/* Basic type definitions */
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long long   int64_t;
typedef uint64_t           size_t;
typedef int64_t            ssize_t;
typedef uint8_t            bool;

/* Boolean constants */
#define true  1
#define false 0
#endif /* UNIT_TEST */

/* Ensure NULL is defined */
#ifndef NULL
#define NULL  ((void*)0)
#endif

/* Kernel version information */
#define EDGEX_VERSION_MAJOR 0
#define EDGEX_VERSION_MINOR 1
#define EDGEX_VERSION_PATCH 0
#define EDGEX_VERSION_STRING "0.1.0"
#define EDGEX_CODENAME "Quantum Edge"

/* Kernel build information */
typedef struct {
    const char* version;
    const char* codename;
    const char* build_date;
    const char* build_time;
    const char* compiler;
    const char* architecture;
} kernel_info_t;

/* Debug levels */
#define LOG_LEVEL_NONE    0
#define LOG_LEVEL_ERROR   1
#define LOG_LEVEL_WARNING 2
#define LOG_LEVEL_INFO    3
#define LOG_LEVEL_DEBUG   4
#define LOG_LEVEL_TRACE   5

/* Current log level - can be changed at runtime */
extern int kernel_log_level;

/* Debug macros */
#define LOG_ERROR(fmt, ...)   kprintf("[ERROR] " fmt "\n", ##__VA_ARGS__)
#define LOG_WARNING(fmt, ...) if (kernel_log_level >= LOG_LEVEL_WARNING) kprintf("[WARN]  " fmt "\n", ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)    if (kernel_log_level >= LOG_LEVEL_INFO)    kprintf("[INFO]  " fmt "\n", ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...)   if (kernel_log_level >= LOG_LEVEL_DEBUG)   kprintf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
#define LOG_TRACE(fmt, ...)   if (kernel_log_level >= LOG_LEVEL_TRACE)   kprintf("[TRACE] " fmt "\n", ##__VA_ARGS__)

/* Panic - halts the system with a message */
#define PANIC(fmt, ...) do { \
    kprintf("[PANIC] " fmt "\n", ##__VA_ARGS__); \
    __asm__ volatile("cli; hlt"); \
    for(;;); \
} while(0)

/* Assert - checks a condition and panics if false */
#define ASSERT(cond, fmt, ...) do { \
    if (!(cond)) { \
        PANIC("Assertion failed: " #cond "\n" fmt, ##__VA_ARGS__); \
    } \
} while(0)

/* Memory constants */
#define PAGE_SIZE 4096
#define PAGE_SHIFT 12
#define PAGE_MASK 0xFFFFFFFFFFFFF000

/* Physical memory page frame */
typedef struct {
    uint64_t flags;           /* Page flags (free, reserved, etc.) */
    uint32_t ref_count;       /* Reference count */
    uint32_t order;           /* Buddy allocator order (power of 2) */
} page_frame_t;

/* Memory zone types */
typedef enum {
    ZONE_NORMAL,              /* Normal usable memory */
    ZONE_DMA,                 /* DMA-capable memory (<16MB) */
    ZONE_RESERVED,            /* Reserved/unusable memory */
    ZONE_TYPES_COUNT
} memory_zone_type_t;

/* Memory zone information */
typedef struct {
    uint64_t start_address;   /* Start physical address */
    uint64_t end_address;     /* End physical address */
    uint64_t size;            /* Size in bytes */
    uint64_t free;            /* Free memory in bytes */
    uint64_t pages;           /* Number of pages in zone */
    uint64_t free_pages;      /* Number of free pages */
    memory_zone_type_t type;  /* Zone type */
} memory_zone_t;

/* Memory map entry */
typedef struct {
    uint64_t base_addr;       /* Base address */
    uint64_t length;          /* Length in bytes */
    uint32_t type;            /* Entry type */
    uint32_t reserved;        /* Reserved, must be 0 */
} memory_map_entry_t;

/* Multiboot2 header structure (simplified) */
typedef struct {
    uint32_t total_size;
    uint32_t reserved;
    /* Tags follow here */
} multiboot2_info_t;

/* Multiboot2 tag header */
typedef struct {
    uint32_t type;
    uint32_t size;
    /* Tag-specific data follows */
} multiboot2_tag_t;

/* Multiboot2 memory map tag */
typedef struct {
    multiboot2_tag_t tag;
    uint32_t entry_size;
    uint32_t entry_version;
    /* Entries follow */
} multiboot2_mmap_t;

/* Virtual memory mapping flags */
#define VM_READ     (1 << 0)  /* Readable */
#define VM_WRITE    (1 << 1)  /* Writable */
#define VM_EXEC     (1 << 2)  /* Executable */
#define VM_USER     (1 << 3)  /* User accessible */
#define VM_NOCACHE  (1 << 4)  /* Disable caching */
#define VM_GLOBAL   (1 << 5)  /* Global mapping */

/* Core kernel functions */
void kernel_main(void);
void early_init(void);
void init_console(void);
void init_memory(void);
void parse_multiboot_info(void);

/* Console/output functions */
void kputchar(char c);
void kputs(const char* str);
int kprintf(const char* fmt, ...);

/* Memory management functions */
void* alloc_page(void);
void* alloc_dma_page(void);
void free_page(void* page);
void page_inc_ref(void* page);
uint32_t page_get_ref_count(void* page);
uint64_t page_get_flags(void* page);
void page_set_flags(void* page, uint64_t flags);
void reserve_page_range(void* start, size_t size);
void get_memory_stats(uint64_t* total, uint64_t* free, uint64_t* used);

void* kmalloc(size_t size);
void* kzalloc(size_t size);
void kfree(void* ptr);
void* vmalloc(size_t size);
void vfree(void* ptr);

#ifndef UNIT_TEST
/* Memory manipulation functions - only when not in unit test mode */
void* memset(void* s, int c, size_t n);
void* memcpy(void* dest, const void* src, size_t n);
int memcmp(const void* s1, const void* s2, size_t n);
void* memmove(void* dest, const void* src, size_t n);
#endif /* UNIT_TEST */
/* Inline assembly wrapper */
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void io_wait(void) {
    outb(0x80, 0);
}

static inline void cli(void) {
    __asm__ volatile("cli");
}

static inline void sti(void) {
    __asm__ volatile("sti");
}

static inline void hlt(void) {
    __asm__ volatile("hlt");
}

#endif /* EDGEX_KERNEL_H */
