/*
 * EdgeX OS - Shared Memory Implementation
 *
 * This file implements the shared memory system for EdgeX OS,
 * allowing tasks to share memory regions with controlled access.
 */

#include <edgex/kernel.h>
#include <edgex/memory.h>
#include <edgex/scheduler.h>
#include <edgex/ipc.h>
#include <edgex/vm.h>

/* Maximum number of shared memory regions in the system */
#define MAX_SHARED_MEMORY_REGIONS 64

/* Maximum number of mappings per shared memory region */
#define MAX_MAPPINGS_PER_REGION 16

/* Shared memory permission flags */
#define SHM_PERM_READ     0x01
#define SHM_PERM_WRITE    0x02
#define SHM_PERM_EXEC     0x04

/* Shared memory mapping flags */
#define SHM_MAP_FIXED     0x01  /* Map at specific address or fail */
#define SHM_MAP_PRIVATE   0x02  /* Changes are not visible to other processes */

/* Memory mapping information */
typedef struct shm_mapping {
    pid_t pid;                  /* Process ID using this mapping */
    void* virtual_addr;         /* Starting virtual address in process */
    uint32_t permissions;       /* Access permissions */
    uint32_t flags;             /* Mapping flags */
    bool is_active;             /* Whether this mapping is active */
} shm_mapping_t;

/* Shared memory region structure */
struct shared_memory {
    ipc_object_header_t header; /* Common IPC header */
    mutex_t mutex;              /* Access synchronization */
    
    size_t size;                /* Size of the region in bytes */
    size_t page_count;          /* Number of physical pages */
    physical_addr_t* pages;     /* Array of physical page addresses */
    
    pid_t creator;              /* Process that created the region */
    uint32_t default_perm;      /* Default permissions */
    
    uint32_t mapping_count;     /* Number of active mappings */
    shm_mapping_t mappings[MAX_MAPPINGS_PER_REGION]; /* Process mappings */
    
    /* Statistics */
    uint64_t access_count;      /* Number of times accessed */
    uint64_t creation_time;     /* Tick count when created */
};

/* Pool of all shared memory regions */
static struct shared_memory shm_pool[MAX_SHARED_MEMORY_REGIONS];
static uint32_t shm_count = 0;

/* Forward declarations */
static void destroy_shared_memory(void* shm_ptr);
static physical_addr_t* allocate_physical_pages(size_t count);
static void free_physical_pages(physical_addr_t* pages, size_t count);
static int add_mapping(shared_memory_t shm, pid_t pid, void* addr, uint32_t perm, uint32_t flags);
static int remove_mapping(shared_memory_t shm, pid_t pid);
static shm_mapping_t* find_mapping(shared_memory_t shm, pid_t pid);

/*
 * Find a free shared memory slot
 */
static shared_memory_t find_free_shared_memory(void) {
    for (uint32_t i = 0; i < MAX_SHARED_MEMORY_REGIONS; i++) {
        if (shm_pool[i].header.type == 0) {
            return &shm_pool[i];
        }
    }
    return NULL;
}

/*
 * Create a shared memory region
 */
shared_memory_t create_shared_memory(const char* name, size_t size, uint32_t permissions) {
    if (size == 0 || size > (1024 * 1024 * 1024)) {  // 1 GB max
        return NULL;
    }
    
    // Round up to page size
    size = ALIGN_UP(size, PAGE_SIZE);
    
    shared_memory_t shm = find_free_shared_memory();
    if (!shm) {
        kernel_printf("Failed to allocate shared memory: no free slots\n");
        return NULL;
    }
    
    // Initialize shared memory region
    shm->header.type = IPC_TYPE_SHARED_MEMORY;
    strncpy(shm->header.name, name, MAX_IPC_NAME_LENGTH - 1);
    shm->header.name[MAX_IPC_NAME_LENGTH - 1] = '\0';
    shm->header.ref_count = 1;
    shm->header.destroy_fn = destroy_shared_memory;
    
    // Create synchronization mutex
    char mutex_name[MAX_IPC_NAME_LENGTH];
    snprintf(mutex_name, MAX_IPC_NAME_LENGTH, "%s_mutex", name);
    shm->mutex = create_mutex(mutex_name);
    
    if (!shm->mutex) {
        memset(shm, 0, sizeof(struct shared_memory));
        return NULL;
    }
    
    // Calculate number of pages needed
    size_t page_count = size / PAGE_SIZE;
    
    // Allocate physical pages
    physical_addr_t* pages = allocate_physical_pages(page_count);
    if (!pages) {
        destroy_mutex(shm->mutex);
        memset(shm, 0, sizeof(struct shared_memory));
        return NULL;
    }
    
    shm->size = size;
    shm->page_count = page_count;
    shm->pages = pages;
    shm->creator = get_current_pid();
    shm->default_perm = permissions;
    shm->mapping_count = 0;
    
    // Clear the mappings array
    memset(shm->mappings, 0, sizeof(shm->mappings));
    
    // Initialize statistics
    shm->access_count = 0;
    shm->creation_time = get_tick_count();
    
    shm_count++;
    
    // Automatically map into the creator's address space
    void* mapped_addr = map_shared_memory(shm, NULL, permissions);
    if (!mapped_addr) {
        destroy_shared_memory(shm);
        return NULL;
    }
    
    return shm;
}

/*
 * Destroy a shared memory region
 */
static void destroy_shared_memory(void* shm_ptr) {
    shared_memory_t shm = (shared_memory_t)shm_ptr;
    
    if (!shm || shm->header.type != IPC_TYPE_SHARED_MEMORY) {
        return;
    }
    
    // Lock the shared memory
    mutex_lock(shm->mutex);
    
    // Unmap from all processes
    for (uint32_t i = 0; i < MAX_MAPPINGS_PER_REGION; i++) {
        if (shm->mappings[i].is_active) {
            pid_t pid = shm->mappings[i].pid;
            void* addr = shm->mappings[i].virtual_addr;
            
            // Unmap from the process's address space
            // This would call into the VM system
            vm_unmap_pages(pid, addr, shm->page_count);
            
            shm->mappings[i].is_active = false;
        }
    }
    
    // Free the physical pages
    free_physical_pages(shm->pages, shm->page_count);
    
    // Unlock and destroy mutex
    mutex_unlock(shm->mutex);
    destroy_mutex(shm->mutex);
    
    // Clear the shared memory
    memset(shm, 0, sizeof(struct shared_memory));
    
    shm_count--;
}

/*
 * Destroy a shared memory region (public API)
 */
void destroy_shared_memory(shared_memory_t shm) {
    if (!shm || shm->header.type != IPC_TYPE_SHARED_MEMORY) {
        return;
    }
    
    // Only the creator can destroy the region
    if (shm->creator != get_current_pid()) {
        return;
    }
    
    destroy_shared_memory((void*)shm);
}

/*
 * Map a shared memory region into a process's address space
 */
void* map_shared_memory(shared_memory_t shm, void* addr, uint32_t permissions) {
    if (!shm || shm->header.type != IPC_TYPE_SHARED_MEMORY) {
        return NULL;
    }
    
    pid_t pid = get_current_pid();
    
    // Lock the shared memory
    mutex_lock(shm->mutex);
    
    // Check if already mapped
    shm_mapping_t* existing = find_mapping(shm, pid);
    if (existing) {
        // Already mapped
        void* mapped_addr = existing->virtual_addr;
        mutex_unlock(shm->mutex);
        return mapped_addr;
    }
    
    // Check if we've reached the maximum number of mappings
    if (shm->mapping_count >= MAX_MAPPINGS_PER_REGION) {
        mutex_unlock(shm->mutex);
        return NULL;
    }
    
    // Validate permissions - can't exceed what the memory allows
    permissions &= shm->default_perm;
    
    // Call into VM system to map the pages into the process's address space
    uint32_t flags = addr ? SHM_MAP_FIXED : 0;
    void* mapped_addr = vm_map_physical_pages(pid, addr, shm->pages, shm->page_count, permissions);
    
    if (!mapped_addr) {
        mutex_unlock(shm->mutex);
        return NULL;
    }
    
    // Add the mapping to our tracking
    if (add_mapping(shm, pid, mapped_addr, permissions, flags) != 0) {
        // Failed to add mapping, unmap the memory
        vm_unmap_pages(pid, mapped_addr, shm->page_count);
        mutex_unlock(shm->mutex);
        return NULL;
    }
    
    // Increment access count
    shm->access_count++;
    
    mutex_unlock(shm->mutex);
    return mapped_addr;
}

/*
 * Unmap a shared memory region from a process's address space
 */
int unmap_shared_memory(shared_memory_t shm) {
    if (!shm || shm->header.type != IPC_TYPE_SHARED_MEMORY) {
        return -1;
    }
    
    pid_t pid = get_current_pid();
    
    // Lock the shared memory
    mutex_lock(shm->mutex);
    
    // Find the mapping
    shm_mapping_t* mapping = find_mapping(shm, pid);
    if (!mapping) {
        // Not mapped
        mutex_unlock(shm->mutex);
        return -1;
    }
    
    // Call into VM system to unmap the pages
    int result = vm_unmap_pages(pid, mapping->virtual_addr, shm->page_count);
    
    // Remove the mapping from our tracking
    remove_mapping(shm, pid);
    
    mutex_unlock(shm->mutex);
    return result;
}

/*
 * Resize a shared memory region
 */
int resize_shared_memory(shared_memory_t shm, size_t new_size) {
    if (!shm || shm->header.type != IPC_TYPE_SHARED_MEMORY) {
        return -1;
    }
    
    // Only the creator can resize the region
    if (shm->creator != get_current_pid()) {
        return -1;
    }
    
    // Round up to page size
    new_size = ALIGN_UP(new_size, PAGE_SIZE);
    
    // Lock the shared memory
    mutex_lock(shm->mutex);
    
    // Calculate new page count
    size_t new_page_count = new_size / PAGE_SIZE;
    
    if (new_page_count == shm->page_count) {
        // No change needed
        mutex_unlock(shm->mutex);
        return 0;
    }
    
    if (new_page_count > shm->page_count) {
        // Growing the region
        size_t additional_pages = new_page_count - shm->page_count;
        
        // Allocate additional pages
        physical_addr_t* new_pages = allocate_physical_pages(additional_pages);
        if (!new_pages) {
            mutex_unlock(shm->mutex);
            return -1;
        }
        
        // Create a new combined array
        physical_addr_t* combined_pages = (physical_addr_t*)kmalloc(new_page_count * sizeof(physical_addr_t));
        if (!combined_pages) {
            free_physical_pages(new_pages, additional_pages);
            mutex_unlock(shm->mutex);
            return -1;
        }
        
        // Copy old and new pages into the combined array
        memcpy(combined_pages, shm->pages, shm->page_count * sizeof(physical_addr_t));
        memcpy(combined_pages + shm->page_count, new_pages, additional_pages * sizeof(physical_addr_t));
        
        // Free the old array and new pages array (but not the pages themselves)
        kfree(shm->pages);
        kfree(new_pages);
        
        // Update with the new combined array
        shm->pages = combined_pages;
        
        // For each active mapping, expand the mapping in the process's address space
        for (uint32_t i = 0; i < MAX_MAPPINGS_PER_REGION; i++) {
            if (shm->mappings[i].is_active) {
                pid_t pid = shm->mappings[i].pid;
                void* addr = shm->mappings[i].virtual_addr;
                uint32_t perm = shm->mappings[i].permissions;
                
                // Expand the mapping in the process's address space
                // In a real implementation, this would be more complex
                // For example, you might need to move the entire region if the pages after it are already mapped
                vm_expand_mapping(pid, addr, combined_pages + shm->page_count, additional_pages, perm);
            }
        }
    } else {
        // Shrinking the region
        // For each active mapping, contract the mapping in the process's address space
        for (uint32_t i = 0; i < MAX_MAPPINGS_PER_REGION; i++) {
            if (shm->mappings[i].is_active) {
                pid_t pid = shm->mappings[i].pid;
                void* addr = shm->mappings[i].virtual_addr;
                
                // Calculate the new end address
                void* new_end = (void*)((uintptr_t)addr + new_size);
                void* old_end = (void*)((uintptr_t)addr + shm->size);
                
                // Unmap the portion of the region beyond the new size
                size_t pages_to_unmap = shm->page_count - new_page_count;
                vm_unmap_pages(pid, new_end, pages_to_unmap);
            }
        }
        
        // Free the excess physical pages
        size_t pages_to_free = shm->page_count - new_page_count;
        free_physical_pages(shm->pages + new_page_count, pages_to_free);
        
        // Create a new smaller array
        physical_addr_t* smaller_pages = (physical_addr_t*)kmalloc(new_page_count * sizeof(physical_addr_t));
        if (!smaller_pages) {
            // Failed to allocate, but we've already unmapped and freed pages
            // This is a partial failure that's hard to recover from
            mutex_unlock(shm->mutex);
            return -1;
        }
        
        // Copy the remaining pages
        memcpy(smaller_pages, shm->pages, new_page_count * sizeof(physical_addr_t));
        
        // Free the old array
        kfree(shm->pages);
        
        // Update with the new smaller array
        shm->pages = smaller_pages;
    }
    
    // Update size and page count
    shm->size = new_size;
    shm->page_count = new_page_count;
    
    mutex_unlock(shm->mutex);
    return 0;
}

/*
 * Allocate physical pages for a shared memory region
 */
static physical_addr_t* allocate_physical_pages(size_t count) {
    if (count == 0) {
        return NULL;
    }
    
    // Allocate an array to hold the physical addresses
    physical_addr_t* pages = (physical_addr_t*)kmalloc(count * sizeof(physical_addr_t));
    if (!pages) {
        return NULL;
    }
    
    // Allocate each physical page
    for (size_t i = 0; i < count; i++) {
        // Call into the physical memory manager to allocate a page
        pages[i] = allocate_physical_page();
        
        if (pages[i] == 0) {
            // Failed to allocate, free all previous pages
            for (size_t j = 0; j < i; j++) {
                free_physical_page(pages[j]);
            }
            
            kfree(pages);
            return NULL;
        }
        
        // Zero out the physical page for security
        void* mapped_temp = map_physical_page(pages[i]);
        memset(mapped_temp, 0, PAGE_SIZE);
        unmap_physical_page(mapped_temp);
    }
    
    return pages;
}

/*
 * Free physical pages used by a shared memory region
 */
static void free_physical_pages(physical_addr_t* pages, size_t count) {
    if (!pages || count == 0) {
        return;
    }
    
    // Free each physical page
    for (size_t i = 0; i < count; i++) {
        if (pages[i] != 0) {
            free_physical_page(pages[i]);
        }
    }
}

/*
 * Find a mapping for a process
 */
static shm_mapping_t* find_mapping(shared_memory_t shm, pid_t pid) {
    if (!shm) {
        return NULL;
    }
    
    for (uint32_t i = 0; i < MAX_MAPPINGS_PER_REGION; i++) {
        if (shm->mappings[i].is_active && shm->mappings[i].pid == pid) {
            return &shm->mappings[i];
        }
    }
    
    return NULL;
}

/*
 * Add a mapping for a process
 */
static int add_mapping(shared_memory_t shm, pid_t pid, void* addr, uint32_t perm, uint32_t flags) {
    if (!shm) {
        return -1;
    }
    
    // Find a free mapping slot
    for (uint32_t i = 0; i < MAX_MAPPINGS_PER_REGION; i++) {
        if (!shm->mappings[i].is_active) {
            // Found a free slot
            shm->mappings[i].pid = pid;
            shm->mappings[i].virtual_addr = addr;
            shm->mappings[i].permissions = perm;
            shm->mappings[i].flags = flags;
            shm->mappings[i].is_active = true;
            
            shm->mapping_count++;
            return 0;
        }
    }
    
    // No free slots
    return -1;
}

/*
 * Remove a mapping for a process
 */
static int remove_mapping(shared_memory_t shm, pid_t pid) {
    if (!shm) {
        return -1;
    }
    
    for (uint32_t i = 0; i < MAX_MAPPINGS_PER_REGION; i++) {
        if (shm->mappings[i].is_active && shm->mappings[i].pid == pid) {
            // Found the mapping
            shm->mappings[i].is_active = false;
            shm->mapping_count--;
            return 0;
        }
    }
    
    // Mapping not found
    return -1;
}

/*
 * Clean up shared memory for a terminated task
 */
void cleanup_task_shared_memory(pid_t pid) {
    // Go through all shared memory regions
    for (uint32_t i = 0; i < MAX_SHARED_MEMORY_REGIONS; i++) {
        shared_memory_t shm = &shm_pool[i];
        
        if (shm->header.type != IPC_TYPE_SHARED_MEMORY) {
            continue;
        }
        
        // Lock the shared memory
        mutex_lock(shm->mutex);
        
        // Check if this task has a mapping
        shm_mapping_t* mapping = find_mapping(shm, pid);
        if (mapping) {
            // Unmap from the process's address space
            vm_unmap_pages(pid, mapping->virtual_addr, shm->page_count);
            
            // Remove the mapping
            remove_mapping(shm, pid);
        }
        
        // If this task was the creator and no other mappings exist, destroy the region
        if (shm->creator == pid && shm->mapping_count == 0) {
            // Unlock before destroying
            mutex_unlock(shm->mutex);
            destroy_shared_memory(shm);
            continue;
        }
        
        mutex_unlock(shm->mutex);
    }
}

/*
 * Get information about a shared memory region
 */
int get_shared_memory_info(shared_memory_t shm, struct shared_memory_info* info) {
    if (!shm || !info || shm->header.type != IPC_TYPE_SHARED_MEMORY) {
        return -1;
    }
    
    // Lock the shared memory
    mutex_lock(shm->mutex);
    
    // Fill in basic info
    info->size = shm->size;
    info->creator = shm->creator;
    info->mapping_count = shm->mapping_count;
    info->default_permissions = shm->default_perm;
    info->access_count = shm->access_count;
    info->creation_time = shm->creation_time;
    strncpy(info->name, shm->header.name, MAX_IPC_NAME_LENGTH);
    
    mutex_unlock(shm->mutex);
    return 0;
}

/*
 * Find a shared memory region by name
 */
shared_memory_t find_shared_memory(const char* name) {
    if (!name) {
        return NULL;
    }
    
    for (uint32_t i = 0; i < MAX_SHARED_MEMORY_REGIONS; i++) {
        shared_memory_t shm = &shm_pool[i];
        
        if (shm->header.type == IPC_TYPE_SHARED_MEMORY && 
            strcmp(shm->header.name, name) == 0) {
            return shm;
        }
    }
    
    return NULL;
}

/*
 * Initialize the shared memory system
 */
void init_shared_memory_system(void) {
    kernel_printf("Initializing shared memory system...\n");
    
    // Initialize shared memory pool
    memset(shm_pool, 0, sizeof(shm_pool));
    shm_count = 0;
    
    // Create initial kernel shared memory regions
    shared_memory_t kernel_shm = create_shared_memory(
        "kernel_shared", 
        PAGE_SIZE * 4, 
        SHM_PERM_READ | SHM_PERM_WRITE
    );
    
    if (!kernel_shm) {
        kernel_panic("Failed to create kernel shared memory");
    }
    
    // Create a read-only shared memory region for system configuration
    shared_memory_t config_shm = create_shared_memory(
        "system_config", 
        PAGE_SIZE * 2, 
        SHM_PERM_READ
    );
    
    if (!config_shm) {
        kernel_panic("Failed to create system config shared memory");
    }
    
    kernel_printf("Shared memory system initialized with %d regions\n", shm_count);
}

/*
 * Initialize the shared memory subsystem and register with scheduler
 * Called during kernel startup
 */
void init_shared_memory_subsystem(void) {
    // Initialize the shared memory system
    init_shared_memory_system();
    
    // Register with the task manager to clean up shared memory on task termination
    // This would be handled by the scheduler subsystem
    // register_task_cleanup_handler(cleanup_task_shared_memory);
}
                size_t pages_to_unmap = shm->page_count - new_page_count;
                vm_unmap_pages(pid, new_end, pages_to_unmap);
            }
        }
        
        // Free the excess physical pages
        size_t pages_to_free = shm->page_count - new_page_count;
        free_physical_pages(shm->pages + new_page_count, pages_to_free);
        
        // Create a new smaller array
        physical_addr_t* smaller_pages = (physical_addr_t*)kmalloc(new_page_count * sizeof(physical_addr_t));
        if (!smaller_pages) {
            // Failed to allocate, but we've already unmapped and freed pages
            // This is a partial failure that's hard to recover from
            // In a real implementation, you'd have more robust error handling
            mutex_unlock(shm->mutex);
            return -1;
        }
        
        // Copy the remaining pages
        memcpy(smaller_pages, shm->pages, new_page_count * sizeof(physical_addr_t));
        
        // Free the old array
        kfree(shm->pages);
        
        // Update with the new smaller array
        shm->pages = smaller_pages;
    }
    
    // Update size and page count
    shm->size = new_size;
    shm->page_count = new_page_count;
    
    mutex_unlock(shm->mutex);
    return 0;
}

/*
 * Allocate physical pages for a shared memory region
 */
static physical_addr_t* allocate_physical_pages(size_t count) {
    if (count == 0) {
        return NULL;
    }
    
    // Allocate an array to hold the physical addresses
    physical_addr_t* pages = (physical_addr_t*)kmalloc(count * sizeof(physical_addr_t));
    if (!pages) {
        return NULL;
    }
    
    // Allocate each physical page
    for (size_t i = 0; i < count; i++) {
        // Call into the physical memory manager to allocate a page
        // This is a placeholder and would call into the actual physical memory allocator
        pages[i] = allocate_physical_page();
        
        if (pages[i] == 0) {
            // Failed to allocate, free all previous pages
            for (size_t j = 0; j < i; j++) {
                free_physical_page(pages[j]);
            }
            
            kfree(pages);
            return NULL;
        }
        
        // Zero out the physical page
        void* mapped_temp = map_physical_page(pages[i]);
        memset(mapped_temp, 0, PAGE_SIZE);
        unmap_physical_page(mapped_temp);
    }
    
    return pages;
}

/*
 * Free physical pages used by a shared memory region
 */
static void free_physical_pages(physical_addr_t* pages, size_t count) {
    if (!pages || count == 0) {
        return;
    }
    
    // Free each physical page
    for (size_t i = 0; i < count; i++) {
        // Call into the physical memory manager to free the page
        free_physical_page(pages[i]);
    }
    
    // Don't free the array here as it's typically handled by the caller
}

/*
 * Find a mapping for a process
 */
static shm_mapping_t* find_mapping(shared_memory_t shm, pid_t pid) {
    if (!shm) {
        return NULL;
    }
    
    for (uint32_t i = 0; i < MAX_MAPPINGS_PER_REGION; i++) {
        if (shm->mappings[i].is_active && shm->mappings[i].pid == pid) {
            return &shm->mappings[i];
        }
    }
    
    return NULL;
}

/*
 * Add a mapping for a process
 */
static int add_mapping(shared_memory_t shm, pid_t pid, void* addr, uint32_t perm, uint32_t flags) {
    if (!shm) {
        return -1;
    }
    
    // Find a free mapping slot
    for (uint32_t i = 0; i < MAX_MAPPINGS_PER_REGION; i++) {
        if (!shm->mappings[i].is_active) {
            // Found a free slot
            shm->mappings[i].pid = pid;
            shm->mappings[i].virtual_addr = addr;
            shm->mappings[i].permissions = perm;
            shm->mappings[i].flags = flags;
            shm->mappings[i].is_active = true;
            
            shm->mapping_count++;
            return 0;
        }
    }
    
    // No free slots
    return -1;
}

/*
 * Remove a mapping for a process
 */
static int remove_mapping(shared_memory_t shm, pid_t pid) {
    if (!shm) {
        return -1;
    }
    
    for (uint32_t i = 0; i < MAX_MAPPINGS_PER_REGION; i++) {
        if (shm->mappings[i].is_active && shm->mappings[i].pid == pid) {
            // Found the mapping
            shm->mappings[i].is_active = false;
            shm->mapping_count--;
            return 0;
        }
    }
    
    // Mapping not found
    return -1;
}

/*
 * Clean up shared memory for a terminated task
 */
void cleanup_task_shared_memory(pid_t pid) {
    // Go through all shared memory regions
    for (uint32_t i = 0; i < MAX_SHARED_MEMORY_REGIONS; i++) {
        shared_memory_t shm = &shm_pool[i];
        
        if (shm->header.type != IPC_TYPE_SHARED_MEMORY) {
            continue;
        }
        
        // Lock the shared memory
        mutex_lock(shm->mutex);
        
        // Check if this task has a mapping
        shm_mapping_t* mapping = find_mapping(shm, pid);
        if (mapping) {
            // Unmap from the process's address space
            // This would call into the VM system
            vm_unmap_pages(pid, mapping->virtual_addr, shm->page_count);
            
            // Remove the mapping
            remove_mapping(shm, pid);
        }
        
        // If this task was the creator and no other mappings exist, destroy the region
        if (shm->creator == pid && shm->mapping_count == 0) {
            // Unlock before destroying
            mutex_unlock(shm->mutex);
            destroy_shared_memory(shm);
            continue;
        }
        
        mutex_unlock(shm->mutex);
    }
}

/*
 * Initialize the shared memory system
 */
void init_shared_memory_system(void) {
    kernel_printf("Initializing shared memory system...\n");
    
    // Initialize shared memory pool
    memset(shm_pool, 0, sizeof(shm_pool));
    shm_count = 0;
    
    // Create initial kernel shared memory region
    shared_memory_t kernel_shm = create_shared_memory("kernel_shared", 
                                                     PAGE_SIZE * 4, 
                                                     SHM_PERM_READ | SHM_PERM_WRITE);
    if (!kernel_shm) {
        kernel_panic("Failed to create kernel shared memory");
    }
    
    kernel_printf("Shared memory system initialized: %d regions\n", shm_count);
}

/*
 * Initialize the shared memory subsystem
 * Called during kernel startup
 */
void init_shared_memory_subsystem(void) {
    // Initialize the shared memory system
    init_shared_memory_system();
    
    // Register cleanup handler with the task manager
    // When a task terminates, this will be called
    // register_task_cleanup_handler(cleanup_task_shared_memory);
}
