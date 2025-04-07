/*
 * EdgeX OS - Main Kernel Entry Point
 * 
 * This file contains the main kernel entry function and
 * the early initialization code for the EdgeX microkernel.
 */

#include <edgex/kernel.h>

/* Global kernel information */
const kernel_info_t kernel_info = {
    .version = EDGEX_VERSION_STRING,
    .codename = EDGEX_CODENAME,
    .build_date = __DATE__,
    .build_time = __TIME__,
    .compiler = "GCC " __VERSION__,
#ifdef __x86_64__
    .architecture = "x86_64"
#elif defined(__aarch64__)
    .architecture = "ARM64"
#elif defined(__riscv)
    .architecture = "RISC-V"
#else
    .architecture = "Unknown"
#endif
};

/* Current log level - starts with info level */
int kernel_log_level = LOG_LEVEL_INFO;

/* Multiboot info pointer, set in boot.S */
extern uint64_t multiboot_info;

/* VGA text mode buffer */
static uint16_t* const vga_buffer = (uint16_t*)0xFFFFFFFF800B8000;
static const int vga_width = 80;
static const int vga_height = 25;
static int vga_row = 0;
static int vga_column = 0;
static uint8_t vga_color = 0x0F; /* White on black */

/* Memory information */
static uint64_t total_memory = 0;
static uint64_t available_memory = 0;
static memory_zone_t memory_zones[ZONE_TYPES_COUNT];
static int memory_map_entries = 0;

/* Simple string functions */
static size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len]) {
        len++;
    }
    return len;
}

/* Initialize VGA text mode console */
static void init_vga_console(void) {
    /* Clear the screen */
    for (int y = 0; y < vga_height; y++) {
        for (int x = 0; x < vga_width; x++) {
            const int index = y * vga_width + x;
            vga_buffer[index] = (uint16_t)(' ' | (vga_color << 8));
        }
    }
    
    vga_row = 0;
    vga_column = 0;
}

/* Scroll the screen up one line */
static void vga_scroll(void) {
    /* Move all lines up one */
    for (int y = 0; y < vga_height - 1; y++) {
        for (int x = 0; x < vga_width; x++) {
            const int dst_index = y * vga_width + x;
            const int src_index = (y + 1) * vga_width + x;
            vga_buffer[dst_index] = vga_buffer[src_index];
        }
    }
    
    /* Clear the last line */
    for (int x = 0; x < vga_width; x++) {
        const int index = (vga_height - 1) * vga_width + x;
        vga_buffer[index] = (uint16_t)(' ' | (vga_color << 8));
    }
    
    vga_row = vga_height - 1;
}

/* Put a character to the VGA console */
void kputchar(char c) {
    /* Handle special characters */
    if (c == '\n') {
        vga_column = 0;
        vga_row++;
        if (vga_row >= vga_height) {
            vga_scroll();
        }
        return;
    }
    
    if (c == '\r') {
        vga_column = 0;
        return;
    }
    
    if (c == '\t') {
        /* Tab aligns to 8-character boundary */
        vga_column = (vga_column + 8) & ~7;
        if (vga_column >= vga_width) {
            vga_column = 0;
            vga_row++;
            if (vga_row >= vga_height) {
                vga_scroll();
            }
        }
        return;
    }
    
    /* Put the character at the current position */
    const int index = vga_row * vga_width + vga_column;
    vga_buffer[index] = (uint16_t)(c | (vga_color << 8));
    
    /* Advance the cursor */
    vga_column++;
    if (vga_column >= vga_width) {
        vga_column = 0;
        vga_row++;
        if (vga_row >= vga_height) {
            vga_scroll();
        }
    }
}

/* Put a string to the VGA console */
void kputs(const char* str) {
    for (size_t i = 0; i < strlen(str); i++) {
        kputchar(str[i]);
    }
}

/* Simple printf-like function */
int kprintf(const char* fmt, ...) {
    // A very minimal implementation for now - just print the format string
    // Later, we'll implement a proper formatted output
    kputs(fmt);
    return 0;
}

/* Initialize memory subsystem */
void init_memory(void) {
    LOG_INFO("Initializing memory subsystem...");
    
    /* Zero out memory zones */
    for (int i = 0; i < ZONE_TYPES_COUNT; i++) {
        memory_zones[i].type = i;
        memory_zones[i].size = 0;
        memory_zones[i].free = 0;
        memory_zones[i].pages = 0;
        memory_zones[i].free_pages = 0;
    }
    
    /* Initialize physical memory manager */
    extern void init_physical_memory_manager(void);
    init_physical_memory_manager();
    
    LOG_INFO("Memory initialization complete");
}

/* Parse multiboot2 information structure */
void parse_multiboot_info(void) {
    if (!multiboot_info) {
        LOG_ERROR("No multiboot information available!");
        return;
    }
    
    LOG_INFO("Parsing multiboot information at 0x%llx", (uint64_t)multiboot_info);
    
    /* The pointer is physical, so adjust for higher-half kernel */
    multiboot2_info_t* mb_info = (multiboot2_info_t*)(multiboot_info + 0xFFFFFFFF80000000);
    uint32_t size = mb_info->total_size;
    
    LOG_DEBUG("Multiboot info size: %u bytes", size);
    
    /* Parse all tags */
    multiboot2_tag_t* tag = (multiboot2_tag_t*)(mb_info + 1);
    while ((uint8_t*)tag < (uint8_t*)mb_info + size) {
        /* Align tag to 8 bytes */
        tag = (multiboot2_tag_t*)(((uint64_t)tag + 7) & ~7);
        
        if (tag->type == 0) {
            /* End of tags */
            break;
        }
        
        /* Process different tag types */
        switch (tag->type) {
            case 6: /* Memory Map */
                {
                    multiboot2_mmap_t* mmap = (multiboot2_mmap_t*)tag;
                    uint32_t entries = (mmap->tag.size - sizeof(multiboot2_mmap_t)) / mmap->entry_size;
                    memory_map_entries = entries;
                    
                    LOG_INFO("Memory map: %u entries", entries);
                    
                    memory_map_entry_t* entry = (memory_map_entry_t*)(mmap + 1);
                    for (uint32_t i = 0; i < entries; i++) {
                        uint64_t start = entry->base_addr;
                        uint64_t end = start + entry->length;
                        uint32_t type = entry->type;
                        
                        LOG_DEBUG("Memory region: 0x%llx - 0x%llx, type %u",
                                  start, end, type);
                        
                        /* Update memory statistics */
                        total_memory += entry->length;
                        
                        /* Type 1 is available memory */
                        if (type == 1) {
                            available_memory += entry->length;
                            
                            /* Determine which zone this memory belongs to */
                            memory_zone_type_t zone_type;
                            if (start < 0x1000000) { /* <16MB */
                                zone_type = ZONE_DMA;
                            } else {
                                zone_type = ZONE_NORMAL;
                            }
                            
                            /* Update zone information */
                            memory_zone_t* zone = &memory_zones[zone_type];
                            if (zone->size == 0) {
                                /* First region for this zone */
                                zone->start_address = start;
                                zone->end_address = end;
                            } else {
                                /* Extend existing zone */
                                if (start < zone->start_address) {
                                    zone->start_address = start;
                                }
                                if (end > zone->end_address) {
                                    zone->end_address = end;
                                }
                            }
                            
                            zone->size += entry->length;
                            zone->free += entry->length;
                            
                            uint64_t pages = entry->length / PAGE_SIZE;
                            zone->pages += pages;
                            zone->free_pages += pages;
                        } else {
                            /* Reserved memory, update the reserved zone */
                            memory_zone_t* zone = &memory_zones[ZONE_RESERVED];
                            if (zone->size == 0) {
                                zone->start_address = start;
                                zone->end_address = end;
                            } else {
                                if (start < zone->start_address) {
                                    zone->start_address = start;
                                }
                                if (end > zone->end_address) {
                                    zone->end_address = end;
                                }
                            }
                            
                            zone->size += entry->length;
                            uint64_t pages = entry->length / PAGE_SIZE;
                            zone->pages += pages;
                        }
                        
                        /* Move to next entry */
                        entry = (memory_map_entry_t*)((uint8_t*)entry + mmap->entry_size);
                    }
                }
                break;
                
            case 1: /* Boot Command Line */
                LOG_INFO("Boot command line: %s", (char*)(tag + 1));
                break;
                
            case 2: /* Boot Loader Name */
                LOG_INFO("Boot loader: %s", (char*)(tag + 1));
                break;
                
            case 10: /* APM Table */
                LOG_DEBUG("APM table present");
                break;
                
            case 3: /* Modules */
                LOG_DEBUG("Modules present");
                break;
                
            case 4: /* Basic Memory Info */
                LOG_DEBUG("Basic memory info present");
                break;
                
            case 5: /* BIOS Boot Device */
                LOG_DEBUG("BIOS boot device info present");
                break;
                
            case 8: /* Framebuffer Info */
                LOG_DEBUG("Framebuffer info present");
                break;
                
            default:
                LOG_DEBUG("Unknown multiboot tag: %u", tag->type);
                break;
        }
        
        /* Move to the next tag */
        tag = (multiboot2_tag_t*)((uint8_t*)tag + tag->size);
    }
    
    /* Print memory summary */
    LOG_INFO("Memory summary: %llu MB total, %llu MB available",
             total_memory / (1024 * 1024), available_memory / (1024 * 1024));
    
    for (int i = 0; i < ZONE_TYPES_COUNT; i++) {
        memory_zone_t* zone = &memory_zones[i];
        if (zone->size > 0) {
            LOG_DEBUG("Zone %d: 0x%llx - 0x%llx, %llu MB, %llu pages",
                     i, zone->start_address, zone->end_address,
                     zone->size / (1024 * 1024), zone->pages);
        }
    }
}

/* Early system initialization */
static void early_init(void) {
    /* Initialize the console first for debug output */
    init_vga_console();
    LOG_INFO("EdgeX OS %s (%s) booting...", EDGEX_VERSION_STRING, EDGEX_CODENAME);
    LOG_INFO("Kernel compiled with %s for %s on %s %s",
             kernel_info.compiler, kernel_info.architecture,
             kernel_info.build_date, kernel_info.build_time);
    
    /* Parse multiboot information to get memory map */
    parse_multiboot_info();
    
    /* Initialize memory subsystems */
    init_memory();
}

/* Main kernel entry point (from boot.S) */
void kernel_main(void) {
    /* First phase: early initialization */
    early_init();
    
    /* Print welcome message */
    LOG_INFO("EdgeX OS kernel initialized successfully!");
    LOG_INFO("-------------------------------------------");
    LOG_INFO("A secure, decentralized, microkernel-based OS");
    LOG_INFO("for edge AI devices with quantum readiness");
    LOG_INFO("-------------------------------------------");
    
    /* Initialize scheduler */
    LOG_INFO("Initializing kernel subsystems...");
    
    /* TODO: Initialize remaining kernel subsystems */
    
    /* Enter idle loop for now */
    LOG_INFO("Kernel initialization complete. Entering idle loop.");
    
    /* Simple idle loop - will be replaced with proper scheduler later */
    while (1) {
        /* Enable interrupts and halt until next interrupt */
        sti();
        hlt();
    }
}
