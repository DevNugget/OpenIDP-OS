#include <drivers/com1.h>
#include <utility/port.h>

#include <stdarg.h>

#define PORT 0x3f8 // COM1

/* https://wiki.osdev.org/Serial_Ports#Initialization */
int serial_init() {
    outportb(PORT + 1, 0x00); // Disable all interrupts
    outportb(PORT + 3, 0x80); // Enable DLAB (set baud rate divisor)
    outportb(PORT + 0, 0x03); // Set divisor to 3 (lo byte) 38400 baud
    outportb(PORT + 1, 0x00); // (hi byte)
    outportb(PORT + 3, 0x03); // 8 bits, no parity, one stop bit
    outportb(PORT + 2, 0xC7); // Enable FIFO, clear them, with 14-byte threshold
    outportb(PORT + 4, 0x0B); // IRQs enabled, RTS/DSR set
    outportb(PORT + 4, 0x1E); // Set in loopback mode, test the serial chip
    outportb(PORT + 0, 0xAE); // Send a test byte
    // Check that we received the same test byte we sent
    if(inportb(PORT + 0) != 0xAE) {
        return 1;
    }
    // If serial is not faulty set it in normal operation mode:
    // not-loopback with IRQs enabled and OUT#1 and OUT#2 bits enabled
    outportb(PORT + 4, 0x0F);
    return 0;
}

void serial_write_str(char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        outportb(PORT, str[i]);
    }
}

void serial_u64_dec(uint64_t n) {
    uint64_t x = n;
    char n_str[20];
    
    int i; // Length of number string
    for (i = 0; x != 0; i++) {
        n_str[i] = '0' + (x % 10);
        x = x/10;
    }
    n_str[i] = '\0';
    
    // Reverse string
    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = n_str[start];
        n_str[start] = n_str[end];
        n_str[end] = temp;
        start++;
        end--;
    }
    
    serial_write_str(n_str);
}

void serial_u64_hex(uint64_t n) {
    uint64_t x = n;
    char n_str[20];
    char hex_letters[] = {'A', 'B', 'C', 'D', 'E', 'F'};
    
    int i; // Length of number string
    for (i = 0; x != 0; i++) {
        uint8_t digit = (x % 16);
        if (digit < 10) {
            n_str[i] = '0' + digit;
        } else {
            n_str[i] = hex_letters[digit - 10];
        }
        x = x/16;
    }
    n_str[i] = '\0';
    
    // Reverse string
    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = n_str[start];
        n_str[start] = n_str[end];
        n_str[end] = temp;
        start++;
        end--;
    }
    
    serial_write_str(n_str);
}

void serial_printf(const char* fmt_str, ...) {
    va_list args;
    va_start(args, fmt_str);

    for (const char* chr = fmt_str; *chr != '\0'; chr++) {
        if (*chr != '%') {
            outportb(PORT, *chr);
            continue;
        }

        chr++;
        switch (*chr) {
            case 'd':
            case 'u': {
                uint64_t n = va_arg(args, uint64_t);
                if (n == 0) serial_write_str("0");
                else serial_u64_dec(n);
                break;
            }
            case 'x':
            case 'p': {
                uint64_t n = va_arg(args, uint64_t);
                if (n == 0) serial_write_str("0");
                else serial_u64_hex(n);
                break;
            }
            case '%': {
                outportb(PORT, '%');
                break;
            }
            default: {
                serial_write_str("[COM1](printf) Unknown specifier");
                break;
            }
        }
    }

    va_end(args);
}