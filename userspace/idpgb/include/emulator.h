#ifndef EMULATOR_H
#define EMULATOR_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    bool running;
    bool paused;
    uint64_t ticks;
} emulator_ctx_t;

int emulator_run(int argc, char** argv);
emulator_ctx_t* get_emulator_ctx();
void emulator_cycles(int cpu_cycles);

#endif