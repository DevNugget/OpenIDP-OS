#include <com1.h>

void serial_init(void) {
    outb(SERIAL_COM1_BASE + SERIAL_IER, 0x00); // Disable interrupts

    outb(SERIAL_COM1_BASE + SERIAL_LCR, LCR_DLAB); // Enable DLAB
    outb(SERIAL_COM1_BASE + SERIAL_DATA, SERIAL_DLL);
    outb(SERIAL_COM1_BASE + SERIAL_IER,  SERIAL_DLH);

    outb(SERIAL_COM1_BASE + SERIAL_LCR,
         LCR_8_BITS | LCR_1_STOP | LCR_NO_PARITY);

    outb(SERIAL_COM1_BASE + SERIAL_FCR,
         FCR_ENABLE_FIFO | FCR_CLEAR_RX | FCR_CLEAR_TX | FCR_14_BYTE);

    outb(SERIAL_COM1_BASE + SERIAL_MCR,
         MCR_DTR | MCR_RTS | MCR_OUT2);

    serial_printf("Serial output ready on COM1.\n");
}

static int serial_transmit_empty(void) {
    return inb(SERIAL_COM1_BASE + 5) & 0x20;
}

static void serial_write_uint(uint64_t value, int base) {
    char buffer[32];
    int i = 0;

    if (value == 0) {
        serial_write_char('0');
        return;
    }

    while (value > 0) {
        uint64_t digit = value % base;
        buffer[i++] = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
        value /= base;
    }

    // Reverse output
    while (i--)
        serial_write_char(buffer[i]);
}

static void serial_write_int(int64_t value) {
    if (value < 0) {
        serial_write_char('-');
        serial_write_uint((uint64_t)(-value), 10);
    } else {
        serial_write_uint((uint64_t)value, 10);
    }
}

void serial_write_char(char c) {
    while (!serial_transmit_empty());
    outb(SERIAL_COM1_BASE, c);
}

void serial_write_string(const char* str) {
    while (*str) {
        if (*str == '\n')
            serial_write_char('\r');
        serial_write_char(*str++);
    }
}

void serial_printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    while (*fmt) {
        if (*fmt != '%') {
            serial_write_char(*fmt++);
            continue;
        }

        fmt++; // skip '%'

        switch (*fmt) {
            case 's': {
                const char* s = va_arg(args, const char*);
                serial_write_string(s ? s : "(null)");
                break;
            }

            case 'c': {
                char c = (char)va_arg(args, int);
                serial_write_char(c);
                break;
            }

            case 'd': {
                int v = va_arg(args, int);
                serial_write_int(v);
                break;
            }

            case 'u': {
                unsigned v = va_arg(args, unsigned);
                serial_write_uint(v, 10);
                break;
            }

            case 'x': {
                unsigned v = va_arg(args, unsigned);
                serial_write_uint(v, 16);
                break;
            }

            case '%':
                serial_write_char('%');
                break;

            default:
                serial_write_string("[?]");
                break;
        }

        fmt++;
    }

    va_end(args);
}