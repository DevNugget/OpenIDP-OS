#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <limine.h>
#include <gdt.h>
#include <idt.h>
#include <pmm.h>
#include <vmm.h>
#include <kheap.h>
#include <task.h>
#include <pic.h>
#include <com1.h>
#include <graphics.h>
#include <keyboard.h>

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

uint64_t* kernel_pml4; // Global variable to hold the kernel's PML4 table

// Finally, define the start and end markers for the Limine requests.
// These can also be moved anywhere, to any .c file, as seen fit.

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

// Halt and catch fire function.
static void hcf(void) {
    for (;;) {
        asm ("hlt");
    }
}

void task_a() {
    while(1) {
        serial_printf("A");
        // Slow down so we don't spam serial too fast
        for(volatile int i=0; i<10000000; i++); 
    }
}

void task_b() {
    while(1) {
        serial_printf("B");
        for(volatile int i=0; i<10000000; i++);
    }
}

static void install_drivers(struct limine_memmap_response* memmap,
                             struct limine_executable_address_response* kernel_addr,
                             struct limine_hhdm_response* hhdm_response) {
    serial_init();
    gdt_init();
    idt_init();
    
    pmm_init(memmap, kernel_addr, hhdm_response);
    
    // Get current PML4 from bootloader
    uint64_t boot_cr3 = read_cr3();
    uint64_t* boot_pml4 = (uint64_t*)((boot_cr3 & PAGE_ALIGN_MASK) + hhdm_response->offset);
    
    kernel_pml4 = vmm_create_pml4();
    
    // CRITICAL: Copy all existing mappings from bootloader's PML4
    // This includes HHDM region, kernel, and any other mappings
    for (size_t i = 0; i < 512; i++) {
        if (boot_pml4[i] & VMM_PRESENT) {
            kernel_pml4[i] = boot_pml4[i];
        }
    }

    serial_printf("About to switch CR3 to %x\n", kernel_pml4);
    vmm_switch_pml4(kernel_pml4);

    heap_init();
    keyboard_init();
}


// The following will be our kernel's entry point.
// If renaming kmain() to something else, make sure to change the
// linker script accordingly.
void kmain(void) {
    // Ensure the bootloader actually understands our base revision (see spec).
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        hcf();
    }
 
    // Get the responses from the requests we made earlier needed to install drivers.
    struct limine_memmap_response* memmap = memmap_request.response;
    struct limine_executable_address_response* kernel_addr = kernel_addr_request.response;
    struct limine_hhdm_response* hhdm_response = hhdm_request.response;

    install_drivers(memmap, kernel_addr, hhdm_response);

    init_scheduler();

    //create_kernel_task(task_a);
    //create_kernel_task(task_b);
    struct limine_module_response* modules = module_request.response;

    void* psf1_font_address = NULL;
    void* psf2_font_address = NULL;

    if (modules && modules->module_count > 0) {
        struct limine_file* file = modules->modules[0];
        serial_printf("Found module: %s\n", file->path);
        
        struct limine_file* psf1_font_file = modules->modules[1];
        serial_printf("Found module: %s\n", psf1_font_file->path);

        struct limine_file* psf2_font_file = modules->modules[2];
        serial_printf("Found module: %s\n", psf2_font_file->path);

        psf1_font_address = psf1_font_file->address;
        psf2_font_address = psf2_font_file->address;

        //create_user_process(file->address);
    } else {
        serial_printf("No modules found!\n");
    }

    if (psf1_font_address && psf2_font_address) {
        graphics_init(psf1_font_address, psf2_font_address);
        fb_draw_string("Hello Graphical World!\nMultitasking Enabled.", 10, 10, 0xFFFFFF, 0x000000, USE_PSF2);
    } else {
        serial_printf("Font not found!\n");
    }
    
    pit_init(50); // 50Hz context switching
    
    serial_printf("Waiting for input...\n");
    
    while(1) {
        char c = keyboard_read_char();
        if (c != 0) {
            // If we got a character, print it!
            serial_printf("Key pressed: %c\n", c);
            
            // If you implemented the video driver from the previous step:
            // char str[2] = {c, '\0'};
            // draw_string(str, 100, 100, 0xFFFFFF, 0x000000); 
        }
    }

    // We are now the "idle" task (PID 0)
    while(1) {
        serial_printf(".");
        for(volatile int i=0; i<10000000; i++);
        asm("hlt");
    }

    // We're done, just hang...
    hcf();
}