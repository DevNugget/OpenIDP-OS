#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>

#include <utility/port.h>
#include <utility/hhdm.h>
#include <utility/kstring.h>
#include <utility/kascii.h>
#include <utility/cpu_state.h>

#include <drivers/com1.h>
#include <drivers/acpi.h>
#include <drivers/apic.h>
#include <drivers/pci.h>
#include <drivers/keyboard.h>
#include <drivers/mouse.h>
#include <drivers/nvme.h>
#include <drivers/framebuffer.h>

#include <fs/vfs.h>
#include <fs/fatfs_adapter.h>

#include <descriptors/gdt.h>
#include <descriptors/idt.h>

#include <memory/pmm.h>
#include <memory/vmm.h>
#include <memory/kheap.h>

#include <multitasking/scheduler.h>
#include <multitasking/smp.h>

#include <syscall/syscall.h>

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(4);

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
    (void)arg;
    serial_printf("[SCHED] Idle Process Started\n");
    while (true)
    asm("hlt");
}

void worker_1(void* arg) {
    serial_printf("[SCHED] Worker 1 Process Started with arg: %s\n", (char*)arg);
    
    while (true) {
        for (volatile int i = 0; i < 10000000; i++); 
        
        serial_printf("Worker 1 is running on CPU %u...\n", apic_get_id());
    }
}

void worker_2(void* arg) {
    serial_printf("[SCHED] Worker 2 Process Started with arg: %s\n", (char*)arg);
    
    while (true) {
        for (volatile int i = 0; i < 10000000; i++); 
        
        serial_printf("Worker 2 is running on CPU %u...\n", apic_get_id());
    }
}

#define VFS_O_READ   0x1
#define VFS_O_WRITE  0x2
#define VFS_O_CREATE 0x4

static void test_read_file(void) {
    vfs_file_t* f = NULL;
    char buf[128];
    size_t rd = 0;

    vfs_status_t st = vfs_open("/nvme/test.txt", VFS_O_READ, &f);
    if (st != VFS_OK) {
        serial_printf("[TEST] open failed: %d\n", st);
        return;
    }

    st = vfs_read(f, buf, sizeof(buf) - 1, &rd);
    if (st != VFS_OK) {
        serial_printf("[TEST] read failed: %d\n", st);
        vfs_close(f);
        return;
    }

    buf[rd] = '\0';
    serial_printf("[TEST] read %u bytes: %s\n", (uint32_t)rd, buf);
    vfs_close(f);
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
    smp_init();
    scheduler_create_init_processes();
    keyboard_init();
    //mouse_init();
    pci_init();
    nvme_init();
    vfs_init();
    fatfs_mount_nvme("/nvme");
    test_read_file();
    
    if (framebuffer_init_shared_memory() != 0) {
        serial_write_str("[KERNEL] framebuffer shared memory init failed\n");
    }

    //create_process("worker1", worker_1, "TestArg");
    //create_process("worker2", worker_2, "TestArg");
    create_user_process_from_path("idpwm", "/nvme/bin/idpwm.elf");
    //create_user_process_from_path("lscpu", "/nvme/bin/lscpu.elf");
    apic_timer_init(100);
    
    hcf();
}
