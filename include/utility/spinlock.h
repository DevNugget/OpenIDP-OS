#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <stdint.h>

typedef struct {
    volatile uint32_t value;
} spinlock_t;

#define SPINLOCK_INIT { .value = 0 }

static inline uint64_t irq_save(void) {
    uint64_t flags;
    asm volatile ("pushfq; popq %0; cli" : "=r"(flags) :: "memory");
    return flags;
}

static inline void irq_restore(uint64_t flags) {
    asm volatile ("pushq %0; popfq" :: "r"(flags) : "memory", "cc");
}

static inline void spinlock_lock(spinlock_t* lock) {
    while (__atomic_exchange_n(&lock->value, 1, __ATOMIC_ACQUIRE) != 0) {
        while (__atomic_load_n(&lock->value, __ATOMIC_RELAXED) != 0) {
            asm volatile ("pause");
        }
    }
}

static inline void spinlock_unlock(spinlock_t* lock) {
    __atomic_store_n(&lock->value, 0, __ATOMIC_RELEASE);
}

static inline uint64_t spinlock_lock_irqsave(spinlock_t* lock) {
    uint64_t flags = irq_save();
    spinlock_lock(lock);
    return flags;
}

static inline void spinlock_unlock_irqrestore(spinlock_t* lock, uint64_t flags) {
    spinlock_unlock(lock);
    irq_restore(flags);
}

#endif
