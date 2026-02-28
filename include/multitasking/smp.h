#ifndef SMP_H
#define SMP_H

#include <stdint.h>
#include <stddef.h>

void smp_init(void);
size_t smp_get_cpu_count(void);

#endif //SMP_H
