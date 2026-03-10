#ifndef LR35902
#define LR35902

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <instruction.h>

typedef struct flag_reg {
    uint8_t zero;
    uint8_t subtract;
    uint8_t half_carry;
    uint8_t carry;
} flag_reg_t;

typedef struct gba_regs {
    uint8_t a;
    uint8_t f;
    uint8_t b;
    uint8_t c;
    uint8_t d;
    uint8_t e;
    uint8_t h;
    uint8_t l;
    uint16_t pc;
    uint16_t sp;
} gba_regs_t;

typedef struct {
    gba_regs_t regs;
    uint16_t fetched_data;
    uint16_t mem_dest;
    bool dest_is_mem;
    uint8_t current_opcode;
    instruction_t* current_inst;

    bool halted;
    bool stepping;

    bool int_master_enabled;
} lr35902_cpu_ctx_t;

typedef void (*IN_PROC)(lr35902_cpu_ctx_t*);
IN_PROC instruction_get_processor(in_type type);
#define BIT(a, n) ((a & (1 << n)) ? 1 : 0)
#define BIT_SET(a, n, on) { if (on) a |= (1 << n); else a &= ~(1 << n);}
#define CPU_FLAG_Z BIT(ctx->regs.f, 7)
#define CPU_FLAG_C BIT(ctx->regs.f, 4)
uint16_t cpu_read_reg(reg_type rt);
void cpu_set_reg(reg_type rt, uint16_t val);

void lr35902_init();
bool lr35902_step();

#endif