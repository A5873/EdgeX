/*
 * Unmap a shared memory segment from the current task's address space
 *
 * addr: Virtual address of the mapped segment
 * size: Size of the mapped region (must match the original mapping size)
 *
 * Returns: 0 on success, negative error code on failure
 */
int unmap_shared_memory(void* addr, size_t size) {
    shared_memory_segment_t* segment;
    pid_t current_task_id;
    int result;
    
    if (!shm_initialized || addr == NULL || size == 0) {
        return -EINVAL;
    }
    
    /* Get current task ID */
    current_task_id = get_current_pid();
    
    /* Acquire global lock to find the segment */
    mutex_lock(&shm_global_lock);
    
    /* Find the segment that contains this mapping */
    segment = find_segment_by_address(addr, current_task_id);
    if (segment == NULL) {
        mutex_unlock(&shm_global_lock);
        LOG_ERROR("No shared memory segment found at address %p for task %d", 
                  addr, current_task_id);
        return -EINVAL;
    }
    
    /* Switch to segment lock */
    mutex_lock(&segment->lock);
    mutex_unlock(&shm_global_lock);
    
    /* Remove the mapping */
    result = remove_mapping(segment, current_task_id);
    if (result != 0) {
        mutex_unlock(&segment->lock);
        LOG_ERROR("Failed to remove mapping for task %d in segment '%s'", 
                 current_task_id, segment->name);
        return result;
    }
    
    /* Unmap the pages */
    result = unmap_pages((uint64_t)addr, size);
    if (result != 0) {
        mutex_unlock(&segment->lock);
        LOG_ERROR("Failed to unmap pages at %p for task %d", addr, current_task_id);
        return result;
    }
    
    /* Flush TLB for the unmapped pages */
    flush_tlb_range((uint64_t)addr, size);
    
    LOG_INFO("Unmapped shared memory segment '%s' for task %d from %p", 
             segment->name, current_task_id, addr);
    
    mutex_unlock(&segment->lock);
    
    return 0;
}

/*
 * Internal function to unmap shared memory for a task
 * Note: segment->lock must be held when calling this function
 */
static void unmap_shared_memory_internal(shared_memory_segment_t* segment, pid_t task_id) {
    for (uint32_t i = 0; i < segment->mapping_count; i++) {
        if (segment->mappings[i].task_id == task_id) {
            void* virtual_addr = segment->mappings[i].virtual_addr;
            uint32_t size = segment->mappings[i].size;
            
            /* Remove mapping from the task's page tables */
            unmap_pages((uint64_t)virtual_addr, size);
            
            /* Remove from mapping table */
            if (i < segment->mapping_count - 1) {
                /* Shift remaining entries */
                memmove(&segment->mappings[i], &segment->mappings[i + 1], 
                       (segment->mapping_count - i - 1) * sizeof(shm_mapping_t));
            }
            segment->mapping_count--;
            
            LOG_INFO("Internally unmapped shared memory segment '%s' for task %d from %p", 
                    segment->name, task_id, virtual_addr);
            
            /* Only remove the first instance (there should be only one per task) */
            break;
        }
    }
}

/*
 * Resize a shared memory segment
 *
 * shm: Handle to the shared memory segment
 * new_size: New size for the segment
 *
 * Returns: 0 on success, negative error code on failure
 */
int resize_shared_memory(void* shm, size_t new_size) {
    shared_memory_segment_t* segment = (shared_memory_segment_t*)shm;
    void* new_physical;
    uint32_t new_real_size;
    int result = 0;
    
    if (!shm_initialized || segment == NULL || new_size == 0) {
        return -EINVAL;
    }
    
    /* Acquire segment lock */
    mutex_lock(&segment->lock);
    
    /* Check if resize is allowed */
    if (!(segment->flags & SHM_FLAG_RESIZE)) {
        mutex_unlock(&segment->lock);
        LOG_ERROR("Resize not allowed for segment '%s'", segment->name);
        return -EPERM;
    }
    
    /* Calculate page-aligned sizes */
    new_real_size = (new_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    
    /* Check if we need to actually resize */
    if (new_real_size == segment->real_size) {
        /* Just update the logical size */
        segment->size = new_size;
        mutex_unlock(&segment->lock);
        return 0;
    }
    
    /* Allocate new physical memory */
    new_physical = alloc_pages(new_real_size / PAGE_SIZE, ALLOC_ZERO | ALLOC_KERNEL);
    if (new_physical == NULL) {
        mutex_unlock(&segment->lock);
        LOG_ERROR("Failed to allocate memory for resizing shared segment '%s'", segment->name);
        return -ENOMEM;
    }
    
    /* Copy data from old to new memory */
    if (new_real_size > segment->real_size) {
        /* Growing: copy all old data */
        memcpy(new_physical, segment->physical_addr, segment->real_size);
    } else {
        /* Shrinking: copy only what fits */
        memcpy(new_physical, segment->physical_addr, new_real_size);
    }
    
    /* For each mapping, remap to the new physical memory */
    for (uint32_t i = 0; i < segment->mapping_count; i++) {
        pid_t task_id = segment->mappings[i].task_id;
        void* virtual_addr = segment->mappings[i].virtual_addr;
        uint32_t perm = segment->mappings[i].permissions;
        
        /* Build page flags */
        uint64_t page_flags = PAGE_FLAG_SHARED;
        if (perm & SHM_PERM_READ)
            page_flags |= PAGE_FLAG_READ;
        if (perm & SHM_PERM_WRITE)
            page_flags |= PAGE_FLAG_WRITE;
        if (perm & SHM_PERM_EXEC)
            page_flags |= PAGE_FLAG_EXEC;
        
        /* Unmap old pages */
        unmap_pages((uint64_t)virtual_addr, segment->real_size);
        
        /* Map new pages */
        result = map_pages((uint64_t)virtual_addr, (uint64_t)new_physical, 
                          new_real_size, page_flags);
        if (result != 0) {
            LOG_ERROR("Failed to remap shared memory for task %d during resize", task_id);
            /* Continue attempting to remap for other tasks */
        }
        
        /* Update mapping size */
        segment->mappings[i].size = new_size;
        
        /* Flush TLB */
        flush_tlb_range((uint64_t)virtual_addr, (new_real_size > segment->real_size) ? 
                       new_real_size : segment->real_size);
    }
    
    /* Free old physical memory */
    free_pages(segment->physical_addr, segment->real_size / PAGE_SIZE);
    
    /* Update segment */
    segment->physical_addr = new_physical;
    segment->size = new_size;
    segment->real_size = new_real_size;
    
    LOG_INFO("Resized shared memory segment '%s' to %u bytes", segment->name, new_size);
    
    mutex_unlock(&segment->lock);
    
    return 0;
}

/*
 * Cleanup shared memory resources for a terminated task
 *
 * task_id: ID of the task being terminated
 */
void cleanup_task_shared_memory(pid_t task_id) {
    shared_memory_segment_t* segment;
    shared_memory_segment_t* next;
    
    if (!shm_initialized || task_id == PID_INVALID) {
        return;
    }
    
    LOG_INFO("Cleaning up shared memory for terminated task %d", task_id);
    
    /* Acquire global lock */
    mutex_lock(&shm_global_lock);
    
    /* Scan all segments for mappings by this task */
    segment = shm_segments;
    while (segment != NULL) {
        next = segment->next;  /* Save next pointer in case this segment gets destroyed */
        
        mutex_lock(&segment->lock);
        
        /* Check if this task has any mappings in this segment */
        for (uint32_t i = 0; i < segment->mapping_count; i++) {
            if (segment->mappings[i].task_id == task_id) {
                /* Found a mapping for this task */
                void* virtual_addr = segment->mappings[i].virtual_addr;
                
                /* Remove mapping from the task's page tables */
                unmap_pages((uint64_t)virtual_addr, segment->real_size);
                
                /* Remove from mapping table */
                if (i < segment->mapping_count - 1) {
                    /* Shift remaining entries */
                    memmove(&segment->mappings[i], &segment->mappings[i + 1], 
                           (segment->mapping_count - i - 1) * sizeof(shm_mapping_t));
                }
                segment->mapping_count--;
                
                /* Adjust index since we shifted the array */
                i--;
                
                /* Decrement reference count for the segment */
                segment->ref_count--;
                
                LOG_INFO("Removed shared memory mapping for task %d in segment '%s'", 
                        task_id, segment->name);
            }
        }
        
        /* Check if the segment should be destroyed */
        if (segment->ref_count == 0) {
            /* Add code to destroy the segment here if needed */
            /* This would involve removing it from the linked list and freeing resources */
            /* For now, we'll handle this through the normal destroy_shared_memory path */
        }
        
        mutex_unlock(&segment->lock);
        segment = next;
    }
    
    mutex_unlock(&shm_global_lock);
}

/*
 * Helper function to add a mapping to a segment
 * Note: segment->lock must be held when calling this function
 */
static int add_mapping(shared_memory_segment_t* segment, pid_t task_id, 
                      void* virtual_addr, uint32_t size, uint32_t permissions) {
    /* Check if we have room for another mapping */
    if (segment->mapping_count >= MAX_SHM_MAPPINGS) {
        LOG_ERROR("Maximum mappings reached for segment '%s'", segment->name);
        return -ENOMEM;
    }
    
    /* Add the mapping */
    segment->mappings[segment->mapping_count].task_id = task_id;
    segment->mappings[segment->mapping_count].virtual_addr = virtual_addr;
    segment->mappings[segment->mapping_count].size = size;
    segment->mappings[segment->mapping_count].permissions = permissions;
    segment->mapping_count++;
    
    return 0;
}

/*
 * Helper function to remove a mapping from a segment
 * Note: segment->lock must be held when calling this function
 */
static int remove_mapping(shared_memory_segment_t* segment, pid_t task_id) {
    for (uint32_t i = 0; i < segment->mapping_count; i++) {
        if (segment->mappings[i].task_id == task_id) {
            /* Found the mapping, remove it */
            if (i < segment->mapping_count - 1

/*
 * EdgeX OS - Shared Memory Implementation
 *
 * This file implements shared memory functionality for EdgeX OS,
 * providing the ability for tasks to share memory regions with proper
 * permissions and reference counting.
 */

#include <edgex/memory.h>
#include <edgex/scheduler.h>
#include <edgex/ipc/mutex.h>
#include <edgex/kernel.h>
#include <string.h>

/* Shared memory permissions */
#define SHM_PERM_READ    (1 << 0)  /* Read permission */
#define SHM_PERM_WRITE   (1 << 1)  /* Write permission */
#define SHM_PERM_EXEC    (1 << 2)  /* Execute permission */

/* Shared memory flags */
#define SHM_FLAG_CREATE  (1 << 0)  /* Create the segment if it doesn't exist */
#define SHM_FLAG_EXCL    (1 << 1)  /* Fail if segment already exists */
#define SHM_FLAG_RESIZE  (1 << 2)  /* Allow segment to be resized */

/* Maximum number of shared memory segments */
#define MAX_SHARED_MEMORY_SEGMENTS 128

/* Maximum length of shared memory region name */
#define SHM_NAME_MAX 64

/* Shared memory reference counting */
typedef struct {
    pid_t task_id;                /* Task ID that has this mapping */
    void* virtual_addr;           /* Virtual address for this mapping */
    uint32_t size;                /* Size of mapping for this task */
    uint32_t permissions;         /* Permissions for this mapping */
} shm_mapping_t;

/* Maximum mappings per segment */
#define MAX_SHM_MAPPINGS 32

/* Shared memory segment structure */
typedef struct {
    char name[SHM_NAME_MAX];      /* Name of the shared memory segment */
    void* physical_addr;          /* Physical address of the segment */
    uint32_t size;                /* Size of the segment in bytes */
    uint32_t real_size;           /* Actual allocated size (page-aligned) */
    uint32_t permissions;         /* Default permissions */
    uint32_t flags;               /* Segment flags */
    uint32_t ref_count;           /* Reference count */
    
    /* Mappings tracking */
    shm_mapping_t mappings[MAX_SHM_MAPPINGS];
    uint32_t mapping_count;
    
    /* Mutex for thread safety */
    mutex_t lock;
    
    /* For linked list */
    struct shared_memory_segment* next;
} shared_memory_segment_t;

/* Global shared memory state */
static mutex_t shm_global_lock;
static shared_memory_segment_t* shm_segments = NULL;
static uint32_t shm_segment_count = 0;
static bool shm_initialized = false;

/* Forward declarations */
static void unmap_shared_memory_internal(shared_memory_segment_t* segment, pid_t task_id);
static shared_memory_segment_t* find_segment_by_name(const char* name);
static shared_memory_segment_t* find_segment_by_address(void* addr, pid_t task_id);
static int add_mapping(shared_memory_segment_t* segment, pid_t task_id, 
                      void* virtual_addr, uint32_t size, uint32_t permissions);
static int remove_mapping(shared_memory_segment_t* segment, pid_t task_id);

/* Initialize shared memory subsystem */
void init_shared_memory(void) {
    if (shm_initialized) {
        return;
    }
    
    mutex_init(&shm_global_lock);
    shm_segments = NULL;
    shm_segment_count = 0;
    shm_initialized = true;
    
    LOG_INFO("Shared memory subsystem initialized");
}

/* 
 * Create a new shared memory segment
 * 
 * name: Name of the shared memory segment
 * size: Size of the segment in bytes
 * permissions: Default permissions for the segment
 * flags: Creation flags
 * 
 * Returns: Handle to the shared memory segment or NULL on error
 */
void* create_shared_memory(const char* name, size_t size, uint32_t permissions, uint32_t flags) {
    shared_memory_segment_t* segment;
    void* physical_memory;
    bool created = false;
    
    if (!shm_initialized) {
        init_shared_memory();
    }
    
    /* Validate parameters */
    if (name == NULL || strlen(name) == 0 || strlen(name) >= SHM_NAME_MAX || size == 0) {
        LOG_ERROR("Invalid shared memory parameters");
        return NULL;
    }
    
    /* Acquire global lock */
    mutex_lock(&shm_global_lock);
    
    /* Check if we've hit the maximum segment count */
    if (shm_segment_count >= MAX_SHARED_MEMORY_SEGMENTS) {
        LOG_ERROR("Maximum shared memory segment count reached");
        mutex_unlock(&shm_global_lock);
        return NULL;
    }
    
    /* Check if segment already exists */
    segment = find_segment_by_name(name);
    if (segment != NULL) {
        if (flags & SHM_FLAG_EXCL) {
            /* Exclusive create requested, but segment exists */
            LOG_ERROR("Shared memory segment '%s' already exists", name);
            mutex_unlock(&shm_global_lock);
            return NULL;
        }
        
        /* Segment exists, check if resize is allowed */
        mutex_lock(&segment->lock);
        
        if (segment->size < size && !(segment->flags & SHM_FLAG_RESIZE)) {
            LOG_ERROR("Shared memory segment '%s' exists but is too small and resize not allowed", name);
            mutex_unlock(&segment->lock);
            mutex_unlock(&shm_global_lock);
            return NULL;
        }
        
        /* If resize is needed and allowed */
        if (segment->size < size && (segment->flags & SHM_FLAG_RESIZE)) {
            /* Allocate larger physical memory */
            uint32_t page_aligned_size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
            void* new_physical = alloc_pages(page_aligned_size / PAGE_SIZE, ALLOC_ZERO | ALLOC_KERNEL);
            
            if (new_physical == NULL) {
                LOG_ERROR("Failed to allocate memory for resizing shared segment '%s'", name);
                mutex_unlock(&segment->lock);
                mutex_unlock(&shm_global_lock);
                return NULL;
            }
            
            /* Copy old content to new memory */
            memcpy(new_physical, segment->physical_addr, segment->real_size);
            
            /* Free old memory */
            free_pages(segment->physical_addr, segment->real_size / PAGE_SIZE);
            
            /* Update segment */
            segment->physical_addr = new_physical;
            segment->size = size;
            segment->real_size = page_aligned_size;
            
            LOG_INFO("Resized shared memory segment '%s' from %u to %u bytes", 
                    name, segment->size, size);
        }
        
        /* Update permissions if specified */
        if (permissions != 0) {
            segment->permissions = permissions;
        }
        
        /* Increment reference count */
        segment->ref_count++;
        
        mutex_unlock(&segment->lock);
        mutex_unlock(&shm_global_lock);
        
        return segment;
    }
    
    /* Segment doesn't exist, check if we should create it */
    if (!(flags & SHM_FLAG_CREATE)) {
        LOG_ERROR("Shared memory segment '%s' doesn't exist", name);
        mutex_unlock(&shm_global_lock);
        return NULL;
    }
    
    /* Create new segment */
    segment = (shared_memory_segment_t*)kmalloc(sizeof(shared_memory_segment_t));
    if (segment == NULL) {
        LOG_ERROR("Failed to allocate memory for shared segment struct");
        mutex_unlock(&shm_global_lock);
        return NULL;
    }
    
    /* Initialize segment structure */
    strncpy(segment->name, name, SHM_NAME_MAX - 1);
    segment->name[SHM_NAME_MAX - 1] = '\0';
    segment->size = size;
    segment->permissions = permissions;
    segment->flags = flags;
    segment->ref_count = 1;
    segment->mapping_count = 0;
    segment->next = NULL;
    mutex_init(&segment->lock);
    
    /* Allocate physical memory (page-aligned) */
    uint32_t page_aligned_size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    physical_memory = alloc_pages(page_aligned_size / PAGE_SIZE, ALLOC_ZERO | ALLOC_KERNEL);
    
    if (physical_memory == NULL) {
        LOG_ERROR("Failed to allocate physical memory for shared segment '%s'", name);
        kfree(segment);
        mutex_unlock(&shm_global_lock);
        return NULL;
    }
    
    segment->physical_addr = physical_memory;
    segment->real_size = page_aligned_size;
    created = true;
    
    /* Add to global list */
    segment->next = shm_segments;
    shm_segments = segment;
    shm_segment_count++;
    
    mutex_unlock(&shm_global_lock);
    
    LOG_INFO("Created shared memory segment '%s' with size %u bytes", name, size);
    
    return segment;
}

/*
 * Destroy a shared memory segment
 *
 * shm: Handle to the shared memory segment
 */
void destroy_shared_memory(void* shm) {
    shared_memory_segment_t* segment = (shared_memory_segment_t*)shm;
    shared_memory_segment_t* prev = NULL;
    shared_memory_segment_t* current;
    bool found = false;
    
    if (!shm_initialized || segment == NULL) {
        return;
    }
    
    /* Acquire global lock */
    mutex_lock(&shm_global_lock);
    
    /* Find segment in global list */
    current = shm_segments;
    while (current != NULL) {
        if (current == segment) {
            found = true;
            break;
        }
        prev = current;
        current = current->next;
    }
    
    if (!found) {
        LOG_ERROR("Invalid shared memory segment handle: %p", shm);
        mutex_unlock(&shm_global_lock);
        return;
    }
    
    /* Acquire segment lock */
    mutex_lock(&segment->lock);
    
    /* Decrement reference count */
    segment->ref_count--;
    
    if (segment->ref_count == 0) {
        /* Unmap from all tasks that still have it mapped */
        for (uint32_t i = 0; i < segment->mapping_count; i++) {
            pid_t task_id = segment->mappings[i].task_id;
            unmap_shared_memory_internal(segment, task_id);
        }
        
        /* Remove from global list */
        if (prev == NULL) {
            shm_segments = segment->next;
        } else {
            prev->next = segment->next;
        }
        shm_segment_count--;
        
        /* Free physical memory */
        free_pages(segment->physical_addr, segment->real_size / PAGE_SIZE);
        
        LOG_INFO("Destroyed shared memory segment '%s'", segment->name);
        
        /* Release locks before freeing */
        mutex_unlock(&segment->lock);
        mutex_unlock(&shm_global_lock);
        
        /* Free segment structure */
        kfree(segment);
        
        return;
    }
    
    /* Reference count not zero, just unlock and return */
    mutex_unlock(&segment->lock);
    mutex_unlock(&shm_global_lock);
}

/*
 * Map a shared memory segment into the current task's address space
 *
 * shm: Handle to the shared memory segment
 * permissions: Desired access permissions
 *
 * Returns: Virtual address of the mapped segment or NULL on error
 */
void* map_shared_memory(void* shm, uint32_t permissions) {
    shared_memory_segment_t* segment = (shared_memory_segment_t*)shm;
    pid_t current_task_id;
    void* virtual_addr;
    int result;
    
    if (!shm_initialized || segment == NULL) {
        LOG_ERROR("Invalid shared memory handle: %p", shm);
        return NULL;
    }
    
    /* Get current task ID */
    current_task_id = get_current_pid();
    
    /* Acquire segment lock */
    mutex_lock(&segment->lock);
    
    /* Check if this task already has this segment mapped */
    for (uint32_t i = 0; i < segment->mapping_count; i++) {
        if (segment->mappings[i].task_id == current_task_id) {
            LOG_WARNING("Task %d already has segment '%s' mapped at %p", 
                       current_task_id, segment->name, segment->mappings[i].virtual_addr);
            
            /* Return existing mapping address */
            void* addr = segment->mappings[i].virtual_addr;
            mutex_unlock(&segment->lock);
            return addr;
        }
    }
    
    /* Find a free virtual address region for this mapping */
    task_t* current_task = get_current_task();
    if (current_task == NULL || current_task->page_dir == NULL) {
        LOG_ERROR("Current task has no valid page directory");
        mutex_unlock(&segment->lock);
        return NULL;
    }
    
    /* Find a suitable virtual address - for simplicity, we'll allocate from a hardcoded range */
    /* In a real implementation, this would use a proper virtual memory allocator */
    static uint64_t next_shm_addr = 0x7F0000000000;  /* Start of shared memory region */
    virtual_addr = (void*)next_shm_addr;
    next_shm_addr += segment->real_size;
    
    /* Check permissions */
    uint32_t effective_permissions = permissions & segment->permissions;
    if (effective_permissions == 0) {
        LOG_ERROR("Requested permissions not allowed for segment '%s'", segment->name);
        mutex_unlock(&segment->lock);
        return NULL;
    }
    
    /* Map the physical memory into the task's address space */
    /* Map the physical memory into the task's address space */
    uint64_t page_flags = 0;
    if (effective_permissions & SHM_PERM_READ)
        page_flags |= PAGE_FLAG_READ;
    if (effective_permissions & SHM_PERM_WRITE)
        page_flags |= PAGE_FLAG_WRITE;
    if (effective_permissions & SHM_PERM_EXEC)
        page_flags |= PAGE_FLAG_EXEC;
    
    /* Perform the mapping */
    result = map_pages((uint64_t)virtual_addr, (uint64_t)segment->physical_addr, 
                      segment->real_size, page_flags | PAGE_FLAG_SHARED);
    
    if (result != 0) {
        LOG_ERROR("Failed to map shared memory segment '%s' for task %d, error %d", 
                 segment->name, current_task_id, result);
        mutex_unlock(&segment->lock);
        return NULL;
    }
    
    /* Add to mapping table */
    result = add_mapping(segment, current_task_id, virtual_addr, segment->size, effective_permissions);
    if (result != 0) {
        /* Mapping failed, unmap the memory */
        unmap_pages((uint64_t)virtual_addr, segment->real_size);
        mutex_unlock(&segment->lock);
        return NULL;
    }
    
    /* Flush TLB for the newly mapped pages */
    flush_tlb_range((uint64_t)virtual_addr, segment->real_size);
    
    LOG_INFO("Mapped shared memory segment '%s' for task %d at %p with permissions %x", 
             segment->name, current_task_id, virtual_addr, effective_permissions);
    
    mutex_unlock(&segment->lock);
    
    return virtual_addr;
}
