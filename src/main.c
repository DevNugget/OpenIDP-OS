#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>

#include <utility/port.h>
#include <utility/hhdm.h>
#include <utility/kstring.h>
#include <utility/kascii.h>

#include <drivers/com1.h>
#include <drivers/acpi.h>
#include <drivers/apic.h>
#include <drivers/keyboard.h>

#include <descriptors/gdt.h>
#include <descriptors/idt.h>

#include <memory/pmm.h>
#include <memory/vmm.h>
#include <memory/kheap.h>

#include <multitasking/scheduler.h>

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(4);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

static void hcf(void) {
    for (;;) {
        asm ("hlt");
    }
}

void idle_main(void* arg) {
    serial_printf("[SCHED] Idle Process Started\n");
    while (true)
    asm("hlt");
}

void worker_1(void* arg) {
    serial_printf("[SCHED] Worker 1 Process Started with arg: %s\n", (char*)arg);
    
    while (true) {
        for (volatile int i = 0; i < 10000000; i++); 
        
        serial_printf("Worker 1 is running...\n");
    }
}

void worker_2(void* arg) {
    serial_printf("[SCHED] Worker 2 Process Started with arg: %s\n", (char*)arg);
    
    while (true) {
        for (volatile int i = 0; i < 10000000; i++); 
        
        serial_printf("Worker 2 is running...\n");
    }
}

void kmain(void) {
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        hcf();
    }
    
    serial_init();
    gdt_init();
    idt_init();
    hhdm_request_offset();
    pmm_init();
    vmm_init();
    kheap_init();
    acpi_init();
    apic_init();
    apic_timer_init(10);
    keyboard_init();
    
    if (framebuffer_request.response == NULL
        || framebuffer_request.response->framebuffer_count < 1) {
        hcf();
    }
    
    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];
    
    for (size_t i = 0; i < 100; i++) {
        volatile uint32_t *fb_ptr = framebuffer->address;
        fb_ptr[i * (framebuffer->pitch / 4) + i] = 0xffffff;
    }

    create_process("idle", idle_main, NULL);
    create_process("worker1", worker_1, "TestArg");
    create_process("worker2", worker_2, "TestArg");
        
    hcf();
}
