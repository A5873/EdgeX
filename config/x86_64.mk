# EdgeX OS - x86_64 Architecture Configuration
#
# This file contains build settings specific to the x86_64 architecture

# Architecture name
ARCH := x86_64
ARCH_FAMILY := x86

# Cross-compiler settings
CROSS_COMPILE := $(ARCH)-elf-
CC      := $(CROSS_COMPILE)gcc
CXX     := $(CROSS_COMPILE)g++
AS      := $(CROSS_COMPILE)as
LD      := $(CROSS_COMPILE)ld
AR      := $(CROSS_COMPILE)ar
OBJCOPY := $(CROSS_COMPILE)objcopy
STRIP   := $(CROSS_COMPILE)strip
NM      := $(CROSS_COMPILE)nm
OBJDUMP := $(CROSS_COMPILE)objdump

# Architecture-specific flags
CFLAGS += -m64 -mcmodel=kernel -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -ffreestanding
ASFLAGS += --64
LDFLAGS += -m elf_x86_64 -z max-page-size=0x1000

# Architecture-specific definitions
CFLAGS += -D__x86_64__

# Boot directory
BOOT_DIR := $(SRC_DIR)/boot

# Kernel entry point
KERNEL_ENTRY := _start

# Kernel linker script
KERNEL_LDSCRIPT := $(BOOT_DIR)/linker.ld

# Boot files
BOOT_SOURCES := $(BOOT_DIR)/boot.S

# Architecture-specific kernel sources
ARCH_SOURCES := 

# QEMU settings for this architecture
QEMU := qemu-system-x86_64
QEMU_FLAGS := -m 512M -serial stdio -display sdl -no-reboot
QEMU_DEBUG_FLAGS := -S -s

# QEMU boot options
QEMU_BOOT_FLAGS := -cdrom $(ISO_FILE)

# Bootloader settings (GRUB for x86_64)
GRUB_DIR := $(BUILD_DIR)/iso/boot/grub
GRUB_MODULES := multiboot2 normal iso9660 biosdisk test ata

# Boot medium is ISO for x86_64
BOOT_MEDIUM := iso

# Generate ISO image
define make_iso
	@echo "Creating ISO image..."
	@mkdir -p $(GRUB_DIR)
	@cp $(KERNEL_ELF) $(BUILD_DIR)/iso/boot/
	@echo "set timeout=0" > $(GRUB_DIR)/grub.cfg
	@echo "set default=0" >> $(GRUB_DIR)/grub.cfg
	@echo "menuentry \"EdgeX OS $(VERSION)\" {" >> $(GRUB_DIR)/grub.cfg
	@echo "    multiboot2 /boot/$(notdir $(KERNEL_ELF))" >> $(GRUB_DIR)/grub.cfg
	@echo "    boot" >> $(GRUB_DIR)/grub.cfg
	@echo "}" >> $(GRUB_DIR)/grub.cfg
	@grub-mkrescue -o $(ISO_FILE) $(BUILD_DIR)/iso 2>/dev/null
	@echo "ISO image created: $(ISO_FILE)"
endef

# Debug using GDB
define run_debug
	@echo "Starting QEMU in debug mode, connect with 'gdb -ex \"target remote localhost:1234\"'"
	@$(QEMU) $(QEMU_FLAGS) $(QEMU_DEBUG_FLAGS) $(QEMU_BOOT_FLAGS)
endef

# Run in QEMU
define run_qemu
	@echo "Running EdgeX OS ($(ARCH)) in QEMU..."
	@$(QEMU) $(QEMU_FLAGS) $(QEMU_BOOT_FLAGS)
endef

