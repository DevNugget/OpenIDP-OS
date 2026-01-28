typedef unsigned long long uint64_t;

void _start() {
    // 1. Do some work (Waste time)
    // This proves the scheduler can switch to us and back to kernel
    volatile int i = 0;
    while(i < 10000000) {
        i++;
    }

    // 2. The "Hello World" of User Mode: Trigger a Page Fault
    // We try to write to a random address (0xDEADBEEF).
    // This will trigger the Kernel's Page Fault Handler.
    // If the error code says "User Mode", WE WIN.
    *((volatile char*)0xDEADBEEF) = 'A';

    // 3. Infinite loop (incase the fault handler returns)
    while(1);
}