<img width="2405" height="1080" alt="image" src="https://github.com/user-attachments/assets/740c3eac-6dd4-46cc-be44-c96869cf933a" />

# OpenIDP OS
![GitHub repo size](https://img.shields.io/github/repo-size/DevNugget/OpenIDP-OS?style=flat-square&labelColor=%2345475a&color=%231e66f5)
![GitHub Repo stars](https://img.shields.io/github/stars/DevNugget/OpenIDP-OS?style=flat-square&labelColor=%2345475a&color=%238839ef)
![GitHub commit activity](https://img.shields.io/github/commit-activity/t/DevNugget/OpenIDP-OS?style=flat-square&labelColor=%2345475a&color=%237287fd)

OpenIDP (Idiot's Dream Project) is a 64 bit hobby operating system developed in C, using the [Limine bootloader](https://github.com/limine-bootloader/limine).

# Development Roadmap 
- Descriptors
  - [x] GDT
  - [x] IDT
  - [x] TSS
- Drivers
  - [x] COM1 Serial Output
  - [x] PIT Timer
  - [x] Fatfs File System
  - [x] PS/2 Keyboard Support
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
  - [x] Graphics Libary
  - [x] Tiling Window Manager (On-going)
  - [x] Terminal Emulator
  - [x] Shell (Isolated from Terminal Emulator)
  - [x] Coreutils (On-going)
    - [x] ls
  - [x] Standard Library (On-going)
    - [x] Heap Allocator
    - [x] stdio.h (Minimal/On-going)
    - [x] stdint.h
    - [x] string.h

