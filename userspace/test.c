typedef unsigned long long uint64_t;

// Syscall wrapper function
// Arguments: RDI, RSI, RDX
// Syscall Number: RAX
void syscall(uint64_t number, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    asm volatile (
        "int $0x80"
        : 
        : "a"(number), "D"(arg1), "S"(arg2), "d"(arg3)
        : "memory"
    );
}

void _start() {
    char msg[] = "Hello from User Space via int 0x80!\n";
    
    while(1) {
        syscall(0, 1, (uint64_t)msg, sizeof(msg) - 1);
        for(volatile int i=0; i<10000000; i++); 
    }
    // Syscall 0: Write (1 = stdout (ignored), msg, length)
    syscall(0, 1, (uint64_t)msg, sizeof(msg) - 1);
    
    // Syscall 1: Exit (code 0)
    //syscall(1, 0, 0, 0);

    // Should not reach here
    while(1);
}