/*
 * EdgeX OS - Memory Management Subsystem
 * 
 * This file implements the physical memory manager, page frame
 * allocation, and memory zone management for EdgeX OS.
 */

#include <edgex/kernel.h>

/* Physical memory management */
#define PAGE_FLAG_FREE     0x0000
#define PAGE_FLAG_USED     0x0001
#define PAGE_FLAG_RESERVED 0x0002
#define PAGE_FLAG_KERNEL   0x0004
#define PAGE_FLAG_DMA      0x0008
#define PAGE_FLAG_MMIO     0x0010

/* Maximum number of physical pages we can track */
#define MAX_PHYSICAL_PAGES 1048576 /* 4GB with 4KB pages */

/* Page frame database */
static page_frame_t* page_frames = NULL;
static uint64_t total_pages = 0;
static uint64_t free_pages = 0;

/* Memory zones, defined in main.c */
extern memory_zone_t memory_zones[ZONE_TYPES_COUNT];

/* Simple physical memory allocator */
static void* early_alloc(size_t size) {
    /* This is a very simple bump allocator used during early boot */
    /* We'll use memory starting at 1MB physical and just move upward */
    static uint64_t next_address = 0x100000; /* Start at 1MB physical */
    
    /* Align to 16 bytes */
    size = (size + 15) & ~15;
    
    /* Allocate memory */
    void* result = (void*)next_address;
    next_address += size;
    
    /* Zero the memory */
    for (size_t i = 0; i < size; i++) {
        ((uint8_t*)result)[i] = 0;
    }
    
    return result;
}

/* Initialize physical memory manager */
static void init_physical_memory(void) {
    LOG_INFO("Initializing physical memory manager...");
    
    /* Calculate the number of pages we need to track */
    uint64_t highest_address = 0;
    
    for (int i = 0; i < ZONE_TYPES_COUNT; i++) {
        memory_zone_t* zone = &memory_zones[i];
        if (zone->end_address > highest_address) {
            highest_address = zone->end_address;
        }
    }
    
    total_pages = (highest_address + PAGE_SIZE - 1) / PAGE_SIZE;
    if (total_pages > MAX_PHYSICAL_PAGES) {
        total_pages = MAX_PHYSICAL_PAGES;
        LOG_WARNING("Limiting physical memory tracking to %llu pages (%llu MB)",
                  total_pages, (total_pages * PAGE_SIZE) / (1024 * 1024));
    }
    
    /* Allocate page frame database */
    size_t db_size = total_pages * sizeof(page_frame_t);
    page_frames = early_alloc(db_size);
    
    LOG_DEBUG("Page frame database at 0x%p, size: %llu bytes", 
              page_frames, db_size);
    
    /* Mark all pages as reserved initially */
    for (uint64_t i = 0; i < total_pages; i++) {
        page_frames[i].flags = PAGE_FLAG_RESERVED;
        page_frames[i].ref_count = 0;
        page_frames[i].order = 0;
    }
    
    /* Mark available pages as free */
    memory_zone_t* normal_zone = &memory_zones[ZONE_NORMAL];
    memory_zone_t* dma_zone = &memory_zones[ZONE_DMA];
    
    /* Process DMA zone */
    if (dma_zone->size > 0) {
        uint64_t start_page = dma_zone->start_address / PAGE_SIZE;
        uint64_t end_page = (dma_zone->end_address + PAGE_SIZE - 1) / PAGE_SIZE;
        
        for (uint64_t i = start_page; i < end_page && i < total_pages; i++) {
            page_frames[i].flags = PAGE_FLAG_FREE | PAGE_FLAG_DMA;
            free_pages++;
        }
    }
    
    /* Process normal zone */
    if (normal_zone->size > 0) {
        uint64_t start_page = normal_zone->start_address / PAGE_SIZE;
        uint64_t end_page = (normal_zone->end_address + PAGE_SIZE - 1) / PAGE_SIZE;
        
        for (uint64_t i = start_page; i < end_page && i < total_pages; i++) {
            /* Check if already marked as DMA (overlapping zones) */
            if ((page_frames[i].flags & PAGE_FLAG_FREE) == 0) {
                page_frames[i].flags = PAGE_FLAG_FREE;
                free_pages++;
            }
        }
    }
    
    /* Reserve kernel pages */
    extern uint64_t _kernel_physical_start;
    extern uint64_t _kernel_physical_end;
    
    uint64_t kernel_start_page = (uint64_t)&_kernel_physical_start / PAGE_SIZE;
    uint64_t kernel_end_page = ((uint64_t)&_kernel_physical_end + PAGE_SIZE - 1) / PAGE_SIZE;
    
    LOG_DEBUG("Kernel physical: 0x%llx - 0x%llx (%llu pages)",
             (uint64_t)&_kernel_physical_start, (uint64_t)&_kernel_physical_end,
             kernel_end_page - kernel_start_page);
    
    for (uint64_t i = kernel_start_page; i < kernel_end_page && i < total_pages; i++) {
        if (page_frames[i].flags & PAGE_FLAG_FREE) {
            page_frames[i].flags = PAGE_FLAG_USED | PAGE_FLAG_KERNEL;
            page_frames[i].ref_count = 1;
            free_pages--;
        }
    }
    
    /* Reserve first 1MB for BIOS and early boot structures */
    for (uint64_t i = 0; i < 256 && i < total_pages; i++) {
        if (page_frames[i].flags & PAGE_FLAG_FREE) {
            page_frames[i].flags = PAGE_FLAG_RESERVED;
            free_pages--;
        }
    }
    
    LOG_INFO("Physical memory initialized: %llu pages total, %llu pages free",
           total_pages, free_pages);
}

/* Allocate a single physical page */
void* alloc_page(void) {
    /* Very simple first-fit allocator for now */
    for (uint64_t i = 0; i < total_pages; i++) {
        if (page_frames[i].flags == PAGE_FLAG_FREE) {
            page_frames[i].flags = PAGE_FLAG_USED;
            page_frames[i].ref_count = 1;
            free_pages--;
            return (void*)(i * PAGE_SIZE);
        }
    }
    
    /* No free pages */
    LOG_ERROR("Out of memory: no free pages available!");
    return NULL;
}

/* Allocate a DMA-capable page (below 16MB) */
void* alloc_dma_page(void) {
    /* Find a free page in the DMA zone */
    for (uint64_t i = 0; i < total_pages; i++) {
        if ((page_frames[i].flags & (PAGE_FLAG_FREE | PAGE_FLAG_DMA)) == 
            (PAGE_FLAG_FREE | PAGE_FLAG_DMA)) {
            page_frames[i].flags = (page_frames[i].flags & ~PAGE_FLAG_FREE) | PAGE_FLAG_USED;
            page_frames[i].ref_count = 1;
            free_pages--;
            return (void*)(i * PAGE_SIZE);
        }
    }
    
    /* No free DMA pages */
    LOG_ERROR("Out of memory: no free DMA pages available!");
    return NULL;
}

/* Free a previously allocated page */
void free_page(void* page) {
    if (!page) {
        return;
    }
    
    /* Get page frame index */
    uint64_t idx = (uint64_t)page / PAGE_SIZE;
    
    /* Check if index is valid */
    if (idx >= total_pages) {
        LOG_ERROR("Invalid page address: 0x%p", page);
        return;
    }
    
    /* Decrement reference count */
    if (page_frames[idx].ref_count == 0) {
        LOG_ERROR("Double free detected for page 0x%p", page);
        return;
    }
    
    page_frames[idx].ref_count--;
    
    /* Free the page when ref count reaches 0 */
    if (page_frames[idx].ref_count == 0) {
        /* Keep DMA flag if present, but set page as free */
        page_frames[idx].flags = (page_frames[idx].flags & PAGE_FLAG_DMA) | PAGE_FLAG_FREE;
        free_pages++;
    }
}

/* Increment page reference count */
void page_inc_ref(void* page) {
    if (!page) {
        return;
    }
    
    uint64_t idx = (uint64_t)page / PAGE_SIZE;
    if (idx < total_pages && (page_frames[idx].flags & PAGE_FLAG_USED)) {
        page_frames[idx].ref_count++;
    } else {
        LOG_ERROR("Attempted to reference invalid page: 0x%p", page);
    }
}

/* Get reference count for a page */
uint32_t page_get_ref_count(void* page) {
    if (!page) {
        return 0;
    }
    
    uint64_t idx = (uint64_t)page / PAGE_SIZE;
    if (idx < total_pages) {
        return page_frames[idx].ref_count;
    }
    
    return 0;
}

/* Get page flags */
uint64_t page_get_flags(void* page) {
    if (!page) {
        return 0;
    }
    
    uint64_t idx = (uint64_t)page / PAGE_SIZE;
    if (idx < total_pages) {
        return page_frames[idx].flags;
    }
    
    return 0;
}

/* Set page flags */
void page_set_flags(void* page, uint64_t flags) {
    if (!page) {
        return;
    }
    
    uint64_t idx = (uint64_t)page / PAGE_SIZE;
    if (idx < total_pages) {
        page_frames[idx].flags = flags;
    }
}

/* Reserve a specific physical page range */
void reserve_page_range(void* start, size_t size) {
    uint64_t start_page = (uint64_t)start / PAGE_SIZE;
    uint64_t end_page = ((uint64_t)start + size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    for (uint64_t i = start_page; i < end_page && i < total_pages; i++) {
        if (page_frames[i].flags & PAGE_FLAG_FREE) {
            page_frames[i].flags = PAGE_FLAG_RESERVED;
            free_pages--;
        }
    }
    
    LOG_DEBUG("Reserved physical page range: 0x%p - 0x%p", 
             start, (void*)((uint64_t)start + size));
}

/* Get memory statistics */
void get_memory_stats(uint64_t* total, uint64_t* free, uint64_t* used) {
    if (total) {
        *total = total_pages * PAGE_SIZE;
    }
    
    if (free) {
        *free = free_pages * PAGE_SIZE;
    }
    
    if (used) {
        *used = (total_pages - free_pages) * PAGE_SIZE;
    }
}

/* Basic kernel heap - very simple for now, will be replaced later */
#define HEAP_START 0xFFFFFFFF90000000
#define HEAP_SIZE  (16 * 1024 * 1024) /* 16MB initial heap */

/* Extremely simple kmalloc/kfree implementation - just for early boot */
void* kmalloc(size_t size) {
    /* Align to 16 bytes */
    size = (size + 15) & ~15;
    
    /* Very simple bump allocator */
    static void* next_addr = (void*)HEAP_START;
    static size_t remaining = HEAP_SIZE;
    
    if (size > remaining) {
        LOG_ERROR("Out of kernel heap memory!");
        return NULL;
    }
    
    void* result = next_addr;
    next_addr = (void*)((uint64_t)next_addr + size);
    remaining -= size;
    
    return result;
}

/* Allocate zeroed memory */
void* kzalloc(size_t size) {
    void* ptr = kmalloc(size);
    if (ptr) {
        /* Zero the memory */
        for (size_t i = 0; i < size; i++) {
            ((uint8_t*)ptr)[i] = 0;
        }
    }
    return ptr;
}

/* Free allocated memory - not actually implemented in this simple allocator */
void kfree(void* ptr) {
    /* Not implemented in the simple bump allocator */
    /* Will be implemented in the real kernel heap */
}

/* Basic memory operations */
void* memset(void* s, int c, size_t n) {
    uint8_t* p = (uint8_t*)s;
    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }
    return s;
}

void* memcpy(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

int memcmp(const void* s1, const void* s2, size_t n) {
    const uint8_t* p1 = (const uint8_t*)s1;
    const uint8_t* p2 = (const uint8_t*)s2;
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] - p2[i];
        }
    }
    return 0;
}

void* memmove(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    
    if (d < s) {
        /* Copy forward */
        for (size_t i = 0; i < n; i++) {
            d[i] = s[i];
        }
    } else if (d > s) {
        /* Copy backward to handle overlapping regions */
        for (size_t i = n; i > 0; i--) {
            d[i-1] = s[i-1];
        }
    }
    
    return dest;
}

/* Called from init_memory() in main.c */
void init_physical_memory_manager(void) {
    /* Initialize the physical memory manager */
    init_physical_memory();
}
