#include <libidp/syscall.h>
#include <libidp/window_client.h>
#include <libidp/stdio.h>
#include <libgfx/gfx.h>
#include <stdint.h>

#include <emulator.h>

#define ZERO_FLAG_BYTE_POSITION 7
#define SUBTRACT_FLAG_BYTE_POSITION 6
#define HALF_CARRY_FLAG_BYTE_POSITION 5
#define CARRY_FLAG_BYTE_POSITION 4

typedef struct flag_reg {
    uint8_t zero;
    uint8_t subtract;
    uint8_t half_carry;
    uint8_t carry;
} flag_reg_t;

typedef struct gba_regs {
    uint8_t a;
    uint8_t b;
    uint8_t c;
    uint8_t d;
    uint8_t e;
    flag_reg_t f;
    uint8_t h;
    uint8_t l;
} gba_regs_t;

uint8_t flag_reg_to_u8(flag_reg_t flag) {
    return ((flag.zero       ? 1 : 0) << ZERO_FLAG_BYTE_POSITION) |
           ((flag.subtract   ? 1 : 0) << SUBTRACT_FLAG_BYTE_POSITION) |
           ((flag.half_carry ? 1 : 0) << HALF_CARRY_FLAG_BYTE_POSITION) |
           ((flag.carry      ? 1 : 0) << CARRY_FLAG_BYTE_POSITION);
}

flag_reg_t u8_to_flag_reg(uint8_t byte) {
    flag_reg_t flag;
    
    flag.zero       = ((byte >> ZERO_FLAG_BYTE_POSITION) & 0b1) != 0;
    flag.subtract   = ((byte >> SUBTRACT_FLAG_BYTE_POSITION) & 0b1) != 0;
    flag.half_carry = ((byte >> HALF_CARRY_FLAG_BYTE_POSITION) & 0b1) != 0;
    flag.carry      = ((byte >> CARRY_FLAG_BYTE_POSITION) & 0b1) != 0;

    return flag;
}

uint16_t get_af(gba_regs_t *regs) {
    return ((uint16_t)regs->a) << 8 | ((uint16_t)flag_reg_to_u8(regs->f));
}

void set_af(gba_regs_t *regs, uint16_t value) {
    regs->a = (uint8_t)((value & 0xFF00) >> 8);
    regs->f = u8_to_flag_reg((uint8_t)(value & 0xFF));
}

uint16_t get_bc(gba_regs_t *regs) {
    return ((uint16_t)regs->b) << 8 | ((uint16_t)regs->c);
}

void set_bc(gba_regs_t *regs, uint16_t value) {
    regs->b = (uint8_t)((value & 0xFF00) >> 8);
    regs->c = (uint8_t)(value & 0xFF);
}

uint16_t get_de(gba_regs_t *regs) {
    return ((uint16_t)regs->d) << 8 | ((uint16_t)regs->e);
}

void set_de(gba_regs_t *regs, uint16_t value) {
    regs->d = (uint8_t)((value & 0xFF00) >> 8);
    regs->e = (uint8_t)(value & 0xFF);
}

uint16_t get_hl(gba_regs_t *regs) {
    return ((uint16_t)regs->h) << 8 | ((uint16_t)regs->l);
}

void set_hl(gba_regs_t *regs, uint16_t value) {
    regs->h = (uint8_t)((value & 0xFF00) >> 8);
    regs->l = (uint8_t)(value & 0xFF);
}

void main(int argc, char** argv) {
    emulator_run(argc, argv);
    
    /*
    idp_window_t window;
    if (idp_window_open(&window, "idpgba") != ERR_SUCCESS) {
        printf("idpgb: Error, could not create window.\n");
        sys_exit(1);
    }

    int running = 1;
    while (running) {
        key_event_t ev;
        while (idp_window_poll_key(&window, &ev)) {
            if (ev.is_pressed && ev.code == KEY_ESC) {
                running = 0;
            }
        }

        gfx_clear(&window.gfx, gfx_rgb(30, 30, 46));
        gfx_fill_rect(&window.gfx, 100, 100, 200, 150, gfx_rgb(180, 190, 254));

        idp_window_present(&window);
        sys_yield();
    }

    idp_window_destroy(&window);
    */
    sys_exit(0);
}
