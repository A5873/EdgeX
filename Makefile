# EdgeX OS Makefile
# A secure, decentralized, microkernel-based OS for edge AI devices

# Architecture options: arm64, riscv, x86_64
ARCH ?= x86_64

# Build type: debug, release
BUILD ?= debug

# Base directories
SRC_DIR   := .
BOOT_DIR  := $(SRC_DIR)/boot
KERNEL_DIR := $(SRC_DIR)/kernel
CRYPTO_DIR := $(SRC_DIR)/crypto
SYNC_DIR  := $(SRC_DIR)/sync
LIBC_DIR  := $(SRC_DIR)/libc
INCLUDE_DIR := $(SRC_DIR)/include
TOOLS_DIR := $(SRC_DIR)/tools
TESTS_DIR := $(SRC_DIR)/tests

# Output directories
BUILD_DIR := build/$(ARCH)/$(BUILD)
OBJ_DIR   := $(BUILD_DIR)/obj
BIN_DIR   := $(BUILD_DIR)/bin
LIB_DIR   := $(BUILD_DIR)/lib
ISO_DIR   := $(BUILD_DIR)/iso
TEST_DIR  := $(BUILD_DIR)/test

# Create output directories
$(shell mkdir -p $(OBJ_DIR) $(BIN_DIR) $(LIB_DIR) $(ISO_DIR) $(TEST_DIR))
$(shell mkdir -p $(OBJ_DIR)/kernel $(OBJ_DIR)/boot $(OBJ_DIR)/crypto $(OBJ_DIR)/sync $(OBJ_DIR)/libc $(OBJ_DIR)/tests)

# Architecture-specific settings
ifeq ($(ARCH),arm64)
    CROSS_PREFIX := aarch64-linux-gnu-
    ARCH_FLAGS   := -march=armv8-a
    QEMU         := qemu-system-aarch64
    QEMU_FLAGS   := -machine virt -cpu cortex-a53 -m 1G
endif

ifeq ($(ARCH),riscv)
    CROSS_PREFIX := riscv64-linux-gnu-
    ARCH_FLAGS   := -march=rv64gc -mabi=lp64d
    QEMU         := qemu-system-riscv64
    QEMU_FLAGS   := -machine virt -cpu rv64 -m 1G
endif

ifeq ($(ARCH),x86_64)
    CROSS_PREFIX :=
    ARCH_FLAGS   := -m64
    QEMU         := qemu-system-x86_64
    QEMU_FLAGS   := -cpu qemu64 -m 1G
endif

# Toolchain
CC      := $(CROSS_PREFIX)gcc
AS      := $(CROSS_PREFIX)as
LD      := $(CROSS_PREFIX)ld
AR      := $(CROSS_PREFIX)ar
OBJCOPY := $(CROSS_PREFIX)objcopy
OBJDUMP := $(CROSS_PREFIX)objdump
STRIP   := $(CROSS_PREFIX)strip

# Common flags
INCLUDE_FLAGS := -I$(INCLUDE_DIR) -I$(SRC_DIR)

# Test specific flags
TEST_INCLUDE_FLAGS := $(INCLUDE_FLAGS) -I$(TESTS_DIR)

# Warning flags (strict to maintain code quality)
WARN_FLAGS := -Wall -Wextra -Werror -Wstrict-prototypes -Wold-style-definition \
              -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls \
              -Wnested-externs -Wshadow -Wundef -Wformat=2 -Wno-unused-parameter \
              -Wwrite-strings -Wno-unused-but-set-variable -Wno-missing-field-initializers

# Test warning flags (more compatible across compiler versions)
TEST_WARN_FLAGS := -Wall -Wextra -Werror -Wformat=2 -Wno-unused-parameter \
                   -Wno-missing-field-initializers -Wwrite-strings

# Security flags
SECURITY_FLAGS := -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fPIE

# Build-specific flags
ifeq ($(BUILD),debug)
    OPTIMIZE_FLAGS := -O0 -g3 -DDEBUG
    SECURITY_FLAGS := # Disable security flags in debug mode to make debugging easier
else
    OPTIMIZE_FLAGS := -O2 -DNDEBUG
endif

# Test optimization flags (always include debugging symbols)
TEST_OPTIMIZE_FLAGS := -O0 -g3 -DDEBUG -DUNIT_TEST
# Combine flags
CFLAGS := $(ARCH_FLAGS) $(INCLUDE_FLAGS) $(WARN_FLAGS) $(SECURITY_FLAGS) $(OPTIMIZE_FLAGS) \
          -std=c11 -ffreestanding -nostdlib -nostdinc -fno-builtin -fno-stack-protector \
          -fno-pic -mno-red-zone -mno-mmx -mno-sse -mno-sse2

# Test flags (use standard libraries and include debugging info)
TEST_CFLAGS := $(ARCH_FLAGS) $(TEST_INCLUDE_FLAGS) $(TEST_WARN_FLAGS) $(TEST_OPTIMIZE_FLAGS) \
               -std=c11 -fPIC

ASFLAGS := $(ARCH_FLAGS)
LDFLAGS := -nostdlib -z max-page-size=0x1000
TEST_LDFLAGS := -lpthread -lm

# Source files
KERNEL_SRCS := $(wildcard $(KERNEL_DIR)/*.c)
BOOT_SRCS := $(wildcard $(BOOT_DIR)/*.S) $(wildcard $(BOOT_DIR)/*.c)
CRYPTO_SRCS := $(wildcard $(CRYPTO_DIR)/*.c)
SYNC_SRCS := $(wildcard $(SYNC_DIR)/*.c)
LIBC_SRCS := $(wildcard $(LIBC_DIR)/*.c)

# Test source files (recursive wildcard function)
rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))
TEST_SRCS := $(call rwildcard,$(TESTS_DIR)/,*.c)

# Object files
KERNEL_OBJS := $(patsubst $(KERNEL_DIR)/%.c,$(OBJ_DIR)/kernel/%.o,$(KERNEL_SRCS))
BOOT_OBJS := $(patsubst $(BOOT_DIR)/%.S,$(OBJ_DIR)/boot/%.o,$(filter %.S,$(BOOT_SRCS))) \
             $(patsubst $(BOOT_DIR)/%.c,$(OBJ_DIR)/boot/%.o,$(filter %.c,$(BOOT_SRCS)))
CRYPTO_OBJS := $(patsubst $(CRYPTO_DIR)/%.c,$(OBJ_DIR)/crypto/%.o,$(CRYPTO_SRCS))
SYNC_OBJS := $(patsubst $(SYNC_DIR)/%.c,$(OBJ_DIR)/sync/%.o,$(SYNC_SRCS))
LIBC_OBJS := $(patsubst $(LIBC_DIR)/%.c,$(OBJ_DIR)/libc/%.o,$(LIBC_SRCS))

# Test object files and binaries
TEST_OBJS := $(patsubst $(TESTS_DIR)/%.c,$(OBJ_DIR)/tests/%.o,$(TEST_SRCS))
TEST_BINS := $(patsubst $(TESTS_DIR)/%.c,$(TEST_DIR)/%.test,$(TEST_SRCS))

# All objects
OBJS := $(KERNEL_OBJS) $(BOOT_OBJS) $(CRYPTO_OBJS) $(SYNC_OBJS) $(LIBC_OBJS)

# Target kernel binary
KERNEL_BIN := $(BIN_DIR)/edgex-kernel-$(ARCH)
KERNEL_IMG := $(BIN_DIR)/edgex-kernel-$(ARCH).img

# Targets
.PHONY: all clean arm64 riscv x86_64 debug release run help build-tests run-tests

all: $(KERNEL_BIN)

arm64:
	@$(MAKE) ARCH=arm64

riscv:
	@$(MAKE) ARCH=riscv

x86_64:
	@$(MAKE) ARCH=x86_64

debug:
	@$(MAKE) BUILD=debug

release:
	@$(MAKE) BUILD=release

# Build rules

# Test compilation rule
$(OBJ_DIR)/tests/%.o: $(TESTS_DIR)/%.c
	@echo "CC (test) $<"
	@mkdir -p $(dir $@)
	@$(CC) $(TEST_CFLAGS) -c $< -o $@

# Test linking rule
$(TEST_DIR)/%.test: $(OBJ_DIR)/tests/%.o
	@echo "LD (test) $@"
	@mkdir -p $(dir $@)
	@$(CC) $< -o $@ $(TEST_LDFLAGS)

$(OBJ_DIR)/boot/%.o: $(BOOT_DIR)/%.c
	@echo "CC $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/boot/%.o: $(BOOT_DIR)/%.S
	@echo "AS $<"
	@$(AS) $(ASFLAGS) -c $< -o $@

$(OBJ_DIR)/crypto/%.o: $(CRYPTO_DIR)/%.c
	@echo "CC $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/sync/%.o: $(SYNC_DIR)/%.c
	@echo "CC $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/libc/%.o: $(LIBC_DIR)/%.c
	@echo "CC $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Link kernel
$(KERNEL_BIN): $(OBJS)
	@echo "LD $@"
	@$(LD) $(LDFLAGS) -T $(BOOT_DIR)/linker.ld -o $@ $^
	@echo "OBJCOPY $@.img"
	@$(OBJCOPY) -O binary $@ $(KERNEL_IMG)
	@echo "Built EdgeX OS kernel for $(ARCH) ($(BUILD) mode)"

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf build/
	@echo "Done."

# Test targets
build-tests: $(TEST_BINS)
	@echo "Built $(words $(TEST_BINS)) test binaries"

run-tests: build-tests
	@echo "Running all unit tests..."
	@for test in $(TEST_BINS); do \
		echo "Running $$test..."; \
		$$test; \
		if [ $$? -ne 0 ]; then \
			echo "Test $$test failed"; \
			exit 1; \
		fi; \
	done
	@echo "All tests passed!"

# Run in QEMU
run: $(KERNEL_BIN)
	@echo "Running EdgeX OS in QEMU ($(ARCH))..."
	@$(QEMU) $(QEMU_FLAGS) -kernel $(KERNEL_BIN) -nographic

# Help
help:
	@echo "EdgeX OS Build System"
	@echo "Usage: make [target] [ARCH=arch] [BUILD=build]"
	@echo ""
	@echo "Targets:"
	@echo "  all        - Build the kernel (default)"
	@echo "  clean      - Remove all build artifacts"
	@echo "  arm64      - Build for ARM64 architecture"
	@echo "  riscv      - Build for RISC-V architecture"
	@echo "  x86_64     - Build for x86_64 architecture"
	@echo "  debug      - Build with debug flags"
	@echo "  release    - Build with release flags"
	@echo "  run        - Run the kernel in QEMU"
	@echo "  build-tests - Build all test binaries"
	@echo "  run-tests  - Run all unit tests"
	@echo "  help       - Display this help message"
	@echo ""
	@echo "Options:"
	@echo "  ARCH       - Target architecture (arm64, riscv, x86_64)"
	@echo "  BUILD      - Build type (debug, release)"
	@echo ""
	@echo "Examples:"
	@echo "  make                       - Build for x86_64 in debug mode"
	@echo "  make ARCH=arm64            - Build for ARM64 in debug mode"
	@echo "  make run ARCH=riscv        - Build and run for RISC-V"
	@echo "  make clean                 - Clean all build artifacts"
	@echo "  make release ARCH=x86_64   - Build for x86_64 in release mode"
	@echo "  make build-tests           - Build all test binaries"
	@echo "  make run-tests             - Build and run all tests"

# Default to help if no target is specified
.DEFAULT_GOAL := help

