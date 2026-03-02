#include <drivers/mouse.h>
#include <drivers/apic.h>
#include <utility/port.h>
#include <utility/kstring.h>

#define MOUSE_DATA_PORT 0x60
#define MOUSE_STATUS_PORT 0x64
#define MOUSE_COMMAND_PORT 0x64

#define MOUSE_WRITE_AUX 0xD4
#define MOUSE_ENABLE_AUX 0xA8
#define MOUSE_READ_CONFIG 0x20
#define MOUSE_WRITE_CONFIG 0x60

#define MOUSE_CMD_SET_DEFAULTS 0xF6
#define MOUSE_CMD_ENABLE_STREAMING 0xF4
#define MOUSE_ACK 0xFA

#define BUFFER_SIZE 128

static mouse_event_t mouse_buffer[BUFFER_SIZE];
static uint8_t write_pos;
static uint8_t read_pos;
static uint8_t packet[3];
static uint8_t packet_index;

static bool mouse_wait_read(void) {
    for (uint32_t i = 0; i < 100000; ++i) {
        if (inportb(MOUSE_STATUS_PORT) & 0x01) {
            return true;
        }
    }
    return false;
}

static bool mouse_wait_write(void) {
    for (uint32_t i = 0; i < 100000; ++i) {
        if ((inportb(MOUSE_STATUS_PORT) & 0x02) == 0) {
            return true;
        }
    }
    return false;
}

static void mouse_write(uint8_t value) {
    if (!mouse_wait_write()) {
        return;
    }
    outportb(MOUSE_COMMAND_PORT, MOUSE_WRITE_AUX);

    if (!mouse_wait_write()) {
        return;
    }
    outportb(MOUSE_DATA_PORT, value);
}

static uint8_t mouse_read_data(void) {
    if (!mouse_wait_read()) {
        return 0;
    }
    return inportb(MOUSE_DATA_PORT);
}

void mouse_init(void) {
    io_apic_map_irq(12, 0x2C);

    memset(mouse_buffer, 0, sizeof(mouse_buffer));
    write_pos = 0;
    read_pos = 0;
    packet_index = 0;

    if (!mouse_wait_write()) {
        return;
    }
    outportb(MOUSE_COMMAND_PORT, MOUSE_ENABLE_AUX);

    if (!mouse_wait_write()) {
        return;
    }
    outportb(MOUSE_COMMAND_PORT, MOUSE_READ_CONFIG);
    uint8_t status = mouse_read_data();

    status |= 0x02;
    status &= (uint8_t)~0x20;

    if (!mouse_wait_write()) {
        return;
    }
    outportb(MOUSE_COMMAND_PORT, MOUSE_WRITE_CONFIG);
    if (!mouse_wait_write()) {
        return;
    }
    outportb(MOUSE_DATA_PORT, status);

    mouse_write(MOUSE_CMD_SET_DEFAULTS);
    (void)mouse_read_data();

    mouse_write(MOUSE_CMD_ENABLE_STREAMING);
    (void)mouse_read_data();
}

void mouse_driver_irq_handler(void) {
    uint8_t status = inportb(MOUSE_STATUS_PORT);
    if ((status & 0x20) == 0) {
        return;
    }

    uint8_t data = inportb(MOUSE_DATA_PORT);

    if (packet_index == 0 && (data & 0x08) == 0) {
        return;
    }

    packet[packet_index++] = data;
    if (packet_index < 3) {
        return;
    }

    packet_index = 0;

    if ((packet[0] & 0x40) != 0 || (packet[0] & 0x80) != 0) {
        return;
    }

    int16_t dx = (int16_t)((int8_t)packet[1]);
    int16_t dy = (int16_t)(-((int8_t)packet[2]));

    mouse_event_t event;
    event.delta_x = dx;
    event.delta_y = dy;
    event.buttons = (uint8_t)(packet[0] & 0x07);

    uint8_t next = (uint8_t)((write_pos + 1) % BUFFER_SIZE);
    if (next != read_pos) {
        mouse_buffer[write_pos] = event;
        write_pos = next;
    }
}

bool mouse_poll(void) {
    return write_pos != read_pos;
}

mouse_event_t mouse_read(void) {
    mouse_event_t event = {0};
    if (write_pos != read_pos) {
        event = mouse_buffer[read_pos];
        read_pos = (uint8_t)((read_pos + 1) % BUFFER_SIZE);
    }
    return event;
}
