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

cpu_status_t* syscall_dispatch(cpu_status_t* context);

#endif