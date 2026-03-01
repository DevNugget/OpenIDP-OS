#include <multitasking/smp.h>

#include <descriptors/gdt.h>
#include <descriptors/idt.h>

#include <drivers/apic.h>
#include <drivers/com1.h>

#include <limine.h>

__attribute__((used, section(".limine_requests")))
static volatile struct limine_mp_request smp_mp_request = {
    .id = LIMINE_MP_REQUEST_ID,
    .revision = 0
};

static volatile uint64_t smp_online_cpus = 1;
static volatile size_t smp_cpu_count = 1;

static void smp_ap_entry(struct limine_mp_info* info) {
    size_t cpu_index = (size_t)info->extra_argument;

    gdt_init_cpu(cpu_index);
    
    idt_init_cpu();
    apic_enable_local();
    apic_timer_start(10);

    __atomic_add_fetch(&smp_online_cpus, 1, __ATOMIC_SEQ_CST);

    for (;;) {
        asm volatile ("hlt");
    }
}

void smp_init(void) {
    struct limine_mp_response* response = smp_mp_request.response;
    if (response == NULL || response->cpu_count <= 1 || response->cpus == NULL) {
        serial_write_str("[SMP] single-core boot or no MP response\n");
        smp_cpu_count = 1;
        return;
    }

    smp_cpu_count = (size_t)response->cpu_count;
    uint64_t target_cpus = response->cpu_count;
    for (uint64_t i = 0; i < response->cpu_count; i++) {
        struct limine_mp_info* cpu = response->cpus[i];
        if (cpu == NULL) {
            continue;
        }

        if (cpu->lapic_id == response->bsp_lapic_id) {
            continue;
        }

        cpu->extra_argument = i;
        cpu->goto_address = smp_ap_entry;
    }

    while (__atomic_load_n(&smp_online_cpus, __ATOMIC_SEQ_CST) < target_cpus) {
        asm volatile ("pause");
    }

    serial_printf("[SMP] online CPUs: %u\n", (uint32_t)smp_online_cpus);
}

size_t smp_get_cpu_count(void) {
    return smp_cpu_count;
}
