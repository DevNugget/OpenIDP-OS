#include <LR35902.h>
#include <emulator.h>
#include <bus.h>

#include <libidp/stdio.h>
#include <libidp/ansi.h>
#include <libidp/syscall.h>

lr35902_cpu_ctx_t cpu_ctx = {0};

uint16_t reverse(uint16_t n) {
    return ((n & 0xFF00) >> 8) | ((n & 0x00FF) << 8);
}

uint16_t cpu_read_reg(reg_type rt) {
    switch(rt) {
        case RT_A: return cpu_ctx.regs.a;
        case RT_F: return cpu_ctx.regs.f;
        case RT_B: return cpu_ctx.regs.b;
        case RT_C: return cpu_ctx.regs.c;
        case RT_D: return cpu_ctx.regs.d;
        case RT_E: return cpu_ctx.regs.e;
        case RT_H: return cpu_ctx.regs.h;
        case RT_L: return cpu_ctx.regs.l;

        case RT_AF: return reverse(*((uint16_t *)&cpu_ctx.regs.a));
        case RT_BC: return reverse(*((uint16_t *)&cpu_ctx.regs.b));
        case RT_DE: return reverse(*((uint16_t *)&cpu_ctx.regs.d));
        case RT_HL: return reverse(*((uint16_t *)&cpu_ctx.regs.h));

        case RT_PC: return cpu_ctx.regs.pc;
        case RT_SP: return cpu_ctx.regs.sp;
        default: return 0;
    }
}

void cpu_set_reg(reg_type rt, uint16_t val) {
    switch(rt) {
        case RT_A: cpu_ctx.regs.a = val & 0xFF; break;
        case RT_F: cpu_ctx.regs.f = val & 0xFF; break;
        case RT_B: cpu_ctx.regs.b = val & 0xFF; break;
        case RT_C: {
            cpu_ctx.regs.c = val & 0xFF;
        } break;
        case RT_D: cpu_ctx.regs.d = val & 0xFF; break;
        case RT_E: cpu_ctx.regs.e = val & 0xFF; break;
        case RT_H: cpu_ctx.regs.h = val & 0xFF; break;
        case RT_L: cpu_ctx.regs.l = val & 0xFF; break;

        case RT_AF: *((uint16_t *)&cpu_ctx.regs.a) = reverse(val); break;
        case RT_BC: *((uint16_t *)&cpu_ctx.regs.b) = reverse(val); break;
        case RT_DE: *((uint16_t *)&cpu_ctx.regs.d) = reverse(val); break;
        case RT_HL: {
         *((uint16_t *)&cpu_ctx.regs.h) = reverse(val); 
         break;
        }

        case RT_PC: cpu_ctx.regs.pc = val; break;
        case RT_SP: cpu_ctx.regs.sp = val; break;
        case RT_NONE: break;
    }
}

void lr35902_init() {
    cpu_ctx.regs.pc = 0x100;
}

static void fetch_instruction() {
    cpu_ctx.current_opcode = bus_read(cpu_ctx.regs.pc++);
    cpu_ctx.current_inst = instruction_by_opcode(cpu_ctx.current_opcode);
}

static void fetch_data() {
    cpu_ctx.mem_dest = 0;
    cpu_ctx.dest_is_mem = false;
    
    if (cpu_ctx.current_inst == NULL) {
        return;
    }

    switch(cpu_ctx.current_inst->mode) {
        case AM_IMP: return;

        case AM_R:
            cpu_ctx.fetched_data = cpu_read_reg(cpu_ctx.current_inst->reg_1);
            return;

        case AM_R_D8:
            cpu_ctx.fetched_data = bus_read(cpu_ctx.regs.pc);
            emulator_cycles(1);
            cpu_ctx.regs.pc++;
            return;

        case AM_R_D16:
        case AM_D16: {
            uint16_t lo = bus_read(cpu_ctx.regs.pc);
            emulator_cycles(1);

            uint16_t hi = bus_read(cpu_ctx.regs.pc + 1);
            emulator_cycles(1);

            cpu_ctx.fetched_data = lo | (hi << 8);

            cpu_ctx.regs.pc += 2;

            return;
        }

        case AM_MR_R:
            cpu_ctx.fetched_data = cpu_read_reg(cpu_ctx.current_inst->reg_2);
            cpu_ctx.mem_dest = cpu_read_reg(cpu_ctx.current_inst->reg_1);
            cpu_ctx.dest_is_mem = true;

            if (cpu_ctx.current_inst->reg_1 == RT_C) {
                cpu_ctx.mem_dest |= 0xFF00;
            }

            return;

        case AM_R_MR: {
            uint16_t addr = cpu_read_reg(cpu_ctx.current_inst->reg_2);

            if (cpu_ctx.current_inst->reg_2 == RT_C) {
                addr |= 0xFF00;
            }

            cpu_ctx.fetched_data = bus_read(addr);
            emulator_cycles(1);

        } return;

        case AM_R_HLI:
            cpu_ctx.fetched_data = bus_read(cpu_read_reg(cpu_ctx.current_inst->reg_2));
            emulator_cycles(1);
            cpu_set_reg(RT_HL, cpu_read_reg(RT_HL) + 1);
            return;

        case AM_R_HLD:
            cpu_ctx.fetched_data = bus_read(cpu_read_reg(cpu_ctx.current_inst->reg_2));
            emulator_cycles(1);
            cpu_set_reg(RT_HL, cpu_read_reg(RT_HL) - 1);
            return;

        case AM_HLI_R:
            cpu_ctx.fetched_data = cpu_read_reg(cpu_ctx.current_inst->reg_2);
            cpu_ctx.mem_dest = cpu_read_reg(cpu_ctx.current_inst->reg_1);
            cpu_ctx.dest_is_mem = true;
            cpu_set_reg(RT_HL, cpu_read_reg(RT_HL) + 1);
            return;

        case AM_HLD_R:
            cpu_ctx.fetched_data = cpu_read_reg(cpu_ctx.current_inst->reg_2);
            cpu_ctx.mem_dest = cpu_read_reg(cpu_ctx.current_inst->reg_1);
            cpu_ctx.dest_is_mem = true;
            cpu_set_reg(RT_HL, cpu_read_reg(RT_HL) - 1);
            return;

        case AM_R_A8:
            cpu_ctx.fetched_data = bus_read(cpu_ctx.regs.pc);
            emulator_cycles(1);
            cpu_ctx.regs.pc++;
            return;

        case AM_A8_R:
            cpu_ctx.mem_dest = bus_read(cpu_ctx.regs.pc) | 0xFF00;
            cpu_ctx.dest_is_mem = true;
            emulator_cycles(1);
            cpu_ctx.regs.pc++;
            return;

        case AM_HL_SPR:
            cpu_ctx.fetched_data = bus_read(cpu_ctx.regs.pc);
            emulator_cycles(1);
            cpu_ctx.regs.pc++;
            return;

        case AM_D8:
            cpu_ctx.fetched_data = bus_read(cpu_ctx.regs.pc);
            emulator_cycles(1);
            cpu_ctx.regs.pc++;
            return;

        case AM_A16_R:
        case AM_D16_R: {
            uint16_t lo = bus_read(cpu_ctx.regs.pc);
            emulator_cycles(1);

            uint16_t hi = bus_read(cpu_ctx.regs.pc + 1);
            emulator_cycles(1);

            cpu_ctx.mem_dest = lo | (hi << 8);
            cpu_ctx.dest_is_mem = true;

            cpu_ctx.regs.pc += 2;
            cpu_ctx.fetched_data = cpu_read_reg(cpu_ctx.current_inst->reg_2);

        } return;

        case AM_MR_D8:
            cpu_ctx.fetched_data = bus_read(cpu_ctx.regs.pc);
            emulator_cycles(1);
            cpu_ctx.regs.pc++;
            cpu_ctx.mem_dest = cpu_read_reg(cpu_ctx.current_inst->reg_1);
            cpu_ctx.dest_is_mem = true;
            return;

        case AM_MR:
            cpu_ctx.mem_dest = cpu_read_reg(cpu_ctx.current_inst->reg_1);
            cpu_ctx.dest_is_mem = true;
            cpu_ctx.fetched_data = bus_read(cpu_read_reg(cpu_ctx.current_inst->reg_1));
            emulator_cycles(1);
            return;

        case AM_R_A16: {
            uint16_t lo = bus_read(cpu_ctx.regs.pc);
            emulator_cycles(1);

            uint16_t hi = bus_read(cpu_ctx.regs.pc + 1);
            emulator_cycles(1);

            uint16_t addr = lo | (hi << 8);

            cpu_ctx.regs.pc += 2;
            cpu_ctx.fetched_data = bus_read(addr);
            emulator_cycles(1);

            return;
        }

        default:
            printf("idpgb: unknown addressing mode! %d (%x)\n", 
                cpu_ctx.current_inst->mode, cpu_ctx.current_opcode);
            sys_exit(-7);
            return;
    }
}

static void execute() {
    IN_PROC proc = instruction_get_processor(cpu_ctx.current_inst->type);
    if (!proc) {
        sys_exit(-69);
    }
    
    proc(&cpu_ctx);
}

bool lr35902_step() {
    if (!cpu_ctx.halted) {
        uint16_t pc = cpu_ctx.regs.pc;

        fetch_instruction();
        fetch_data();

        printf(
            "%x: %s (%x %x %x)\n", 
            pc, instruction_name(cpu_ctx.current_inst->type),
            cpu_ctx.current_opcode, bus_read(pc+1), bus_read(pc+2)
        );

        if (cpu_ctx.current_inst == NULL) {
            printf("idpgb: unknown instruction %x\n", cpu_ctx.current_opcode);
        }

        execute();
    }

    return true;
}