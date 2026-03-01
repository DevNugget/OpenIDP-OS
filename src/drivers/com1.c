#include <drivers/com1.h>
#include <utility/port.h>
#include <utility/spinlock.h>

#include <stddef.h>
#include <stdarg.h>

#define PORT 0x3f8 // COM1

static spinlock_t serial_lock = SPINLOCK_INIT;

/* https://wiki.osdev.org/Serial_Ports#Initialization */
int serial_init() {
    uint64_t flags = spinlock_lock_irqsave(&serial_lock);
    
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
        spinlock_unlock_irqrestore(&serial_lock, flags);
        return 1;
    }
    
    // If serial is not faulty set it in normal operation mode:
    // not-loopback with IRQs enabled and OUT#1 and OUT#2 bits enabled
    outportb(PORT + 4, 0x0F);
    
    spinlock_unlock_irqrestore(&serial_lock, flags);
    return 0;
}

static void _serial_write_str(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        outportb(PORT, str[i]);
    }
}

static void _serial_u64_dec(uint64_t n) {
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
    
    _serial_write_str(n_str);
}

static void _serial_u64_hex(uint64_t n) {
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
    
    _serial_write_str(n_str);
}

void serial_write_str(char* str) {
    uint64_t flags = spinlock_lock_irqsave(&serial_lock);
    _serial_write_str(str);
    spinlock_unlock_irqrestore(&serial_lock, flags);
}

void serial_u64_dec(uint64_t n) {
    uint64_t flags = spinlock_lock_irqsave(&serial_lock);
    _serial_u64_dec(n);
    spinlock_unlock_irqrestore(&serial_lock, flags);
}

void serial_u64_hex(uint64_t n) {
    uint64_t flags = spinlock_lock_irqsave(&serial_lock);
    _serial_u64_hex(n);
    spinlock_unlock_irqrestore(&serial_lock, flags);
}

void serial_printf(const char* fmt_str, ...) {
    uint64_t flags = spinlock_lock_irqsave(&serial_lock);
    
    va_list args;
    va_start(args, fmt_str);

    for (const char* chr = fmt_str; *chr != '\0'; chr++) {
        if (*chr != '%') {
            outportb(PORT, *chr);
            continue;
        }

        chr++;
        switch (*chr) {
            case 's': {
                char* str = va_arg(args, char*); 
                if (str == NULL) {
                    str = "(null)";
                }
                while (*str) {
                    outportb(PORT, *str++);
                }
                break;
            }
            case 'c': {
                char c = (char)va_arg(args, int);
                outportb(PORT, c);
                break;
            }
            case 'd':
            case 'u': {
                uint64_t n = va_arg(args, uint64_t);
                if (n == 0) _serial_write_str("0");
                else _serial_u64_dec(n);
                break;
            }
            case 'x':
            case 'p': {
                uint64_t n = va_arg(args, uint64_t);
                if (n == 0) _serial_write_str("0");
                else _serial_u64_hex(n);
                break;
            }
            case '%': {
                outportb(PORT, '%');
                break;
            }
            default: {
                _serial_write_str("[COM1](printf) Unknown specifier");
                break;
            }
        }
    }

    va_end(args);
    
    spinlock_unlock_irqrestore(&serial_lock, flags);
}