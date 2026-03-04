#ifndef SMP_H
#define SMP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

void smp_init(void);
size_t smp_get_cpu_count(void);
bool smp_is_bsp(void);

#endif //SMP_H
