<img width="2405" height="1080" alt="image" src="https://github.com/user-attachments/assets/740c3eac-6dd4-46cc-be44-c96869cf933a" />

# OpenIDP OS
OpenIDP (Idiot's Dream Project) is a 64 bit hobby operating system developed in C, using the [Limine bootloader](https://github.com/limine-bootloader/limine).

# Development Roadmap 
- Descriptors
  - [x] GDT
  - [x] IDT
  - [x] TSS
- Drivers
  - [x] COM1 Serial Output
  - [x] PIT Timer
- Memory
  - [x] Physical Memory Manager
  - [x] Virtual Memory Manager
  - [x] Kernel Heap Allocator
- Multi-tasking
  - [x] Round-robin Scheduler
  - [x] Kernel & User-mode Process Creation
  - [x] Process Exit & Clean-up
- User-land
  - [x] ELF Binary Loading
  - [x] Syscall Interface
