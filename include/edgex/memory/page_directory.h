/*
 * EdgeX OS - Page Directory Management Interface
 *
 * This file defines the interface for managing page directories and memory
 * mappings in EdgeX OS. The page directory system is responsible for:
 *   - Virtual memory address translation
 *   - Memory protection and access control
 *   - Shared memory mapping
 *   - Copy-on-write memory optimization
 *   - Page fault handling
 *
 * The memory management system in EdgeX OS uses a multi-level paging structure
 * compatible with modern x86-64 processors, providing efficient address translation
 * with support for multiple page sizes to optimize TLB usage.
 */

#ifndef EDGEX_MEMORY_PAGE_DIRECTORY_H
#define EDGEX_MEMORY_PAGE_DIRECTORY_H

#include <edgex/types.h>
#include <edgex/memory/physical.h>

/*
 * Multi-level Paging Structure
 * ----------------------------
 *
 * EdgeX OS uses a 4-level paging structure for x86-64, consisting of:
 *   - PML4 (Page Map Level 4)
 *   - PDPT (Page Directory Pointer Table)
 *   - PD (Page Directory)
 *   - PT (Page Table)
 *
 * This structure allows for efficient address translation with minimal memory overhead.
 * The virtual address is divided into five components:
 *   - Bits 0-11:   Page offset (12 bits)
 *   - Bits 12-20:  PT index (9 bits)
 *   - Bits 21-29:  PD index (9 bits)
 *   - Bits 30-38:  PDPT index (9 bits)
 *   - Bits 39-47:  PML4 index (9 bits)
 *   - Bits 48-63:  Sign extension
 *
 * This structure supports a 48-bit virtual address space (256 TB), which is
 * sufficient for most applications. Future extensions may support larger
 * address spaces using 5-level paging.
 */

/* Page size constants */
/**
 * EdgeX OS supports multiple page sizes to optimize memory usage and TLB efficiency.
 * Larger page sizes reduce TLB pressure but may lead to internal fragmentation.
 */
#define PAGE_SIZE_4K       (4 * 1024)        /* 4 KB page (standard) */
#define PAGE_SIZE_2M       (2 * 1024 * 1024) /* 2 MB page (huge) */
#define PAGE_SIZE_1G       (1024 * 1024 * 1024) /* 1 GB page (giant) */

/* Page directory flags */
/**
 * These flags control page properties and access permissions.
 * They can be combined with the bitwise OR operator (|).
 */
#define PAGE_FLAG_PRESENT  (1ULL << 0)  /* Page is present in memory */
#define PAGE_FLAG_WRITE    (1ULL << 1)  /* Page is writable */
#define PAGE_FLAG_USER     (1ULL << 2)  /* Page is accessible from user mode */
#define PAGE_FLAG_PWT      (1ULL << 3)  /* Page write-through caching enabled */
#define PAGE_FLAG_PCD      (1ULL << 4)  /* Page cache disabled */
#define PAGE_FLAG_ACCESSED (1ULL << 5)  /* Page has been accessed */
#define PAGE_FLAG_DIRTY    (1ULL << 6)  /* Page has been written to */
#define PAGE_FLAG_HUGE     (1ULL << 7)  /* Huge page (2MB) */
#define PAGE_FLAG_GLOBAL   (1ULL << 8)  /* Global page (shared across all processes) */
#define PAGE_FLAG_COW      (1ULL << 9)  /* Copy-on-write page */
#define PAGE_FLAG_NOEXEC   (1ULL << 63) /* Page is non-executable (NX) */

/* Common permission combinations */
#define PAGE_PERM_NONE     0
#define PAGE_PERM_R        (PAGE_FLAG_PRESENT)
#define PAGE_PERM_RW       (PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE)
#define PAGE_PERM_RX       (PAGE_FLAG_PRESENT)
#define PAGE_PERM_RWX      (PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE)

/* Page mapping flags */
/**
 * These flags control how memory is mapped into the address space.
 * They can be combined with the bitwise OR operator (|).
 */
#define MAP_FLAG_FIXED     (1 << 0)  /* Map at the specified virtual address */
#define MAP_FLAG_SHARED    (1 << 1)  /* Create a shared mapping */
#define MAP_FLAG_PRIVATE   (1 << 2)  /* Create a private (COW) mapping */
#define MAP_FLAG_ANON      (1 << 3)  /* Create an anonymous mapping */
#define MAP_FLAG_STACK     (1 << 4)  /* Mapping is used for stack (grows downward) */
#define MAP_FLAG_POPULATE  (1 << 5)  /* Pre-populate page tables */
#define MAP_FLAG_HUGETLB   (1 << 6)  /* Use huge pages if available */

/* Page directory handle (opaque to user code) */
typedef struct page_directory* page_dir_t;

/* Memory region descriptor */
typedef struct {
    void*    start;          /* Start of the memory region */
    size_t   size;           /* Size of the region in bytes */
    uint64_t flags;          /* Region flags (PAGE_FLAG_*) */
    void*    physical_base;  /* Base physical address (if applicable) */
    char     name[32];       /* Name of the region (for debugging) */
} memory_region_t;

/**
 * Initialize the page directory system
 *
 * This function initializes the page directory management subsystem.
 * It is called automatically during system startup and should not be
 * called directly by application code.
 */
void init_page_directory_system(void);

/**
 * Create a new page directory
 *
 * @param flags  Flags controlling directory creation
 *
 * @return Handle to the new page directory or NULL on failure
 *
 * This function creates a new, empty page directory. The page directory
 * serves as the root of a virtual address space and is typically associated
 * with a task.
 *
 * Example:
 *   page_dir_t new_dir = create_page_directory(0);
 *   if (new_dir == NULL) {
 *       // Handle error
 *   }
 */
page_dir_t create_page_directory(uint32_t flags);

/**
 * Clone an existing page directory
 *
 * @param source  Handle to the source page directory
 * @param flags   Flags controlling clone behavior
 *
 * @return Handle to the cloned page directory or NULL on failure
 *
 * This function creates a new page directory by cloning an existing one.
 * The clone is initially identical to the source, but any subsequent
 * modifications affect only the clone, not the source.
 *
 * If MAP_FLAG_PRIVATE is specified, all writable pages are marked as
 * copy-on-write, allowing efficient memory sharing until one directory
 * modifies the page.
 *
 * Example:
 *   // Clone a directory for a new process (with COW optimization)
 *   page_dir_t child_dir = clone_page_directory(parent_dir, MAP_FLAG_PRIVATE);
 *   if (child_dir == NULL) {
 *       // Handle error
 *   }
 */
page_dir_t clone_page_directory(page_dir_t source, uint32_t flags);

/**
 * Destroy a page directory
 *
 * @param dir  Handle to the page directory to destroy
 *
 * This function destroys a page directory, freeing all associated resources.
 * Any physical memory pages that are referenced only by this directory are
 * also freed.
 *
 * Example:
 *   destroy_page_directory(task_dir);
 */
void destroy_page_directory(page_dir_t dir);

/**
 * Map a physical memory region into a page directory
 *
 * @param dir           Handle to the page directory
 * @param virt_addr     Desired virtual address (or NULL for auto-allocation)
 * @param phys_addr     Physical address to map
 * @param size          Size of the region to map (in bytes)
 * @param flags         Page flags (PAGE_FLAG_* constants)
 *
 * @return Mapped virtual address or NULL on failure
 *
 * This function maps a region of physical memory into the virtual address space
 * defined by the specified page directory. If virt_addr is NULL, the system
 * automatically selects an available virtual address.
 *
 * Example:
 *   // Map a device's memory-mapped I/O region
 *   void* mmio = map_physical_memory(task_dir, NULL, 0xFED00000, 0x1000, PAGE_PERM_RW);
 *   if (mmio == NULL) {
 *       // Handle error
 *   }
 */
void* map_physical_memory(page_dir_t dir, void* virt_addr, physical_addr_t phys_addr,
                        size_t size, uint64_t flags);

/**
 * Map a region of anonymous memory into a page directory
 *
 * @param dir        Handle to the page directory
 * @param virt_addr  Desired virtual address (or NULL for auto-allocation)
 * @param size       Size of the region to map (in bytes)
 * @param flags      Page flags (PAGE_FLAG_* constants)
 * @param map_flags  Mapping control flags (MAP_FLAG_* constants)
 *
 * @return Mapped virtual address or NULL on failure
 *
 * This function maps a region of anonymous memory (not backed by a file)
 * into the virtual address space defined by the specified page directory.
 * The memory is initially zeroed.
 *
 * Example:
 *   // Allocate 1MB of anonymous memory
 *   void* buffer = map_anonymous_memory(task_dir, NULL, 1024*1024, PAGE_PERM_RW, 0);
 *   if (buffer == NULL) {
 *       // Handle error
 *   }
 */
void* map_anonymous_memory(page_dir_t dir, void* virt_addr, size_t size,
                         uint64_t flags, uint32_t map_flags);

/**
 * Unmap a memory region from a page directory
 *
 * @param dir        Handle to the page directory
 * @param virt_addr  Virtual address of the region to unmap
 * @param size       Size of the region to unmap (in bytes)
 *
 * @return 0 on success, negative error code on failure
 *
 * This function unmaps a region of memory from the virtual address space
 * defined by the specified page directory. If the memory was mapped with
 * MAP_FLAG_PRIVATE or MAP_FLAG_ANON, the physical pages are freed if no
 * other mappings to them exist.
 *
 * Example:
 *   // Unmap a previously mapped memory region
 *   int result = unmap_memory(task_dir, buffer, 1024*1024);
 *   if (result != 0) {
 *       // Handle error
 *   }
 */
int unmap_memory(page_dir_t dir, void* virt_addr, size_t size);

/**
 * Change the access permissions for a memory region
 *
 * @param dir        Handle to the page directory
 * @param virt_addr  Virtual address of the region
 * @param size       Size of the region (in bytes)
 * @param flags      New page flags (PAGE_FLAG_* constants)
 *
 * @return 0 on success, negative error code on failure
 *
 * This function changes the access permissions for a previously mapped
 * memory region. It can be used to implement dynamic memory protection,
 * such as making a region read-only or executable.
 *
 * Example:
 *   // Make a code region executable after loading
 *   int result = change_memory_permissions(task_dir, code_buffer, code_size,
 *                                        PAGE_PERM_RX);
 *   if (result != 0) {
 *       // Handle error
 *   }
 */
int change_memory_permissions(page_dir_t dir, void* virt_addr, size_t size, uint64_t flags);

/**
 * Get information about a memory region
 *
 * @param dir        Handle to the page directory
 * @param virt_addr  Virtual address within the region to query
 * @param region     Pointer to a memory_region_t structure to fill
 *
 * @return 0 on success, negative error code on failure
 *
 * This function retrieves information about a memory region containing the
 * specified virtual address. The region structure is filled with information
 * about the memory region's properties.
 *
 * Example:
 *   memory_region_t region;
 *   int result = get_memory_region_info(task_dir, ptr, &region);
 *   if (result == 0) {
 *       printf("Memory region at %p, size: %zu bytes, flags: %llx\n",
 *              region.start, region.size, region.flags);
 *   }
 */
int get_memory_region_info(page_dir_t dir, void* virt_addr, memory_region_t* region);

/**
 * Find a free virtual address range in a page directory
 *
 * @param dir    Handle to the page directory
 * @param size   Size of the address range needed (in bytes)
 * @param align  Alignment requirement (in bytes, must be a power of 2)
 *
 * @return Start of the free virtual address range or NULL if no suitable range exists
 *
 * This function searches for a free virtual address range in the specified
 * page directory that meets the size and alignment requirements.
 *
 * Example:
 *   // Find a 2MB-aligned 16MB region
 *   void* addr = find_free_virtual_address(task_dir, 16*1024*1024, 2*1024*1024);
 *   if (addr == NULL) {
 *       // Handle error
 *   }
 */
void* find_free_virtual_address(page_dir_t dir, size_t size, size_t align);

/**
 * Enable the copy-on-write mechanism for a memory region
 *
 * @param dir        Handle to the page directory
 * @param virt_addr  Virtual address of the region
 * @param size       Size of the region (in bytes)
 *
 * @return 0 on success, negative error code on failure
 *
 * This function enables copy-on-write protection for a memory region.
 * When enabled, any attempt to write to the region will create a private
 * copy of the affected page(s) for the current task.
 *
 * Example:
 *   // Enable COW for a shared data region
 *   int result = enable_copy_on_write(task_dir, shared_data, data_size);
 *   if (result != 0) {
 *       // Handle error
 *   }
 */
int enable_copy_on_write(page_dir_t dir, void* virt_addr, size_t size);

/**
 * Activate a page directory for the current CPU core
 *
 * @param dir  Handle to the page directory to activate
 *
 * @return Previous page directory handle or NULL on failure
 *
 * This function activates the specified page directory for the current CPU core,
 * making its memory mappings available to the executing code. It updates the
 * CR3 register and flushes the TLB to ensure all address translations use
 * the new page directory.
 *
 * This function is typically called during context switching to change the
 * address space when switching between tasks.
 *
 * Example:
 *   // Switch to task's address space
 *   page_dir_t prev_dir = activate_page_directory(task_dir);
 *   if (prev_dir == NULL) {
 *       // Handle error
 *   }
 */
page_dir_t activate_page_directory(page_dir_t dir);

/**
 * Get the currently active page directory
 *
 * @return Handle to the currently active page directory
 *
 * This function returns a handle to the page directory that is currently
 * active on the CPU core executing the call.
 *
 * Example:
 *   page_dir_t current_dir = get_active_page_directory();
 */
page_dir_t get_active_page_directory(void);

/*
 * Page Fault Handling
 * ------------------
 *
 * Page faults occur when a memory access violates the page directory's access
 * permissions or when a page is not present in memory. EdgeX OS uses page faults
 * to implement several memory management features:
 *
 * 1. Demand Paging: Pages are allocated only when they are first accessed,
 *    allowing for efficient memory utilization.
 *
 * 2. Copy-on-Write: When a write occurs to a COW page, a page fault is generated,
 *    and the fault handler creates a private copy for the writing task.
 *
 * 3. Memory Protection: Attempts to access memory without the proper permissions
 *    (e.g., writing to read-only memory or executing non-executable memory)
 *    generate page faults that are reported to the task as memory protection
 *    violations.
 *
 * 4. Swapping: If a page has been swapped out to disk, a page fault triggers
 *    the page to be loaded back into memory.
 *
 * The page fault handler (page_fault_handler) is invoked by the CPU's interrupt
 * mechanism (interrupt 14 on x86-64). It examines the fault type, the faulting
 * address, and the current page directory to determine the appropriate action.
 */

/* Page fault error codes */
#define PF_ERROR_PRESENT   (1 << 0)  /* Page was present but protection violated */
#define PF_ERROR_WRITE     (1 << 1)  /* Fault caused by a write access */
#define PF_ERROR_USER      (1 << 2)  /* Fault occurred in user mode */
#define PF_ERROR_RESERVED  (1 << 3)  /* Fault caused by reserved bit violation */
#define PF_ERROR_EXEC      (1 << 4)  /* Fault caused by instruction fetch */

/* Page fault handler flags */
#define PF_HANDLER_IGNORE  (1 << 0)  /* Ignore this fault (for custom handlers) */
#define PF_HANDLER_RETRY   (1 << 1)  /* Retry the faulting instruction */
#define PF_HANDLER_SIGNAL  (1 << 2)  /* Signal the task about the fault */

/* Page fault information structure */
typedef struct {
    void*    fault_address;  /* Address that caused the fault */
    uint32_t error_code;     /* CPU-provided error code */
    uint32_t pid;            /* PID of the faulting task */
    void*    instruction;    /* Address of the faulting instruction */
    uint32_t flags;          /* Additional flags */
} page_fault_info_t;

/**
 * Register a custom page fault handler for a memory region
 *
 * @param dir        Handle to the page directory
 * @param virt_addr  Virtual address of the region start
 * @param size       Size of the region (in bytes)
 * @param handler    Function pointer to the custom handler
 * @param context    User-defined context passed to the handler
 *
 * @return 0 on success, negative error code on failure
 *
 * This function registers a custom page fault handler for a specific memory
 * region. When a page fault occurs in this region, the custom handler is called
 * instead of the default handler.
 *
 * Custom handlers are useful for implementing specialized memory behavior,
 * such as memory-mapped files, guard pages, or virtual memory devices.
 *
 * The handler should return one of the PF_HANDLER_* flags to indicate how
 * the fault should be handled.
 *
 * Example:
 *   // Register a handler for a guard page
 *   int result = register_page_fault_handler(task_dir, guard_page, PAGE_SIZE_4K,
 *                                           guard_page_handler, NULL);
 *   if (result != 0) {
 *       // Handle error
 *   }
 */
int register_page_fault_handler(page_dir_t dir, void* virt_addr, size_t size,
                              int (*handler)(page_fault_info_t*, void*),
                              void* context);

/**
 * Unregister a custom page fault handler
 *
 * @param dir        Handle to the page directory
 * @param virt_addr  Virtual address of the region start
 *
 * @return 0 on success, negative error code on failure
 *
 * This function removes a previously registered custom page fault handler
 * for a memory region. After this call, the default page fault handler
 * will handle any faults in this region.
 *
 * Example:
 *   int result = unregister_page_fault_handler(task_dir, guard_page);
 *   if (result != 0) {
 *       // Handle error
 *   }
 */
int unregister_page_fault_handler(page_dir_t dir, void* virt_addr);

/*
 * TLB Management
 * -------------
 *
 * The Translation Lookaside Buffer (TLB) is a CPU cache that improves the speed
 * of virtual-to-physical address translation. When a page directory is modified,
 * the TLB must be updated to reflect the changes.
 *
 * EdgeX OS provides functions to explicitly manage the TLB when necessary,
 * though many operations (like page directory activation) automatically
 * handle TLB invalidation.
 */

/**
 * Invalidate a TLB entry for a specific virtual address
 *
 * @param virt_addr  Virtual address whose TLB entry should be invalidated
 *
 * This function invalidates the TLB entry for a specific virtual address
 * on the current CPU core. This ensures that the next access to this address
 * will use the most up-to-date page table entry.
 *
 * Example:
 *   // After changing page permissions
 *   invalidate_tlb_entry(page_addr);
 */
void invalidate_tlb_entry(void* virt_addr);

/**
 * Invalidate all TLB entries
 *
 * This function invalidates all TLB entries on the current CPU core,
 * forcing the CPU to reload all page table entries as needed.
 *
 * This function is typically called when making extensive changes to
 * a page directory.
 *
 * Example:
 *   // After modifying many page table entries
 *   invalidate_all_tlb_entries();
 */
void invalidate_all_tlb_entries(void);

/**
 * Send TLB invalidation request to all CPU cores
 *
 * @param virt_addr  Virtual address whose TLB entry should be invalidated
 *                   (NULL to invalidate all entries)
 *
 * This function sends an Inter-Processor Interrupt (IPI) to all CPU cores
 * requesting that they invalidate their TLB entry for the specified virtual
 * address, or all TLB entries if virt_addr is NULL.
 *
 * This function is necessary when changing page tables that may be in use
 * by multiple CPU cores.
 *
 * Example:
 *   // After changing a shared page mapping
 *   send_tlb_shootdown(shared_page);
 */
void send_tlb_shootdown(void* virt_addr);

/*
 * Memory Statistics and Debugging
 * ------------------------------
 *
 * EdgeX OS provides functions to gather statistics about memory usage
 * and to debug memory-related issues.
 */

/* Memory statistics structure */
typedef struct {
    size_t total_physical_memory;     /* Total physical memory in bytes */
    size_t free_physical_memory;      /* Free physical memory in bytes */
    size_t kernel_used_memory;        /* Memory used by the kernel in bytes */
    size_t user_used_memory;          /* Memory used by user tasks in bytes */
    size_t shared_memory;             /* Memory shared between multiple tasks */
    size_t page_tables_memory;        /* Memory used by page tables */
    size_t cached_memory;             /* Memory used for caching */
    size_t buffered_memory;           /* Memory used for buffers */
    size_t swapped_memory;            /* Memory swapped out to disk */
    uint32_t page_faults;             /* Number of page faults since boot */
    uint32_t tlb_invalidations;       /* Number of TLB invalidations since boot */
} memory_stats_t;

/**
 * Get memory statistics
 *
 * @param stats  Pointer to a memory_stats_t structure to fill
 *
 * @return 0 on success, negative error code on failure
 *
 * This function fills the provided structure with memory usage statistics
 * for the system.
 *
 * Example:
 *   memory_stats_t stats;
 *   if (get_memory_stats(&stats) == 0) {
 *       printf("Free memory: %zu bytes\n", stats.free_physical_memory);
 *   }
 */
int get_memory_stats(memory_stats_t* stats);

/**
 * Get memory usage for a page directory
 *
 * @param dir    Handle to the page directory
 * @param stats  Pointer to a memory_stats_t structure to fill
 *
 * @return 0 on success, negative error code on failure
 *
 * This function fills the provided structure with memory usage statistics
 * specific to the given page directory.
 *
 * Example:
 *   memory_stats_t task_stats;
 *   if (get_directory_memory_stats(task_dir, &task_stats) == 0) {
 *       printf("Task memory usage: %zu bytes\n", task_stats.user_used_memory);
 *   }
 */
int get_directory_memory_stats(page_dir_t dir, memory_stats_t* stats);

/**
 * Dump page directory information for debugging
 *
 * @param dir     Handle to the page directory
 * @param detail  Level of detail (0-3, where 3 is most detailed)
 *
 * This function dumps information about the page directory and its mappings
 * to the debug output for diagnostic purposes.
 *
 * Example:
 *   // Print detailed page directory information
 *   dump_page_directory(task_dir, 2);
 */
void dump_page_directory(page_dir_t dir, int detail);

/*
 * Large Page Support
 * ----------------
 *
 * EdgeX OS supports large pages (2MB and 1GB) to improve TLB efficiency
 * and reduce page table memory overhead. Large pages are particularly
 * useful for:
 *
 * 1. Memory-intensive applications that process large data sets
 * 2. Kernel code and data that are rarely paged out
 * 3. Applications with predictable memory access patterns
 *
 * To use large pages, specify PAGE_FLAG_HUGE when mapping memory:
 *
 *   // Map 2MB of memory using a single 2MB page
 *   void* buffer = map_anonymous_memory(task_dir, NULL, 2*1024*1024,
 *                                      PAGE_PERM_RW | PAGE_FLAG_HUGE,
 *                                      MAP_FLAG_HUGETLB);
 *
 * Large pages have several advantages:
 * - Reduced TLB misses, improving performance
 * - Decreased page table memory usage
 * - Lower overhead for page table management
 *
 * However, they also have drawbacks:
 * - Internal fragmentation if the full page size is not needed
 * - Less granular memory protection
 * - Potential for increased physical memory waste
 *
 * EdgeX OS automatically promotes or demotes page sizes when appropriate
 * to balance these trade-offs.
 */

/**
 * Set the page size policy for a page directory
 *
 * @param dir    Handle to the page directory
 * @param policy Page size policy (see PAGE_SIZE_POLICY_* constants)
 *
 * @return 0 on success, negative error code on failure
 *
 * This function sets the page size policy for the specified page directory,
 * controlling when the system uses 4KB, 2MB, or 1GB pages.
 *
 * Example:
 *   // Prefer large pages for performance
 *   set_page_size_policy(task_dir, PAGE_SIZE_POLICY_PREFER_HUGE);
 */
int set_page_size_policy(page_dir_t dir, uint32_t policy);
659
