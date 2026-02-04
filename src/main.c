#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <limine.h>
#include <gdt.h>
#include <idt.h>

#include <pmm.h>
#include <vmm.h>
#include <kheap.h>

#include <pic.h>
#include <com1.h>

#include <task.h>

#include <graphics.h>
#include <keyboard.h>
#include <fatfs/ff.h>

#define PML4_ENTRY_COUNT 512
#define PIT_FREQUENCY_HZ 200

// Set the base revision to 4, this is recommended as this is the latest
// base revision described by the Limine boot protocol specification.
// See specification for further info.

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(4);

// The Limine requests can be placed anywhere, but it is important that
// the compiler does not optimise them away, so, usually, they should
// be made volatile or equivalent, _and_ they should be accessed at least
// once or marked as used with the "used" attribute as done here.

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_executable_address_request kernel_addr_request = {
    .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST_ID,
    .revision = 0
};

// Finally, define the start and end markers for the Limine requests.
// These can also be moved anywhere, to any .c file, as seen fit.

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

static void hcf(void) {for (;;) {asm ("hlt");}}
static void system_init();
static void copy_bootloader_pml4(uint64_t hhdm_offset);
static void mount_filesystem();

uint64_t* kernel_pml4;
FATFS fat_fs;

void kmain(void) {
    // Ensure the bootloader actually understands our base revision (see spec).
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        hcf();
    }

    system_init();

    create_user_process_from_file("/bin/idpwm.elf", 1);

    hcf();
}

static void system_init() {
    struct limine_memmap_response* memmap = memmap_request.response;
    struct limine_executable_address_response* kernel_addr = kernel_addr_request.response;
    struct limine_hhdm_response* hhdm_response = hhdm_request.response;

    serial_init();
    gdt_init();
    idt_init();
    pmm_init(memmap, kernel_addr, hhdm_response);
    
    copy_bootloader_pml4(hhdm_response->offset);
    vmm_switch_pml4(kernel_pml4);

    heap_init();
    keyboard_init();
    mount_filesystem();
    scheduler_init();
    graphics_init();
    pit_init(PIT_FREQUENCY_HZ);
}

static void copy_bootloader_pml4(uint64_t hhdm_offset) {
    // Get current PML4 from bootloader
    uint64_t boot_cr3 = read_cr3();
    uint64_t* boot_pml4 = (uint64_t*)((boot_cr3 & PAGE_ALIGN_MASK) + hhdm_offset);
    
    kernel_pml4 = vmm_create_pml4();
    
    // Copy all existing mappings from bootloader's PML4
    for (size_t i = 0; i < PML4_ENTRY_COUNT; i++) {
        if (boot_pml4[i] & VMM_PRESENT) {
            kernel_pml4[i] = boot_pml4[i];
        }
    }
}

static void mount_filesystem() {
    FRESULT res = f_mount(&fat_fs, "", 1); // 1 = mount now
    if (res != FR_OK) {
        serial_printf("Failed to mount filesystem\n");
        return;
    }
    serial_printf("Mounted filesystem\n");
}
