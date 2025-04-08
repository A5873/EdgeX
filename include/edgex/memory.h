/*
 * EdgeX OS - Memory Management Header
 * 
 * This file contains memory-related type definitions, structures,
 * and function prototypes for the EdgeX kernel's memory management system.
 */

#ifndef EDGEX_MEMORY_H
#define EDGEX_MEMORY_H

#include <edgex/kernel.h>

/* Memory region types */
#define MEMORY_REGION_AVAILABLE     1
#define MEMORY_REGION_RESERVED      2
#define MEMORY_REGION_ACPI          3
#define MEMORY_REGION_NVS           4
#define MEMORY_REGION_BADRAM        5

/* Physical memory management */
typedef struct {
    uint64_t start_addr;        /* Start physical address */
    uint64_t end_addr;          /* End physical address */
    uint64_t size;              /* Size in bytes */
    uint32_t type;              /* Region type */
    uint32_t attributes;        /* Additional attributes */
} physical_memory_region_t;

/* Virtual memory space descriptor */
typedef struct {
    uint64_t start_vaddr;       /* Start virtual address */
    uint64_t end_vaddr;         /* End virtual address */
    uint64_t flags;             /* VM_* flags */
    struct page* pages;         /* Associated physical pages */
} vm_area_t;

/* Memory allocation flags */
#define ALLOC_ZERO       (1 << 0)  /* Zero the allocated memory */
#define ALLOC_DMA        (1 << 1)  /* Allocate from DMA-capable zone */
#define ALLOC_KERNEL     (1 << 2)  /* Allocate for kernel use */
#define ALLOC_USER       (1 << 3)  /* Allocate for user space */
#define ALLOC_CONTIGUOUS (1 << 4)  /* Physically contiguous allocation */

/* Memory allocation functions */
void* alloc_pages(size_t count, uint32_t flags);
void free_pages(void* addr, size_t count);
int map_pages(uint64_t vaddr, uint64_t paddr, size_t size, uint64_t flags);
int unmap_pages(uint64_t vaddr, size_t size);

/* Memory information function */
void memory_init(void);
void memory_late_init(void);
void get_memory_info(uint64_t* total, uint64_t* free, uint64_t* kernel);
int find_free_pages(size_t* available_pages);

#endif /* EDGEX_MEMORY_H */

