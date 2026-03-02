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

cpu_status_t* syscall_dispatch(cpu_status_t* context);

#endif