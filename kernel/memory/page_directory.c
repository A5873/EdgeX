/*
 * EdgeX OS - Page Directory Management
 *
 * This file implements page directory management for the EdgeX OS memory
 * management system, providing support for address space manipulation,
 * copy-on-write functionality, and memory tracking.
 */

#include <edgex/memory.h>
#include <edgex/scheduler.h>
#include <edgex/ipc/mutex.h>
#include <edgex/kernel.h>
#include <string.h>

/* Page directory constants */
#define PAGE_PRESENT        (1ULL << 0)
#define PAGE_WRITABLE       (1ULL << 1)
#define PAGE_USER           (1ULL << 2)
#define PAGE_WRITE_THROUGH  (1ULL << 3)
#define PAGE_CACHE_DISABLE  (1ULL << 4)
#define PAGE_ACCESSED       (1ULL << 5)
#define PAGE_DIRTY          (1ULL << 6)
#define PAGE_SIZE           (1ULL << 7)  /* For 2MB/1GB pages */
#define PAGE_GLOBAL         (1ULL << 8)
#define PAGE_COW            (1ULL << 9)  /* Custom: Copy-on-write */
#define PAGE_READ_ONLY      (1ULL << 10) /* Custom: Read-only */
#define PAGE_EXEC_DISABLE   (1ULL << 63) /* NX bit */

/* Page directory levels (for x86_64 4-level paging) */
#define PD_LEVEL_PML4       0
#define PD_LEVEL_PDPT       1
#define PD_LEVEL_PD         2
#define PD_LEVEL_PT         3

/* Page size constants */
#define PAGE_SIZE_4K        4096
#define PAGE_SIZE_2M        (2 * 1024 * 1024)
#define PAGE_SIZE_1G        (1 * 1024 * 1024 * 1024)

/* Page table entry (PTE) count per table */
#define PTE_COUNT_PER_TABLE 512

/* Memory access flags (correspond to x86 page fault error codes) */
#define MEM_ACCESS_READ     0x0
#define MEM_ACCESS_WRITE    0x1
#define MEM_ACCESS_EXEC     0x2
#define MEM_ACCESS_USER     0x4
#define MEM_ACCESS_RESERVED 0x8
#define MEM_ACCESS_INSTR    0x10

/* Page directory structure */
struct page_directory {
    uint64_t* pml4_table;           /* Level 0: PML4 table physical address */
    uint64_t cr3_value;             /* Value to load into CR3 register */
    mutex_t lock;                   /* Lock for this page directory */
    uint32_t ref_count;             /* Reference count */
    uint32_t owner_pid;             /* PID of the owning task */
    
    /* Stats and tracking */
    uint64_t page_fault_count;
    uint64_t cow_breaks_count;
    uint64_t total_mapped_pages;
    uint64_t total_user_pages;
    
    /* Access tracking */
    struct {
        uint64_t vaddr;             /* Virtual address of fault */
        uint64_t access_type;       /* Type of access that caused fault */
        uint64_t timestamp;         /* When the fault occurred */
        uint32_t task_id;           /* Task ID that caused the fault */
    } last_page_faults[10];         /* Circular buffer of last 10 page faults */
    uint32_t last_fault_index;      /* Index for circular buffer */
    
    /* For list management */
    struct page_directory* next;
};

/* Global state */
static mutex_t global_pd_lock;
static struct page_directory* all_page_directories = NULL;
static uint32_t total_page_directories = 0;
static bool pd_system_initialized = false;

/* Forward declarations */
static void destroy_page_directory_internal(struct page_directory* pd);
static int copy_page_tables(struct page_directory* src, struct page_directory* dest, uint64_t start_addr, uint64_t end_addr);
static int mark_pages_cow(struct page_directory* pd, uint64_t start_addr, uint64_t end_addr);
static int handle_cow_page_fault(struct page_directory* pd, uint64_t fault_addr, uint64_t error_code);
static void track_page_fault(struct page_directory* pd, uint64_t fault_addr, uint64_t error_code);
static void* get_physical_address(struct page_directory* pd, uint64_t virtual_addr);
static int set_page_flags(struct page_directory* pd, uint64_t virtual_addr, uint64_t flags, uint64_t mask);

/*
 * Initialize the page directory management system
 */
void init_page_directory_system(void) {
    if (pd_system_initialized) {
        return;
    }
    
    mutex_init(&global_pd_lock);
    all_page_directories = NULL;
    total_page_directories = 0;
    pd_system_initialized = true;
    
    LOG_INFO("Page directory management system initialized");
}

/*
 * Create a new page directory
 * 
 * owner_pid: PID of the task that will own this page directory
 * 
 * Returns: Pointer to the new page directory or NULL on error
 */
page_directory_t create_page_directory(pid_t owner_pid) {
    struct page_directory* pd;
    void* pml4_table;
    
    if (!pd_system_initialized) {
        init_page_directory_system();
    }
    
    /* Allocate memory for the page directory structure */
    pd = (struct page_directory*)kmalloc(sizeof(struct page_directory));
    if (pd == NULL) {
        LOG_ERROR("Failed to allocate memory for page directory structure");
        return NULL;
    }
    
    /* Allocate memory for the PML4 table (top level) */
    pml4_table = alloc_pages(1, ALLOC_ZERO | ALLOC_KERNEL);
    if (pml4_table == NULL) {
        LOG_ERROR("Failed to allocate memory for PML4 table");
        kfree(pd);
        return NULL;
    }
    
    /* Initialize fields */
    pd->pml4_table = (uint64_t*)pml4_table;
    pd->cr3_value = (uint64_t)pml4_table;  /* CR3 holds physical address of PML4 */
    mutex_init(&pd->lock);
    pd->ref_count = 1;
    pd->owner_pid = owner_pid;
    pd->page_fault_count = 0;
    pd->cow_breaks_count = 0;
    pd->total_mapped_pages = 0;
    pd->total_user_pages = 0;
    pd->last_fault_index = 0;
    pd->next = NULL;
    
    /* Initialize kernel mappings (identity mapping for kernel space) */
    /* This would typically map physical memory from 0 to kernel_size to high virtual addresses */
    /* For simplicity, we're not implementing the details of this here */
    
    /* Register this page directory */
    mutex_lock(&global_pd_lock);
    pd->next = all_page_directories;
    all_page_directories = pd;
    total_page_directories++;
    mutex_unlock(&global_pd_lock);
    
    LOG_INFO("Created page directory for task %d", owner_pid);
    
    return pd;
}

/*
 * Destroy a page directory
 * 
 * pd: Pointer to the page directory to destroy
 */
void destroy_page_directory(page_directory_t pd) {
    struct page_directory* directory = (struct page_directory*)pd;
    struct page_directory* curr;
    struct page_directory* prev = NULL;
    bool found = false;
    
    if (!pd_system_initialized || directory == NULL) {
        return;
    }
    
    /* Find and remove from the global list */
    mutex_lock(&global_pd_lock);
    curr = all_page_directories;
    while (curr != NULL) {
        if (curr == directory) {
            found = true;
            if (prev == NULL) {
                all_page_directories = curr->next;
            } else {
                prev->next = curr->next;
            }
            total_page_directories--;
            break;
        }
        prev = curr;
        curr = curr->next;
    }
    mutex_unlock(&global_pd_lock);
    
    if (!found) {
        LOG_ERROR("Attempted to destroy invalid page directory");
        return;
    }
    
    /* Now destroy the page directory */
    destroy_page_directory_internal(directory);
}

/*
 * Internal function to destroy a page directory
 */
static void destroy_page_directory_internal(struct page_directory* pd) {
    /* Lock the page directory */
    mutex_lock(&pd->lock);
    
    /* Decrement reference count */
    pd->ref_count--;
    
    if (pd->ref_count == 0) {
        /* Free all page tables */
        /* This would involve walking the entire page directory structure */
        /* and freeing all allocated page tables */
        /* For simplicity, we're just freeing the top-level table here */
        free_pages(pd->pml4_table, 1);
        
        LOG_INFO("Destroyed page directory for task %d", pd->owner_pid);
        
        /* Unlock before freeing */
        mutex_unlock(&pd->lock);
        
        /* Free the structure */
        kfree(pd);
        
        return;
    }
    
    /* Still referenced, just unlock */
    mutex_unlock(&pd->lock);
}

/*
 * Create a copy of a page directory (similar to fork)
 * 
 * src: Source page directory to copy from
 * dest_pid: PID that will own the new page directory
 * cow: Whether to use copy-on-write for shared pages
 * 
 * Returns: Pointer to the new page directory or NULL on error
 */
page_directory_t copy_page_directory(page_directory_t src, pid_t dest_pid, bool cow) {
    struct page_directory* source = (struct page_directory*)src;
    struct page_directory* dest;
    
    if (!pd_system_initialized || source == NULL) {
        return NULL;
    }
    
    /* Create a new empty page directory */
    dest = (struct page_directory*)create_page_directory(dest_pid);
    if (dest == NULL) {
        LOG_ERROR("Failed to create destination page directory for copy");
        return NULL;
    }
    
    /* Lock both page directories */
    mutex_lock(&source->lock);
    mutex_lock(&dest->lock);
    
    /* Copy user-space mappings from source to destination */
    /* For a real implementation, this would copy all user-space page mappings */
    /* We'll assume user space is from 0 to some value below the kernel base */
    uint64_t user_space_end = 0x00007FFFFFFFFFFF;  /* End of user space in canonical x86_64 */
    if (copy_page_tables(source, dest, 0, user_space_end) != 0) {
        LOG_ERROR("Failed to copy page tables from task %d to task %d", 
                 source->owner_pid, dest_pid);
        mutex_unlock(&dest->lock);
        mutex_unlock(&source->lock);
        destroy_page_directory(dest);
        return NULL;
    }
    
    /* If using copy-on-write, mark shared pages as COW in both page directories */
    if (cow) {
        mark_pages_cow(source, 0, user_space_end);
        mark_pages_cow(dest, 0, user_space_end);
    }
    
    /* Copy statistics (except fault-specific ones) */
    dest->total_mapped_pages = source->total_mapped_pages;
    dest->total_user_pages = source->total_user_pages;
    
    mutex_unlock(&dest->lock);
    mutex_unlock(&source->lock);
    
    LOG_INFO("Copied page directory from task %d to task %d (COW: %s)", 
             source->owner_pid, dest_pid, cow ? "enabled" : "disabled");
    
    return dest;
}

/*
 * Handle a page fault
 * 
 * pd: Page directory in which the fault occurred
 * fault_addr: Virtual address that caused the fault
 * error_code: Fault error code (processor-specific encoding of the fault type)
 * 
 * Returns: 0 if the fault was handled, negative error code otherwise
 */
int handle_page_fault(page_directory_t pd, uint64_t fault_addr, uint64_t error_code) {
    struct page_directory* directory = (struct page_directory*)pd;
    int result;
    
    if (!pd_system_initialized || directory == NULL) {
        return -EINVAL;
    }
    
    /* Lock the page directory */
    mutex_lock(&directory->lock);
    
    /* Track this fault for debugging */
    track_page_fault(directory, fault_addr, error_code);
    
    /* Handle copy-on-write faults */
    if ((error_code & MEM_ACCESS_WRITE) && 
        !(error_code & MEM_ACCESS_RESERVED)) {
        /* This might be a COW fault - check if the page is marked COW */
        result = handle_cow_page_fault(directory, fault_addr, error_code);
        if (result == 0) {
            /* COW fault handled */
            mutex_unlock(&directory->lock);
            return 0;
        }
    }
    
    /* Handle other types of faults here */
    /* For example, handle demand paging, stack expansion, etc. */
    
    /* If we get here, the fault is not handled */
    mutex_unlock(&directory->lock);
    
    LOG_ERROR("Unhandled page fault at %p (error code: %llx) for task %d", 
              (void*)fault_addr, error_code, directory->owner_pid);
    
    return -EFAULT;
}

/*
 * Map a physical memory range into a virtual address space
 * 
 * pd: Page directory to map into
 * vaddr: Starting virtual address
 * paddr: Starting physical address
 * size: Size of the range to map in bytes
 * flags: Mapping flags (read, write, execute, etc.)
 * 
 * Returns: 0 on success, negative error code on failure
 */
int map_memory_range(page_directory_t pd, uint64_t vaddr, uint64_t paddr, 
                     size_t size, uint64_t flags) {
    struct page_directory* directory = (struct page_directory*)pd;
    int result = 0;
    
    if (!pd_system_initialized || directory == NULL) {
        return -EINVAL;
    }
    
    /* Align addresses to page boundaries */
    uint64_t start_vaddr = vaddr & ~(PAGE_SIZE_4K - 1);
    uint64_t start_paddr = paddr & ~(PAGE_SIZE_4K - 1);
    uint64_t end_vaddr = (vaddr + size + PAGE_SIZE_4K - 1) & ~(PAGE_SIZE_4K - 1);
    uint64_t num_pages = (end_vaddr - start_vaddr) / PAGE_SIZE_4K;
    
    /* Validate parameters */
    if (size == 0 || start_vaddr >= end_vaddr) {
        return -EINVAL;
    }
    
    /* Lock the page directory */
    mutex_lock(&directory->lock);
    
    /* For each page to be mapped */
    for (uint64_t i = 0; i < num_pages; i++) {
        uint64_t curr_vaddr = start_vaddr + (i * PAGE_SIZE_4K);
        uint64_t curr_paddr = start_paddr + (i * PAGE_SIZE_4K);
        
        /* Calculate indices for the page table levels */
        uint64_t pml4_idx = (curr_vaddr >> 39) & 0x1FF;
        uint64_t pdpt_idx = (curr_vaddr >> 30) & 0x1FF;
        uint64_t pd_idx = (curr_vaddr >> 21) & 0x1FF;
        uint64_t pt_idx = (curr_vaddr >> 12) & 0x1FF;
        
        /* Check if PML4 entry exists */
        uint64_t* pml4_entry = &directory->pml4_table[pml4_idx];
        uint64_t* pdpt_table;
        
        if (!(*pml4_entry & PAGE_PRESENT)) {
            /* Need to create a new PDPT table */
            pdpt_table = (uint64_t*)alloc_pages(1, ALLOC_ZERO | ALLOC_KERNEL);
            if (pdpt_table == NULL) {
                LOG_ERROR("Failed to allocate PDPT table for address %p", (void*)curr_vaddr);
                result = -ENOMEM;
                goto error_cleanup;
            }
            
            /* Set up the PML4 entry to point to the new PDPT table */
            *pml4_entry = (uint64_t)pdpt_table | PAGE_PRESENT | PAGE_WRITABLE;
            if (flags & PAGE_USER) {
                *pml4_entry |= PAGE_USER;
            }
        } else {
            /* PDPT table already exists */
            pdpt_table = (uint64_t*)(*pml4_entry & ~0xFFF);
        }
        
        /* Check if PDPT entry exists */
        uint64_t* pdpt_entry = &pdpt_table[pdpt_idx];
        uint64_t* pd_table;
        
        /* Check if we want to use a 1GB page */
        if (flags & PAGE_SIZE && (curr_vaddr & (PAGE_SIZE_1G - 1)) == 0 && 
            (curr_paddr & (PAGE_SIZE_1G - 1)) == 0 && 
            i + (PAGE_SIZE_1G / PAGE_SIZE_4K) <= num_pages) {
            
            /* Map a 1GB page */
            *pdpt_entry = curr_paddr | PAGE_PRESENT | PAGE_SIZE | (flags & ~PAGE_COW);
            
            /* Skip the pages we just mapped with this large page */
            i += (PAGE_SIZE_1G / PAGE_SIZE_4K) - 1;
            continue;
        }
        
        if (!(*pdpt_entry & PAGE_PRESENT)) {
            /* Need to create a new PD table */
            pd_table = (uint64_t*)alloc_pages(1, ALLOC_ZERO | ALLOC_KERNEL);
            if (pd_table == NULL) {
                LOG_ERROR("Failed to allocate PD table for address %p", (void*)curr_vaddr);
                result = -ENOMEM;
                goto error_cleanup;
            }
            
            /* Set up the PDPT entry to point to the new PD table */
            *pdpt_entry = (uint64_t)pd_table | PAGE_PRESENT | PAGE_WRITABLE;
            if (flags & PAGE_USER) {
                *pdpt_entry |= PAGE_USER;
            }
        } else if (*pdpt_entry & PAGE_SIZE) {
            /* Already mapped as a 1GB page - can't map part of it */
            LOG_ERROR("Address %p is already mapped as part of a 1GB page", (void*)curr_vaddr);
            result = -EEXIST;
            goto error_cleanup;
        } else {
            /* PD table already exists */
            pd_table = (uint64_t*)(*pdpt_entry & ~0xFFF);
        }
        
        /* Check if PD entry exists */
        uint64_t* pd_entry = &pd_table[pd_idx];
        uint64_t* pt_table;
        
        /* Check if we want to use a 2MB page */
        if (flags & PAGE_SIZE && (curr_vaddr & (PAGE_SIZE_2M - 1)) == 0 && 
            (curr_paddr & (PAGE_SIZE_2M - 1)) == 0 && 
            i + (PAGE_SIZE_2M / PAGE_SIZE_4K) <= num_pages) {
            
            /* Map a 2MB page */
            *pd_entry = curr_paddr | PAGE_PRESENT | PAGE_SIZE | (flags & ~PAGE_COW);
            
            /* Skip the pages we just mapped with this large page */
            i += (PAGE_SIZE_2M / PAGE_SIZE_4K) - 1;
            continue;
        }
        
        if (!(*pd_entry & PAGE_PRESENT)) {
            /* Need to create a new PT table */
            pt_table = (uint64_t*)alloc_pages(1, ALLOC_ZERO | ALLOC_KERNEL);
            if (pt_table == NULL) {
                LOG_ERROR("Failed to allocate PT table for address %p", (void*)curr_vaddr);
                result = -ENOMEM;
                goto error_cleanup;
            }
            
            /* Set up the PD entry to point to the new PT table */
            *pd_entry = (uint64_t)pt_table | PAGE_PRESENT | PAGE_WRITABLE;
            if (flags & PAGE_USER) {
                *pd_entry |= PAGE_USER;
            }
        } else if (*pd_entry & PAGE_SIZE) {
            /* Already mapped as a 2MB page - can't map part of it */
            LOG_ERROR("Address %p is already mapped as part of a 2MB page", (void*)curr_vaddr);
            result = -EEXIST;
            goto error_cleanup;
        } else {
            /* PT table already exists */
            pt_table = (uint64_t*)(*pd_entry & ~0xFFF);
        }
        
        /* Set up the PT entry to point to the physical page */
        uint64_t* pt_entry = &pt_table[pt_idx];
        
        if (*pt_entry & PAGE_PRESENT) {
            /* Page is already mapped */
            LOG_ERROR("Address %p is already mapped", (void*)curr_vaddr);
            result = -EEXIST;
            goto error_cleanup;
        }
        
        /* Create the page table entry */
        *pt_entry = curr_paddr | PAGE_PRESENT | (flags & ~PAGE_COW);
        
        /* Update page counts */
        directory->total_mapped_pages++;
        if (flags & PAGE_USER) {
            directory->total_user_pages++;
        }
    }
    
    /* Flush the TLB for the mapped range */
    flush_tlb_range(start_vaddr, end_vaddr - start_vaddr);
    
    LOG_INFO("Mapped %llu pages at %p to %p with flags %llx", 
             num_pages, (void*)start_vaddr, (void*)start_paddr, flags);
    
    mutex_unlock(&directory->lock);
    return 0;
    
error_cleanup:
    /* Note: In a real implementation, we would clean up any partially 
     * created page tables here. For simplicity, we're skipping that. */
    mutex_unlock(&directory->lock);
    return result;
}

/*
 * Unmap a range of virtual memory
 *
 * pd: Page directory to unmap from
 * vaddr: Starting virtual address
 * size: Size of the range to unmap in bytes
 * free_phys: Whether to free the underlying physical pages
 *
 * Returns: 0 on success, negative error code on failure
 */
int unmap_memory_range(page_directory_t pd, uint64_t vaddr, size_t size, bool free_phys) {
    struct page_directory* directory = (struct page_directory*)pd;
    int result = 0;
    
    if (!pd_system_initialized || directory == NULL) {
        return -EINVAL;
    }
    
    /* Align addresses to page boundaries */
    uint64_t start_vaddr = vaddr & ~(PAGE_SIZE_4K - 1);
    uint64_t end_vaddr = (vaddr + size + PAGE_SIZE_4K - 1) & ~(PAGE_SIZE_4K - 1);
    uint64_t num_pages = (end_vaddr - start_vaddr) / PAGE_SIZE_4K;
    
    /* Validate parameters */
    if (size == 0 || start_vaddr >= end_vaddr) {
        return -EINVAL;
    }
    
    /* Lock the page directory */
    mutex_lock(&directory->lock);
    
    /* For each page to be unmapped */
    for (uint64_t i = 0; i < num_pages; i++) {
        uint64_t curr_vaddr = start_vaddr + (i * PAGE_SIZE_4K);
        
        /* Calculate indices for the page table levels */
        uint64_t pml4_idx = (curr_vaddr >> 39) & 0x1FF;
        uint64_t pdpt_idx = (curr_vaddr >> 30) & 0x1FF;
        uint64_t pd_idx = (curr_vaddr >> 21) & 0x1FF;
        uint64_t pt_idx = (curr_vaddr >> 12) & 0x1FF;
        
        /* Check if PML4 entry exists */
        uint64_t* pml4_entry = &directory->pml4_table[pml4_idx];
        if (!(*pml4_entry & PAGE_PRESENT)) {
            /* Not mapped, skip */
            continue;
        }
        
        /* Get PDPT table */
        uint64_t* pdpt_table = (uint64_t*)(*pml4_entry & ~0xFFF);
        uint64_t* pdpt_entry = &pdpt_table[pdpt_idx];
        
        if (!(*pdpt_entry & PAGE_PRESENT)) {
            /* Not mapped, skip */
            continue;
        }
        
        /* Check if this is a 1GB page */
        if (*pdpt_entry & PAGE_SIZE) {
            /* This is a 1GB page - unmap the entire page if we're unmapping any part of it */
            if ((curr_vaddr & ~(PAGE_SIZE_1G - 1)) == (curr_vaddr & ~(PAGE_SIZE_4K - 1))) {
                /* This is the first 4K page in the 1GB page */
                uint64_t phys_addr = *pdpt_entry & ~0xFFF;
                
                /* Free the physical memory if requested */
                if (free_phys && !(*pdpt_entry & PAGE_COW)) {
                    free_pages((void*)phys_addr, PAGE_SIZE_1G / PAGE_SIZE_4K);
                }
                
                /* Clear the entry */
                *pdpt_entry = 0;
                
                /* Skip the rest of this 1GB page */
                i += (PAGE_SIZE_1G / PAGE_SIZE_4K) - 1;
                
                /* Update page counts */
                directory->total_mapped_pages -= (PAGE_SIZE_1G / PAGE_SIZE_4K);
                if (*pdpt_entry & PAGE_USER) {
                    directory->total_user_pages -= (PAGE_SIZE_1G / PAGE_SIZE_4K);
                }
                
                /* Check if the entire PDPT table is now empty */
                bool pdpt_empty = true;
                for (int j = 0; j < PTE_COUNT_PER_TABLE; j++) {
                    if (pdpt_table[j] & PAGE_PRESENT) {
                        pdpt_empty = false;
                        break;
                    }
                }
                
                /* If PDPT is empty, free it */
                if (pdpt_empty) {
                    free_pages(pdpt_table, 1);
                    *pml4_entry = 0;
                }
                
                continue;
            }
        }
        
        /* Get PD table */
        uint64_t* pd_table = (uint64_t*)(*pdpt_entry & ~0xFFF);
        uint64_t* pd_entry = &pd_table[pd_idx];
        
        if (!(*pd_entry & PAGE_PRESENT)) {
            /* Not mapped, skip */
            continue;
        }
        
        /* Check if this is a 2MB page */
        if (*pd_entry & PAGE_SIZE) {
            /* This is a 2MB page - unmap the entire page if we're unmapping any part of it */
            if ((curr_vaddr & ~(PAGE_SIZE_2M - 1)) == (curr_vaddr & ~(PAGE_SIZE_4K - 1))) {
                /* This is the first 4K page in the 2MB page */
                uint64_t phys_addr = *pd_entry & ~0xFFF;
                
                /* Free the physical memory if requested */
                if (free_phys && !(*pd_entry & PAGE_COW)) {
                    free_pages((void*)phys_addr, PAGE_SIZE_2M / PAGE_SIZE_4K);
                }
                
                /* Clear the entry */
                *pd_entry = 0;
                
                /* Skip the rest of this 2MB page */
                i += (PAGE_SIZE_2M / PAGE_SIZE_4K) - 1;
                
                /* Update page counts */
                directory->total_mapped_pages -= (PAGE_SIZE_2M / PAGE_SIZE_4K);
                if (*pd_entry & PAGE_USER) {
                    directory->total_user_pages -= (PAGE_SIZE_2M / PAGE_SIZE_4K);
                }
                
                /* Check if the entire PD table is now empty */
                bool pd_empty = true;
                for (int j = 0; j < PTE_COUNT_PER_TABLE; j++) {
                    if (pd_table[j] & PAGE_PRESENT) {
                        pd_empty = false;
                        break;
                    }
                }
                
                /* If PD is empty, free it */
                if (pd_empty) {
                    free_pages(pd_table, 1);
                    *pdpt_entry = 0;
                    
                    /* Check if the entire PDPT table is now empty */
                    bool pdpt_empty = true;
                    for (int j = 0; j < PTE_COUNT_PER_TABLE; j++) {
                        if (pdpt_table[j] & PAGE_PRESENT) {
                            pdpt_empty = false;
                            break;
                        }
                    }
                    
                    /* If PDPT is empty, free it */
                    if (pdpt_empty) {
                        free_pages(pdpt_table, 1);
                        *pml4_entry = 0;
                    }
                }
                
                continue;
            }
        }
        
        /* Get PT table */
        uint64_t* pt_table = (uint64_t*)(*pd_entry & ~0xFFF);
        uint64_t* pt_entry = &pt_table[pt_idx];
        
        if (!(*pt_entry & PAGE_PRESENT)) {
            /* Not mapped, skip */
            continue;
        }
        
        /* This is a normal 4K page */
        uint64_t phys_addr = *pt_entry & ~0xFFF;
        
        /* Free the physical memory if requested */
        if (free_phys && !(*pt_entry & PAGE_COW)) {
            free_pages((void*)phys_addr, 1);
        }
        
        /* Clear the entry */
        *pt_entry = 0;
        
        /* Update page counts */
        directory->total_mapped_pages--;
        if (*pt_entry & PAGE_USER) {
            directory->total_user_pages--;
        }
        
        /* Check for empty page tables and clean up if needed */
        bool pt_empty = true;
        for (int j = 0; j < PTE_COUNT_PER_TABLE; j++) {
            if (pt_table[j] & PAGE_PRESENT) {
                pt_empty = false;
                break;
            }
        }
        
        /* If PT is empty, free it */
        if (pt_empty) {
            free_pages(pt_table, 1);
            *pd_entry = 0;
            
            /* Check if the entire PD table is now empty */
            bool pd_empty = true;
            for (int j = 0; j < PTE_COUNT_PER_TABLE; j++) {
                if (pd_table[j] & PAGE_PRESENT) {
                    pd_empty = false;
                    break;
                }
            }
            
            /* If PD is empty, free it */
            if (pd_empty) {
                free_pages(pd_table, 1);
                *pdpt_entry = 0;
                
                /* Check if the entire PDPT table is now empty */
                bool pdpt_empty = true;
                for (int j = 0; j < PTE_COUNT_PER_TABLE; j++) {
                    if (pdpt_table[j] & PAGE_PRESENT) {
                        pdpt_empty = false;
                        break;
                    }
                }
                
                /* If PDPT is empty, free it */
                if (pdpt_empty) {
                    free_pages(pdpt_table, 1);
                    *pml4_entry = 0;
                }
            }
        }
    }
    
    /* Flush the TLB for the unmapped range */
    flush_tlb_range(start_vaddr, end_vaddr - start_vaddr);
    
    LOG_INFO("Unmapped %llu pages at %p with free_phys=%d", 
             num_pages, (void*)start_vaddr, free_phys);
    
    mutex_unlock(&directory->lock);
    return 0;
}

/*
 * Handle a copy-on-write page fault
 *
 * pd: Page directory where the fault occurred
 * fault_addr: Virtual address that caused the fault
 * error_code: Fault error code
 *
 * Returns: 0 if handled, negative error code if not a COW fault or on error
 */
static int handle_cow_page_fault(struct page_directory* pd, uint64_t fault_addr, uint64_t error_code) {
    uint64_t page_aligned_addr = fault_addr & ~(PAGE_SIZE_4K - 1);
    uint64_t* pte = NULL;
    uint64_t pte_value = 0;
    void* new_page = NULL;
    uint64_t phys_addr = 0;
    
    /* Calculate indices for the page table levels */
    uint64_t pml4_idx = (page_aligned_addr >> 39) & 0x1FF;
    uint64_t pdpt_idx = (page_aligned_addr >> 30) & 0x1FF;
    uint64_t pd_idx = (page_aligned_addr >> 21) & 0x1FF;
    uint64_t pt_idx = (page_aligned_addr >> 12) & 0x1FF;
    
    /* Navigate the page table hierarchy to find the PTE */
    uint64_t* pml4_entry = &pd->pml4_table[pml4_idx];
    if (!(*pml4_entry & PAGE_PRESENT)) {
        return -EFAULT;  /* Address not mapped */
    }
    
    uint64_t* pdpt_table = (uint64_t*)(*pml4_entry & ~0xFFF);
    uint64_t* pdpt_entry = &pdpt_table[pdpt_idx];
    if (!(*pdpt_entry & PAGE_PRESENT)) {
        return -EFAULT;  /* Address not mapped */
    }
    
    /* Handle 1GB pages */
    if (*pdpt_entry & PAGE_SIZE) {
        /* 1GB pages are not currently supported for COW */
        return -ENOTSUP;
    }
    
    uint64_t* pd_table = (uint64_t*)(*pdpt_entry & ~0xFFF);
    uint64_t* pd_entry = &pd_table[pd_idx];
    if (!(*pd_entry & PAGE_PRESENT)) {
        return -EFAULT;  /* Address not mapped */
    }
    
    /* Handle 2MB pages */
    if (*pd_entry & PAGE_SIZE) {
        /* 2MB pages are not currently supported for COW */
        return -ENOTSUP;
    }
    
    uint64_t* pt_table = (uint64_t*)(*pd_entry & ~0xFFF);
    uint64_t* pt_entry = &pt_table[pt_idx];
    
    /* Check if this is a COW page */
    if (!(*pt_entry & PAGE_PRESENT) || !(*pt_entry & PAGE_COW)) {
        return -EFAULT;  /* Not a COW fault */
    }
    
    /* Save current PTE value and physical address */
    pte = pt_entry;
    pte_value = *pte;
    phys_addr = pte_value & ~0xFFF;
    
    /* Allocate a new physical page */
    new_page = alloc_pages(1, ALLOC_ZERO | ALLOC_KERNEL);
    if (new_page == NULL) {
        LOG_ERROR("Failed to allocate page for COW at address %p", (void*)fault_addr);
        return -ENOMEM;
    }
    
    /* Copy content from the original page */
    memcpy(new_page, (void*)phys_addr, PAGE_SIZE_4K);
    
    /* Update the page table entry to point to the new page */
    *pte = (uint64_t)new_page | (pte_value & 0xFFF) | PAGE_WRITABLE;
    *pte &= ~PAGE_COW;  /* Clear COW flag */
    
    /* Update statistics */
    pd->cow_breaks_count++;
    
    /* Flush TLB for this page */
    flush_tlb_page(page_aligned_addr);
    
    LOG_INFO("Resolved COW fault at %p for task %d", 
             (void*)fault_addr, pd->owner_pid);
    
    return 0;
}

/*
 * Mark a range of pages as copy-on-write
 *
 * pd: Page directory to modify
 * start_addr: Starting virtual address
 * end_addr: Ending virtual address
 *
 * Returns: 0 on success, negative error code on failure
 */
static int mark_pages_cow(struct page_directory* pd, uint64_t start_addr, uint64_t end_addr) {
    int modified_pages = 0;
    
    /* Align addresses to page boundaries */
    uint64_t start_page = start_addr & ~(PAGE_SIZE_4K - 1);
    uint64_t end_page = (end_addr + PAGE_SIZE_4K - 1) & ~(PAGE_SIZE_4K - 1);
    
    /* For each page in the range */
    for (uint64_t addr = start_page; addr < end_page; addr += PAGE_SIZE_4K) {
        /* Make writable pages COW by removing write permission and adding COW flag */
        if (set_page_flags(pd, addr, PAGE_COW, PAGE_WRITABLE) == 0) {
            modified_pages++;
        }
    }
    
    LOG_INFO("Marked %d pages as COW from %p to %p", 
             modified_pages, (void*)start_addr, (void*)end_addr);
    
    return 0;
}

/*
 * Copy page tables from one page directory to another
 *
 * src: Source page directory
 * dest: Destination page directory
 * start_addr: Starting virtual address to copy
 * end_addr: Ending virtual address to copy
 *
 * Returns: 0 on success, negative error code on failure
 */
static int copy_page_tables(struct page_directory* src, struct page_directory* dest,
                          uint64_t start_addr, uint64_t end_addr) {
    int result = 0;
    uint64_t copied_pages = 0;
    
    /* Align addresses to page boundaries */
    uint64_t start_page = start_addr & ~(PAGE_SIZE_4K - 1);
    uint64_t end_page = (end_addr + PAGE_SIZE_4K - 1) & ~(PAGE_SIZE_4K - 1);
    
    /* For each page in the range */
    for (uint64_t curr_addr = start_page; curr_addr < end_page; curr_addr += PAGE_SIZE_4K) {
        /* Calculate indices for the page table levels */
        uint64_t pml4_idx = (curr_addr >> 39) & 0x1FF;
        uint64_t pdpt_idx = (curr_addr >> 30) & 0x1FF;
        uint64_t pd_idx = (curr_addr >> 21) & 0x1FF;
        uint64_t pt_idx = (curr_addr >> 12) & 0x1FF;
        
        /* Check if the page is mapped in the source */
        uint64_t* src_pml4_entry = &src->pml4_table[pml4_idx];
        if (!(*src_pml4_entry & PAGE_PRESENT)) {
            /* Not mapped, skip */
            continue;
        }
        
        /* Get PDPT table in source */
        uint64_t* src_pdpt_table = (uint64_t*)(*src_pml4_entry & ~0xFFF);
        uint64_t* src_pdpt_entry = &src_pdpt_table[pdpt_idx];
        
        if (!(*src_pdpt_entry & PAGE_PRESENT)) {
            /* Not mapped, skip */
            continue;
        }
        
        /* Check if this is a 1GB page in source */
        if (*src_pdpt_entry & PAGE_SIZE) {
            /* This is a 1GB page - copy the entire page if we're copying any part of it */
            if ((curr_addr & ~(PAGE_SIZE_1G - 1)) == (curr_addr & ~(PAGE_SIZE_4K - 1))) {
                /* This is the first 4K page in the 1GB page */
                uint64_t phys_addr = *src_pdpt_entry & ~0xFFF;
                uint64_t flags = *src_pdpt_entry & 0xFFF;
                
                /* Check if PDPT entry exists in destination */
                uint64_t* dest_pml4_entry = &dest->pml4_table[pml4_idx];
                uint64_t* dest_pdpt_table;
                
                if (!(*dest_pml4_entry & PAGE_PRESENT)) {
                    /* Need to create a new PDPT table in destination */
                    dest_pdpt_table = (uint64_t*)alloc_pages(1, ALLOC_ZERO | ALLOC_KERNEL);
                    if (dest_pdpt_table == NULL) {
                        LOG_ERROR("Failed to allocate PDPT table for copy at address %p", (void*)curr_addr);
                        result = -ENOMEM;
                        goto error_cleanup;
                    }
                    
                    /* Set up the PML4 entry in destination */
                    *dest_pml4_entry = (uint64_t)dest_pdpt_table | (*src_pml4_entry & 0xFFF);
                } else {
                    /* PDPT table already exists in destination */
                    dest_pdpt_table = (uint64_t*)(*dest_pml4_entry & ~0xFFF);
                }
                
                /* Set up the PDPT entry in destination - using same physical page */
                dest_pdpt_table[pdpt_idx] = phys_addr | flags;
                
                /* Skip the rest of this 1GB page */
                copied_pages += (PAGE_SIZE_1G / PAGE_SIZE_4K);
                curr_addr += (PAGE_SIZE_1G / PAGE_SIZE_4K) - 1;
                continue;
            }
        }
        
        /* Get PD table in source */
        uint64_t* src_pd_table = (uint64_t*)(*src_pdpt_entry & ~0xFFF);
        uint64_t* src_pd_entry = &src_pd_table[pd_idx];
        
        if (!(*src_pd_entry & PAGE_PRESENT)) {
            /* Not mapped, skip */
            continue;
        }
        
        /* Check if this is a 2MB page in source */
        if (*src_pd_entry & PAGE_SIZE) {
            /* This is a 2MB page - copy the entire page if we're copying any part of it */
            if ((curr_addr & ~(PAGE_SIZE_2M - 1)) == (curr_addr & ~(PAGE_SIZE_4K - 1))) {
                /* This is the first 4K page in the 2MB page */
                uint64_t phys_addr = *src_pd_entry & ~0xFFF;
                uint64_t flags = *src_pd_entry & 0xFFF;
                
                /* Check if PDPT entry exists in destination */
                uint64_t* dest_pml4_entry = &dest->pml4_table[pml4_idx];
                uint64_t* dest_pdpt_table;
                
                if (!(*dest_pml4_entry & PAGE_PRESENT)) {
                    /* Need to create a new PDPT table in destination */
                    dest_pdpt_table = (uint64_t*)alloc_pages(1, ALLOC_ZERO | ALLOC_KERNEL);
                    if (dest_pdpt_table == NULL) {
                        LOG_ERROR("Failed to allocate PDPT table for copy at address %p", (void*)curr_addr);
                        result = -ENOMEM;
                        goto error_cleanup;
                    }
                    
                    /* Set up the PML4 entry in destination */
                    *dest_pml4_entry = (uint64_t)dest_pdpt_table | (*src_pml4_entry & 0xFFF);
                } else {
                    /* PDPT table already exists in destination */
                    dest_pdpt_table = (uint64_t*)(*dest_pml4_entry & ~0xFFF);
                }
                
                /* Check if PD entry exists in destination */
                uint64_t* dest_pdpt_entry = &dest_pdpt_table[pdpt_idx];
                uint64_t* dest_pd_table;
                
                if (!(*dest_pdpt_entry & PAGE_PRESENT)) {
                    /* Need to create a new PD table in destination */
                    dest_pd_table = (uint64_t*)alloc_pages(1, ALLOC_ZERO | ALLOC_KERNEL);
                    if (dest_pd_table == NULL) {
                        LOG_ERROR("Failed to allocate PD table for copy at address %p", (void*)curr_addr);
                        result = -ENOMEM;
                        goto error_cleanup;
                    }
                    
                    /* Set up the PDPT entry in destination */
                    *dest_pdpt_entry = (uint64_t)dest_pd_table | (*src_pdpt_entry & 0xFFF);
                } else {
                    /* PD table already exists in destination */
                    dest_pd_table = (uint64_t*)(*dest_pdpt_entry & ~0xFFF);
                }
                
                /* Set up the PD entry in destination - using same physical page */
                dest_pd_table[pd_idx] = phys_addr | flags;
                
                /* Skip the rest of this 2MB page */
                copied_pages += (PAGE_SIZE_2M / PAGE_SIZE_4K);
                curr_addr += (PAGE_SIZE_2M / PAGE_SIZE_4K) - 1;
                continue;
            }
        }
        
        /* Get PT table in source */
        uint64_t* src_pt_table = (uint64_t*)(*src_pd_entry & ~0xFFF);
        uint64_t* src_pt_entry = &src_pt_table[pt_idx];
        
        if (!(*src_pt_entry & PAGE_PRESENT)) {
            /* Not mapped, skip */
            continue;
        }
        
        /* This is a normal 4K page */
        uint64_t phys_addr = *src_pt_entry & ~0xFFF;
        uint64_t flags = *src_pt_entry & 0xFFF;
        
        /* Check if PML4 entry exists in destination */
        uint64_t* dest_pml4_entry = &dest->pml4_table[pml4_idx];
        uint64_t* dest_pdpt_table;
        
        if (!(*dest_pml4_entry & PAGE_PRESENT)) {
            /* Need to create a new PDPT table in destination */
            dest_pdpt_table = (uint64_t*)alloc_pages(1, ALLOC_ZERO | ALLOC_KERNEL);
            if (dest_pdpt_table == NULL) {
                LOG_ERROR("Failed to allocate PDPT table for copy at address %p", (void*)curr_addr);
                result = -ENOMEM;
                goto error_cleanup;
            }
            
            /* Set up the PML4 entry in destination */
            *dest_pml4_entry = (uint64_t)dest_pdpt_table | (*src_pml4_entry & 0xFFF);
        } else {
            /* PDPT table already exists in destination */
            dest_pdpt_table = (uint64_t*)(*dest_pml4_entry & ~0xFFF);
        }
        
        /* Check if PDPT entry exists in destination */
        uint64_t* dest_pdpt_entry = &dest_pdpt_table[pdpt_idx];
        uint64_t* dest_pd_table;
        
        if (!(*dest_pdpt_entry & PAGE_PRESENT)) {
            /* Need to create a new PD table in destination */
            dest_pd_table = (uint64_t*)alloc_pages(1, ALLOC_ZERO | ALLOC_KERNEL);
            if (dest_pd_table == NULL) {
                LOG_ERROR("Failed to allocate PD table for copy at address %p", (void*)curr_addr);
                result = -ENOMEM;
                goto error_cleanup;
            }
            
            /* Set up the PDPT entry in destination */
            *dest_pdpt_entry = (uint64_t)dest_pd_table | (*src_pdpt_entry & 0xFFF);
        } else {
            /* PD table already exists in destination */
            dest_pd_table = (uint64_t*)(*dest_pdpt_entry & ~0xFFF);
        }
        
        /* Check if PD entry exists in destination */
        uint64_t* dest_pd_entry = &dest_pd_table[pd_idx];
        uint64_t* dest_pt_table;
        
        if (!(*dest_pd_entry & PAGE_PRESENT)) {
            /* Need to create a new PT table in destination */
            dest_pt_table = (uint64_t*)alloc_pages(1, ALLOC_ZERO | ALLOC_KERNEL);
            if (dest_pt_table == NULL) {
                LOG_ERROR("Failed to allocate PT table for copy at address %p", (void*)curr_addr);
                result = -ENOMEM;
                goto error_cleanup;
            }
            
            /* Set up the PD entry in destination */
            *dest_pd_entry = (uint64_t)dest_pt_table | (*src_pd_entry & 0xFFF);
        } else {
            /* PT table already exists in destination */
            dest_pt_table = (uint64_t*)(*dest_pd_entry & ~0xFFF);
        }
        
        /* Set up the PT entry in destination - using same physical page */
        dest_pt_table[pt_idx] = phys_addr | flags;
        
        copied_pages++;
    }
    
    LOG_INFO("Copied %llu pages from task %d to task %d", 
             copied_pages, src->owner_pid, dest->owner_pid);
    
    return 0;
    
error_cleanup:
    /* Note: In a real implementation, we would clean up any partially 
     * created page tables here. For simplicity, we're skipping that
     * detailed cleanup. */
    LOG_ERROR("Failed to copy page tables from task %d to task %d, error %d", 
             src->owner_pid, dest->owner_pid, result);
    return result;
}

/*
 * Track a page fault for debugging and analysis
 *
 * pd: Page directory where the fault occurred
 * fault_addr: Virtual address that caused the fault
 * error_code: Fault error code
 */
static void track_page_fault(struct page_directory* pd, uint64_t fault_addr, uint64_t error_code) {
    uint32_t index;
    
    /* Increment fault counter */
    pd->page_fault_count++;
    
    /* Store fault information in circular buffer */
    index = pd->last_fault_index;
    pd->last_page_faults[index].vaddr = fault_addr;
    pd->last_page_faults[index].access_type = error_code;
    pd->last_page_faults[index].timestamp = get_system_time();
    pd->last_page_faults[index].task_id = pd->owner_pid;
    
    /* Update circular buffer index */
    pd->last_fault_index = (index + 1) % 10;
    
    LOG_DEBUG("Page fault #%llu at %p (error code: %llx) for task %d", 
              pd->page_fault_count, (void*)fault_addr, error_code, pd->owner_pid);
}

/*
 * Get the physical address corresponding to a virtual address
 *
 * pd: Page directory to use for translation
 * virtual_addr: Virtual address to translate
 *
 * Returns: Physical address or NULL if not mapped
 */
static void* get_physical_address(struct page_directory* pd, uint64_t virtual_addr) {
    uint64_t page_aligned_addr = virtual_addr & ~(PAGE_SIZE_4K - 1);
    uint64_t page_offset = virtual_addr & (PAGE_SIZE_4K - 1);
    uint64_t phys_addr = 0;
    
    /* Calculate indices for the page table levels */
    uint64_t pml4_idx = (page_aligned_addr >> 39) & 0x1FF;
    uint64_t pdpt_idx = (page_aligned_addr >> 30) & 0x1FF;
    uint64_t pd_idx = (page_aligned_addr >> 21) & 0x1FF;
    uint64_t pt_idx = (page_aligned_addr >> 12) & 0x1FF;
    
    /* Navigate the page table hierarchy to find the physical address */
    uint64_t* pml4_entry = &pd->pml4_table[pml4_idx];
    if (!(*pml4_entry & PAGE_PRESENT)) {
        return NULL;  /* Address not mapped */
    }
    
    uint64_t* pdpt_table = (uint64_t*)(*pml4_entry & ~0xFFF);
    uint64_t* pdpt_entry = &pdpt_table[pdpt_idx];
    if (!(*pdpt_entry & PAGE_PRESENT)) {
        return NULL;  /* Address not mapped */
    }
    
    /* Handle 1GB pages */
    if (*pdpt_entry & PAGE_SIZE) {
        /* 1GB page */
        phys_addr = (*pdpt_entry & ~0xFFF) + (page_aligned_addr & (PAGE_SIZE_1G - 1)) + page_offset;
        return (void*)phys_addr;
    }
    
    uint64_t* pd_table = (uint64_t*)(*pdpt_entry & ~0xFFF);
    uint64_t* pd_entry = &pd_table[pd_idx];
    if (!(*pd_entry & PAGE_PRESENT)) {
        return NULL;  /* Address not mapped */
    }
    
    /* Handle 2MB pages */
    if (*pd_entry & PAGE_SIZE) {
        /* 2MB page */
        phys_addr = (*pd_entry & ~0xFFF) + (page_aligned_addr & (PAGE_SIZE_2M - 1)) + page_offset;
        return (void*)phys_addr;
    }
    
    uint64_t* pt_table = (uint64_t*)(*pd_entry & ~0xFFF);
    uint64_t* pt_entry = &pt_table[pt_idx];
    if (!(*pt_entry & PAGE_PRESENT)) {
        return NULL;  /* Address not mapped */
    }
    
    /* Normal 4K page */
    phys_addr = (*pt_entry & ~0xFFF) + page_offset;
    return (void*)phys_addr;
}

/*
 * Set or clear flags in a page table entry
 *
 * pd: Page directory to modify
 * virtual_addr: Virtual address of the page
 * flags: Flags to set
 * mask: Flags to clear (only bits that are 1 will be cleared)
 *
 * Returns: 0 on success, negative error code if not mapped
 */
static int set_page_flags(struct page_directory* pd, uint64_t virtual_addr, uint64_t flags, uint64_t mask) {
    uint64_t page_aligned_addr = virtual_addr & ~(PAGE_SIZE_4K - 1);
    
    /* Calculate indices for the page table levels */
    uint64_t pml4_idx = (page_aligned_addr >> 39) & 0x1FF;
    uint64_t pdpt_idx = (page_aligned_addr >> 30) & 0x1FF;
    uint64_t pd_idx = (page_aligned_addr >> 21) & 0x1FF;
    uint64_t pt_idx = (page_aligned_addr >> 12) & 0x1FF;
    
    /* Navigate the page table hierarchy to find the PTE */
    uint64_t* pml4_entry = &pd->pml4_table[pml4_idx];
    if (!(*pml4_entry & PAGE_PRESENT)) {
        return -EFAULT;  /* Address not mapped */
    }
    
    uint64_t* pdpt_table = (uint64_t*)(*pml4_entry & ~0xFFF);
    uint64_t* pdpt_entry = &pdpt_table[pdpt_idx];
    if (!(*pdpt_entry & PAGE_PRESENT)) {
        return -EFAULT;  /* Address not mapped */
    }
    
    /* Handle 1GB pages */
    if (*pdpt_entry & PAGE_SIZE) {
        /* Update flags for 1GB page */
        *pdpt_entry = (*pdpt_entry & ~mask) | flags;
        return 0;
    }
    
    uint64_t* pd_table = (uint64_t*)(*pdpt_entry & ~0xFFF);
    uint64_t* pd_entry = &pd_table[pd_idx];
    if (!(*pd_entry & PAGE_PRESENT)) {
        return -EFAULT;  /* Address not mapped */
    }
    
    /* Handle 2MB pages */
    if (*pd_entry & PAGE_SIZE) {
        /* Update flags for 2MB page */
        *pd_entry = (*pd_entry & ~mask) | flags;
        return 0;
    }
    
    uint64_t* pt_table = (uint64_t*)(*pd_entry & ~0xFFF);
    uint64_t* pt_entry = &pt_table[pt_idx];
    if (!(*pt_entry & PAGE_PRESENT)) {
        return -EFAULT;  /* Address not mapped */
    }
    
    /* Update flags for 4K page */
    *pt_entry = (*pt_entry & ~mask) | flags;
    
    /* Flush TLB for this page */
    flush_tlb_page(page_aligned_addr);
    
    return 0;
}

/*
 * Dump page directory information for debugging
 *
 * pd: Page directory to dump
 * verbose: Whether to print detailed page table information
 */
void dump_page_directory(page_directory_t pd, bool verbose) {
    struct page_directory* directory = (struct page_directory*)pd;
    
    if (!pd_system_initialized || directory == NULL) {
        LOG_ERROR("Invalid page directory");
        return;
    }
    
    /* Lock the page directory */
    mutex_lock(&directory->lock);
    
    LOG_INFO("=== Page Directory Info ===");
    LOG_INFO("Owner:              Task %d", directory->owner_pid);
    LOG_INFO("PML4 Table:         %p", directory->pml4_table);
    LOG_INFO("CR3 Value:          0x%llx", directory->cr3_value);
    LOG_INFO("Reference Count:    %d", directory->ref_count);
    LOG_INFO("Total Mapped Pages: %llu", directory->total_mapped_pages);
    LOG_INFO("User Pages:         %llu", directory->total_user_pages);
    LOG_INFO("Page Faults:        %llu", directory->page_fault_count);
    LOG_INFO("COW Breaks:         %llu", directory->cow_breaks_count);
    
    /* Print recent page faults */
    LOG_INFO("=== Recent Page Faults ===");
    for (int i = 0; i < 10; i++) {
        int idx = (directory->last_fault_index - 1 - i + 10) % 10;
        if (directory->last_page_faults[idx].vaddr == 0) {
            continue;  /* No fault recorded in this slot */
        }
        
        LOG_INFO("Fault #%d: Address %p, Type 0x%llx, Task %d, Time %llu",
                i + 1,
                (void*)directory->last_page_faults[idx].vaddr,
                directory->last_page_faults[idx].access_type,
                directory->last_page_faults[idx].task_id,
                directory->last_page_faults[idx].timestamp);
    }
    
    if (verbose) {
        /* Detailed page table dump - for brevity, we're not implementing the
         * full traversal here, just showing the concept */
        LOG_INFO("=== Page Table Details ===");
        for (int i = 0; i < 512; i++) {
            if (directory->pml4_table[i] & PAGE_PRESENT) {
                LOG_INFO("PML4[%d]: 0x%llx (Present)", i, directory->pml4_table[i]);
            }
        }
    }
    
    LOG_INFO("===========================");
    
    mutex_unlock(&directory->lock);
}

/*
 * Get memory usage statistics for a page directory
 *
 * pd: Page directory to query
 * stats: Pointer to structure to fill with statistics
 */
void get_page_stats(page_directory_t pd, struct page_directory_stats* stats) {
    struct page_directory* directory = (struct page_directory*)pd;
    
    if (!pd_system_initialized || directory == NULL || stats == NULL) {
        return;
    }
    
    /* Lock the page directory */
    mutex_lock(&directory->lock);
    
    /* Fill in statistics */
    stats->owner_pid = directory->owner_pid;
    stats->ref_count = directory->ref_count;
    stats->total_mapped_pages = directory->total_mapped_pages;
    stats->total_user_pages = directory->total_user_pages;
    stats->page_fault_count = directory->page_fault_count;
    stats->cow_breaks_count = directory->cow_breaks_count;
    
    mutex_unlock(&directory->lock);
}

/*
 * Validate page tables for corruption
 *
 * pd: Page directory to validate
 *
 * Returns: 0 if valid, negative error code if corruption detected
 */
int validate_page_tables(page_directory_t pd) {
    struct page_directory* directory = (struct page_directory*)pd;
    int result = 0;
    
    /* Statistics for validation */
    struct {
        uint64_t total_pml4_entries;
        uint64_t total_pdpt_entries;
        uint64_t total_pd_entries;
        uint64_t total_pt_entries;
        uint64_t total_1gb_pages;
        uint64_t total_2mb_pages;
        uint64_t total_4k_pages;
        uint64_t invalid_entries;
        uint64_t misaligned_entries;
        uint64_t invalid_flags;
        uint64_t hierarchy_errors;
    } stats = {0};
    
    if (!pd_system_initialized || directory == NULL) {
        return -EINVAL;
    }
    
    /* Lock the page directory */
    mutex_lock(&directory->lock);
    
    LOG_INFO("Validating page directory for task %d", directory->owner_pid);
    
    /* Validate PML4 table */
    if (directory->pml4_table == NULL) {
        LOG_ERROR("PML4 table is NULL");
        result = -EFAULT;
        goto cleanup;
    }
    
    /* Check alignment of PML4 table */
    if (((uint64_t)directory->pml4_table & (PAGE_SIZE_4K - 1)) != 0) {
        LOG_ERROR("PML4 table is not page-aligned: %p", directory->pml4_table);
        stats.misaligned_entries++;
        result = -EFAULT;
        goto cleanup;
    }
    
    /* Validate all page table entries */
    for (int pml4_idx = 0; pml4_idx < PTE_COUNT_PER_TABLE; pml4_idx++) {
        uint64_t pml4_entry = directory->pml4_table[pml4_idx];
        
        if (!(pml4_entry & PAGE_PRESENT)) {
            continue;  /* Not present, skip */
        }
        
        stats.total_pml4_entries++;
        
        /* Check PML4 entry alignment */
        if ((pml4_entry & ~0xFFF) & (PAGE_SIZE_4K - 1)) {
            LOG_ERROR("PML4 entry %d points to non-page-aligned PDPT: 0x%llx", 
                     pml4_idx, pml4_entry);
            stats.misaligned_entries++;
            result = -EFAULT;
            continue;
        }
        
        /* Check invalid flag combinations */
        if ((pml4_entry & PAGE_SIZE) != 0) {
            LOG_ERROR("PML4 entry %d has PAGE_SIZE flag set (invalid): 0x%llx", 
                     pml4_idx, pml4_entry);
            stats.invalid_flags++;
            result = -EFAULT;
            continue;
        }
        
        /* Get PDPT table */
        uint64_t* pdpt_table = (uint64_t*)(pml4_entry & ~0xFFF);
        
        /* Validate PDPT table */
        for (int pdpt_idx = 0; pdpt_idx < PTE_COUNT_PER_TABLE; pdpt_idx++) {
            uint64_t pdpt_entry = pdpt_table[pdpt_idx];
            
            if (!(pdpt_entry & PAGE_PRESENT)) {
                continue;  /* Not present, skip */
            }
            
            stats.total_pdpt_entries++;
            
            /* Check if this is a 1GB page */
            if (pdpt_entry & PAGE_SIZE) {
                /* 1GB page */
                stats.total_1gb_pages++;
                
                /* Check 1GB page alignment */
                if ((pdpt_entry & ~0xFFF) & (PAGE_SIZE_1G - 1)) {
                    LOG_ERROR("1GB page at PML4[%d]->PDPT[%d] is not 1GB-aligned: 0x%llx", 
                             pml4_idx, pdpt_idx, pdpt_entry);
                    stats.misaligned_entries++;
                    result = -EFAULT;
                }
                
                /* Validate permission flags */
                if ((pdpt_entry & PAGE_COW) && (pdpt_entry & PAGE_WRITABLE)) {
                    LOG_ERROR("1GB page at PML4[%d]->PDPT[%d] has inconsistent permissions (COW + writable)", 
                             pml4_idx, pdpt_idx);
                    stats.invalid_flags++;
                    result = -EFAULT;
                }
                
                continue;  /* Skip further checks for 1GB pages */
            }
            
            /* Check PDPT entry alignment */
            if ((pdpt_entry & ~0xFFF) & (PAGE_SIZE_4K - 1)) {
                LOG_ERROR("PDPT entry at PML4[%d]->PDPT[%d] points to non-page-aligned PD: 0x%llx", 
                         pml4_idx, pdpt_idx, pdpt_entry);
                stats.misaligned_entries++;
                result = -EFAULT;
                continue;
            }
            
            /* Get PD table */
            uint64_t* pd_table = (uint64_t*)(pdpt_entry & ~0xFFF);
            
            /* Validate PD table */
            for (int pd_idx = 0; pd_idx < PTE_COUNT_PER_TABLE; pd_idx++) {
                uint64_t pd_entry = pd_table[pd_idx];
                
                if (!(pd_entry & PAGE_PRESENT)) {
                    continue;  /* Not present, skip */
                }
                
                stats.total_pd_entries++;
                
                /* Check if this is a 2MB page */
                if (pd_entry & PAGE_SIZE) {
                    /* 2MB page */
                    stats.total_2mb_pages++;
                    
                    /* Check 2MB page alignment */
                    if ((pd_entry & ~0xFFF) & (PAGE_SIZE_2M - 1)) {
                        LOG_ERROR("2MB page at PML4[%d]->PDPT[%d]->PD[%d] is not 2MB-aligned: 0x%llx", 
                                 pml4_idx, pdpt_idx, pd_idx, pd_entry);
                        stats.misaligned_entries++;
                        result = -EFAULT;
                    }
                    
                    /* Validate permission flags */
                    if ((pd_entry & PAGE_COW) && (pd_entry & PAGE_WRITABLE)) {
                        LOG_ERROR("2MB page at PML4[%d]->PDPT[%d]->PD[%d] has inconsistent permissions (COW + writable)", 
                                 pml4_idx, pdpt_idx, pd_idx);
                        stats.invalid_flags++;
                        result = -EFAULT;
                    }
                    
                    continue;  /* Skip further checks for 2MB pages */
                }
                
                /* Check PD entry alignment */
                if ((pd_entry & ~0xFFF) & (PAGE_SIZE_4K - 1)) {
                    LOG_ERROR("PD entry at PML4[%d]->PDPT[%d]->PD[%d] points to non-page-aligned PT: 0x%llx", 
                             pml4_idx, pdpt_idx, pd_idx, pd_entry);
                    stats.misaligned_entries++;
                    result = -EFAULT;
                    continue;
                }
                
                /* Get PT table */
                uint64_t* pt_table = (uint64_t*)(pd_entry & ~0xFFF);
                
                /* Validate PT table */
                for (int pt_idx = 0; pt_idx < PTE_COUNT_PER_TABLE; pt_idx++) {
                    uint64_t pt_entry = pt_table[pt_idx];
                    
                    if (!(pt_entry & PAGE_PRESENT)) {
                        continue;  /* Not present, skip */
                    }
                    
                    stats.total_pt_entries++;
                    stats.total_4k_pages++;
                    
                    /* Check page alignment */
                    if ((pt_entry & ~0xFFF) & (PAGE_SIZE_4K - 1)) {
                        LOG_ERROR("PT entry at PML4[%d]->PDPT[%d]->PD[%d]->PT[%d] points to non-page-aligned memory: 0x%llx", 
                                 pml4_idx, pdpt_idx, pd_idx, pt_idx, pt_entry);
                        stats.misaligned_entries++;
                        result = -EFAULT;
                    }
                    
                    /* Check invalid flag combinations */
                    if (pt_entry & PAGE_SIZE) {
                        LOG_ERROR("PT entry at PML4[%d]->PDPT[%d]->PD[%d]->PT[%d] has PAGE_SIZE flag set (invalid): 0x%llx", 
                                 pml4_idx, pdpt_idx, pd_idx, pt_idx, pt_entry);
                        stats.invalid_flags++;
                        result = -EFAULT;
                    }
                    
                    /* Validate permission flags */
                    if ((pt_entry & PAGE_COW) && (pt_entry & PAGE_WRITABLE)) {
                        LOG_ERROR("Page at PML4[%d]->PDPT[%d]->PD[%d]->PT[%d] has inconsistent permissions (COW + writable)", 
                                 pml4_idx, pdpt_idx, pd_idx, pt_idx);
                        stats.invalid_flags++;
                        result = -EFAULT;
                    }
                }
            }
        }
    }
    
    /* Validate reference counting - compare actual mapped pages with counters */
    uint64_t total_pages = stats.total_1gb_pages * (PAGE_SIZE_1G / PAGE_SIZE_4K) +
                          stats.total_2mb_pages * (PAGE_SIZE_2M / PAGE_SIZE_4K) +
                          stats.total_4k_pages;
    
    if (total_pages != directory->total_mapped_pages) {
        LOG_ERROR("Reference count mismatch: directory says %llu mapped pages, but found %llu", 
                 directory->total_mapped_pages, total_pages);
        result = -EFAULT;
    }
    
    /* Log validation statistics */
    LOG_INFO("=== Page Directory Validation Stats ===");
    LOG_INFO("  PML4 Entries:     %llu", stats.total_pml4_entries);
    LOG_INFO("  PDPT Entries:     %llu", stats.total_pdpt_entries);
    LOG_INFO("  PD Entries:       %llu", stats.total_pd_entries);
    LOG_INFO("  PT Entries:       %llu", stats.total_pt_entries);
    LOG_INFO("  1GB Pages:        %llu", stats.total_1gb_pages);
    LOG_INFO("  2MB Pages:        %llu", stats.total_2mb_pages);
    LOG_INFO("  4KB Pages:        %llu", stats.total_4k_pages);
    LOG_INFO("  Total Pages:      %llu", total_pages);
    LOG_INFO("  Misaligned:       %llu", stats.misaligned_entries);
    LOG_INFO("  Invalid Flags:    %llu", stats.invalid_flags);
    LOG_INFO("  Hierarchy Errors: %llu", stats.hierarchy_errors);
    
    if (result == 0) {
        LOG_INFO("Validation successful - page directory is consistent");
    } else {
        LOG_ERROR("Validation failed - page directory has %d errors", -result);
    }
    
cleanup:
    mutex_unlock(&directory->lock);
    return result;
}

/*
 * Update page directory statistics
 *
 * p
