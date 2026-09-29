#include <stdint.h>
#include "timer.h"
#include "terminal.h"
#include "../hindsight/event.h"

struct idt_entry
{
    uint16_t offset_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t type_attr;
    uint16_t offset_high;
} __attribute__((packed));

struct idt_ptr
{
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr idtp;

extern void irq0_stub(void);
extern void irq1_stub(void);
extern void page_fault_stub(void);

static volatile int page_fault_test_mode = 0;

void page_fault_test_enable(void)
{
    page_fault_test_mode = 1;
}

void page_fault_handler(uint32_t error_code)
{
    uint32_t fault_address;

    __asm__ volatile (
        "mov %%cr2, %0"
        : "=r"(fault_address)
    );

    hindsight_record_event(
        HINDSIGHT_PAGE_FAULT,
        HINDSIGHT_ERROR,
        -1,
        fault_address,
        error_code,
        "Page fault"
    );

    if (page_fault_test_mode)
    {
        terminal_write("\nHINDSIGHT: PAGE FAULT CAPTURED\n");
        terminal_write("BlackForge halted safely.\n");

        while (1)
        {
            __asm__ volatile ("cli; hlt");
        }
    }
}

static void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

static void idt_set_gate(
    int number,
    uint32_t handler)
{
    idt[number].offset_low =
        handler & 0xFFFF;

    idt[number].selector = 0x08;
    idt[number].zero = 0;
    idt[number].type_attr = 0x8E;

    idt[number].offset_high =
        (handler >> 16) & 0xFFFF;
}

static void idt_load(void)
{
    __asm__ volatile (
        "lidt %0"
        :
        : "m"(idtp)
    );
}

static void pic_remap(void)
{
    /*
     * Start PIC initialization.
     */
    outb(0x20, 0x11);
    outb(0xA0, 0x11);

    /*
     * Master PIC:
     * IRQ0-7 -> vectors 0x20-0x27
     *
     * Slave PIC:
     * IRQ8-15 -> vectors 0x28-0x2F
     */
    outb(0x21, 0x20);
    outb(0xA1, 0x28);

    /*
     * Tell master PIC that slave is
     * connected to IRQ2.
     */
    outb(0x21, 0x04);
    outb(0xA1, 0x02);

    /*
     * 8086 mode.
     */
    outb(0x21, 0x01);
    outb(0xA1, 0x01);

    /*
     * Enable:
     *
     * IRQ0 = PIT timer
     * IRQ1 = keyboard
     *
     * Disable remaining master IRQs.
     */
    outb(0x21, 0xFC);

    /*
     * Disable all slave IRQs.
     */
    outb(0xA1, 0xFF);
}

void interrupts_init(void)
{
    /*
     * Clear all IDT entries.
     */
    for (int i = 0; i < 256; i++)
    {
        idt[i].offset_low = 0;
        idt[i].selector = 0;
        idt[i].zero = 0;
        idt[i].type_attr = 0;
        idt[i].offset_high = 0;
    }

    /*
     * IRQ0 -> interrupt vector 0x20.
     */
    idt_set_gate(
        0x20,
        (uint32_t)irq0_stub
    );

    /*
     * CPU Page Fault -> interrupt vector 14.
     */
    idt_set_gate(
        0x0E,
        (uint32_t)page_fault_stub
    );

    /*
     * IRQ1 -> interrupt vector 0x21.
     */
    idt_set_gate(
        0x21,
        (uint32_t)irq1_stub
    );

    idtp.limit =
        sizeof(idt) - 1;

    idtp.base =
        (uint32_t)&idt;

    /*
     * Configure PIC.
     */
    pic_remap();

    /*
     * Configure PIT.
     */
    timer_init();

    /*
     * Load IDT.
     */
    idt_load();

    /*
     * IMPORTANT:
     *
     * Do NOT execute STI here.
     *
     * The kernel still needs to initialize:
     *
     * memory
     * frames
     * paging
     * heap
     * tasks
     * scheduler
     */
}

/*
 * Enable hardware interrupts after the
 * kernel has completely initialized.
 */
void interrupts_enable(void)
{
    __asm__ volatile ("sti");
}
