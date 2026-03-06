#ifndef KSYSCALL_H
#define KSYSCALL_H

#include <stdint.h>
#include <stddef.h>
#include <utility/cpu_state.h>

#define SYS_YIELD 0
#define SYS_PRINT 1
#define SYS_EXIT  2
#define SYS_SHM_CREATE 3
#define SYS_SHM_MAP 4
#define SYS_SHM_UNMAP 5
#define SYS_SHM_DESTROY 6
#define SYS_FRAMEBUFFER_GET_INFO 7
#define SYS_KEYBOARD_POLL 8
#define SYS_KEYBOARD_READ 9
#define SYS_MOUSE_POLL 10
#define SYS_MOUSE_READ 11
#define SYS_GETPID 12
#define SYS_SPAWN 13
#define SYS_WAIT 14
#define SYS_KILL 15
#define SYS_FS_OPEN 16
#define SYS_FS_READ 17
#define SYS_FS_CLOSE 18
#define SYS_PIPE 19
#define SYS_FS_WRITE 20
#define SYS_SYSINFO 21
#define SYS_PROC_LIST 22
#define SYS_DUP2 23
#define SYS_FS_READDIR 24

#define IDP_O_RDONLY   0x1
#define IDP_O_WRONLY   0x2
#define IDP_O_CREATE   0x4
#define IDP_O_DIRECTORY 0x8

#define IDP_DIRENT_TYPE_FILE 1
#define IDP_DIRENT_TYPE_DIR  2
#define IDP_DIRENT_NAME_MAX 64

typedef struct idp_dirent_t {
    uint8_t type;
    char name[IDP_DIRENT_NAME_MAX];
} idp_dirent_t;


typedef struct syscall_key_event_t {
    uint8_t code;
    uint8_t status_mask;
    uint8_t is_pressed;
} syscall_key_event_t;

typedef struct syscall_mouse_event_t {
    int16_t delta_x;
    int16_t delta_y;
    uint8_t buttons;
} syscall_mouse_event_t;

typedef struct sysinfo_t {
    uint64_t uptime_ms;
    uint64_t total_ram;
    uint64_t free_ram;
    uint32_t procs;
    uint32_t cpus;
} sysinfo_t;

cpu_status_t* syscall_dispatch(cpu_status_t* context);

#endif