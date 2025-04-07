# EdgeX

## What is this?

EdgeX is a secure, decentralized, microkernel-based operating system for edge AI devices. It's written from scratch to address the shortcomings of existing systems with bloated kernels, inadequate security models, and overly complex architectures.

## Why?

Because:
- Linux is great for general-purpose servers
- Git solved distributed version control
- Edge AI devices need specialized, efficient systems
- We need a clean slate with better fundamentals

## Core Goals

- Microkernel done right (yes, it can be fast)
- Quantum-secure crypto baked into the OS
- Decentralized, Git-like patch/model sync
- Realtime-capable, deterministic scheduling
- Code that's readable and maintainable

## Tech Stack

- Kernel: C (for performance and control)
- Crypto modules: Rust (for memory safety)
- Sync layer: Custom Git-like engine
- Model loader: Optional AI runtime hooks
- Architecture: ARM64, RISC-V, x86_64

## Subsystems

### IPC (Inter-Process Communication)

EdgeX implements a comprehensive IPC subsystem that includes:

- Mutexes for exclusive access to shared resources
- Semaphores for synchronization and resource counting
- Events for signaling between tasks
- Message Queues with priority support
- Shared Memory for efficient data sharing

## Build Requirements

To build EdgeX, you'll need:

- x86_64-elf-gcc (cross-compiler)
- QEMU (for testing in an emulated environment)
- x86_64-elf-grub (for bootloader)
- xorriso (for creating bootable ISO images)

### Installing Build Requirements

#### On macOS:
```
brew install x86_64-elf-gcc qemu x86_64-elf-grub xorriso
```

#### On Debian/Ubuntu:
```
apt-get install gcc-x86-64-linux-gnu qemu-system-x86 grub-pc-bin xorriso
```

## Building EdgeX

To build the system:

```
make
```

To build a bootable ISO:

```
make iso
```

## Running Tests

EdgeX includes a comprehensive test suite. To run the tests:

```
make test
```

### IPC Subsystem Tests

The IPC subsystem includes tests for:

- Mutex creation, locking, and contention
- Semaphore operations and producer/consumer patterns
- Event signaling and waiting with various reset modes
- Message queue priority handling and boundary conditions
- Shared memory access patterns and permissions

## What this is NOT

- A toy OS
- A purely academic research project
- A bloated Linux distro
- A language ideology showcase

## How to Contribute

- Clone the repo
- Read the code thoroughly
- Submit readable, benchmarked patches
- Follow the coding standards
- Include tests for new functionality

## License

MIT License - See LICENSE file for details

