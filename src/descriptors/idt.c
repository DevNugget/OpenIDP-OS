#include <descriptors/idt.h>
#include <descriptors/gdt.h>
#include <drivers/com1.h>
#include <drivers/apic.h>
#include <drivers/keyboard.h>
#include <drivers/mouse.h>
#include <utility/cpu_state.h>
#include <multitasking/scheduler.h>
#include <syscall/syscall.h>

#define DESCRIPTOR_BYTES 16
#define IDT_SIZE 256

extern char vector_0_handler[];

int_descriptor_t idt[IDT_SIZE];

static inline uint64_t read_cr2(void) {
    uint64_t value;
    asm volatile ("mov %%cr2, %0" : "=r"(value));
    return value;
}

static inline uint64_t read_cr3(void) {
    uint64_t value;
    asm volatile ("mov %%cr3, %0" : "=r"(value));
    return value;
}

static void log_fault_context(const char* fault_name, cpu_status_t* context) {
    thread_t* current = scheduler_current_thread();

    serial_printf("[%s] cpu=%u vector=%u err=0x%x rip=0x%x rsp=0x%x cs=0x%x ss=0x%x rflags=0x%x cr2=0x%x cr3=0x%x\n",
                  fault_name,
                  apic_get_id(),
                  (uint32_t)context->vector_number,
                  context->error_code,
                  context->iret_rip,
                  context->iret_rsp,
                  context->iret_cs,
                  context->iret_ss,
                  context->iret_flags,
                  read_cr2(),
                  read_cr3());

    serial_printf("[%s] regs rax=0x%x rbx=0x%x rcx=0x%x rdx=0x%x rsi=0x%x rdi=0x%x rbp=0x%x\n",
                  fault_name,
                  context->rax,
                  context->rbx,
                  context->rcx,
                  context->rdx,
                  context->rsi,
                  context->rdi,
                  context->rbp);

    if (current != NULL && current->parent != NULL) {
        serial_printf("[%s] thread tid=%u status=%u user=%u process pid=%u name=%s\n",
                      fault_name,
                      (uint32_t)current->tid,
                      (uint32_t)current->status,
                      (uint32_t)current->is_user_thread,
                      (uint32_t)current->parent->pid,
                      current->parent->name);
    }
}

static void log_page_fault_details(cpu_status_t* context) {
    uint64_t error = context->error_code;

    serial_printf("[PAGE_FAULT] decode present=%u write=%u user=%u reserved=%u instr_fetch=%u protection_key=%u shadow_stack=%u sgx=%u\n",
                  (uint32_t)(error & 0x1),
                  (uint32_t)((error >> 1) & 0x1),
                  (uint32_t)((error >> 2) & 0x1),
                  (uint32_t)((error >> 3) & 0x1),
                  (uint32_t)((error >> 4) & 0x1),
                  (uint32_t)((error >> 5) & 0x1),
                  (uint32_t)((error >> 6) & 0x1),
                  (uint32_t)((error >> 15) & 0x1));
}

static __attribute__((noreturn)) void panic_halt(void) {
    for (;;) {
        asm ("cli; hlt");
    }
}

void load_idt(void* idt_addr) {
    idtr_t idt_reg;
    idt_reg.limit = (DESCRIPTOR_BYTES * IDT_SIZE) - 1;
    idt_reg.base = (uint64_t)idt_addr;
    asm volatile("lidt %0" :: "m"(idt_reg));
}

void set_idt_entry(uint8_t vector, void* handler, uint8_t dpl) {
    uint64_t handler_addr = (uint64_t)handler;
    int_descriptor_t* entry = &idt[vector];
    
    entry->address_low = handler_addr & 0xFFFF;
    entry->address_mid = (handler_addr >> 16) & 0xFFFF;
    entry->address_high = handler_addr >> 32;
    
    entry->selector = KERNEL_CODE;
    
    entry->flags = IDT_INTERRUPT_GATE | IDT_DPL(dpl) | IDT_PRESENT;
    
    entry->ist = 0; // Disable
}

void idt_init() {
    load_idt(idt);
    
    for (int i = 0; i < IDT_SIZE; i++) {
        uint8_t dpl = 0;

        if (i == 0x80) {
            dpl = 3;
        }

        set_idt_entry(i, vector_0_handler + (i * DESCRIPTOR_BYTES), dpl);
    }
    
    asm volatile ("sti");
}

void idt_init_cpu(void) {
    load_idt(idt);
    asm volatile ("sti");
}

cpu_status_t* interrupt_dispatch(cpu_status_t* context) {
    cpu_status_t* ctx = context;
    switch (context->vector_number) {
        case 13: {
            log_fault_context("GP_FAULT", context);
            panic_halt();
            break;
        }
        case 14: {
            log_fault_context("PAGE_FAULT", context);
            log_page_fault_details(context);
            panic_halt();
            break;
        }

        case 0x80: {
            ctx = syscall_dispatch(context);
            break;
        }
        
        case 0xFF: {
            serial_write_str("spurious interrupt.\n");
            break;
        }

        case 0x20: {
            ctx = schedule(context);
            inc_uptime();
            break;
        }

        case 0x21: {
            keyboard_driver_irq_handler();
            break;
        }

        case 0x2C: {
            mouse_driver_irq_handler();
            break;
        }
        
        default: {
            serial_write_str("unexpected interrupt.\n");
            break;
        }
    }

    if (context->vector_number >= 32 && context->vector_number != 0x80) apic_eoi();
    return ctx;
}