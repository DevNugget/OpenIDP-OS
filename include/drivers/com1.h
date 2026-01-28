#ifndef COM1_H
#define COM1_H

#include <stdarg.h>
#include <io.h>

#define SERIAL_COM1_BASE  0x3F8

#define SERIAL_DATA       0   // Transmit / Receive buffer
#define SERIAL_IER        1   // Interrupt Enable Register
#define SERIAL_FCR        2   // FIFO Control Register
#define SERIAL_LCR        3   // Line Control Register
#define SERIAL_MCR        4   // Modem Control Register
#define SERIAL_LSR        5   // Line Status Register

#define LCR_DLAB          0x80   // Enable Divisor Latch Access
#define LCR_8_BITS        0x03   // 8 data bits
#define LCR_1_STOP        0x00
#define LCR_NO_PARITY     0x00

#define FCR_ENABLE_FIFO   0x01
#define FCR_CLEAR_RX      0x02
#define FCR_CLEAR_TX      0x04
#define FCR_14_BYTE       0xC0   // Trigger level: 14 bytes

#define MCR_DTR           0x01
#define MCR_RTS           0x02
#define MCR_OUT2          0x08   // Needed for IRQs

#define SERIAL_BAUD       38400
#define SERIAL_DIVISOR    (115200 / SERIAL_BAUD)

#define SERIAL_DLL        (SERIAL_DIVISOR & 0xFF)
#define SERIAL_DLH        ((SERIAL_DIVISOR >> 8) & 0xFF)

void serial_init(void);
void serial_write_char(char c);
void serial_write_string(const char* str);
void serial_printf(const char* fmt, ...);

#endif