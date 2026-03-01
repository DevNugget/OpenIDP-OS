#ifndef KSYSCALL_H
#define KSYSCALL_H

#include <stdint.h>
#include <stddef.h>
#include <utility/cpu_state.h>

cpu_status_t* syscall_dispatch(cpu_status_t* context);

#endif