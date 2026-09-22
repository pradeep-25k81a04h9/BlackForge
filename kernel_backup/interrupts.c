#include <stdint.h>

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

extern void irq1_stub(void);
extern void keyboard_interrupt_handler(void);

static void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

static void idt_set_gate(int number, uint32_t handler)
{
    idt[number].offset_low = handler & 0xFFFF;
    idt[number].selector = 0x08;
    idt[number].zero = 0;
    idt[number].type_attr = 0x8E;
    idt[number].offset_high = (handler >> 16) & 0xFFFF;
}

static void idt_load(void)
{
    __asm__ volatile ("lidt %0" : : "m"(idtp));
}

static void pic_remap(void)
{
    outb(0x20, 0x11);
    outb(0xA0, 0x11);

    outb(0x21, 0x20);
    outb(0xA1, 0x28);

    outb(0x21, 0x04);
    outb(0xA1, 0x02);

    outb(0x21, 0x01);
    outb(0xA1, 0x01);

    /* Allow only keyboard IRQ1 */
    outb(0x21, 0xFD);
    outb(0xA1, 0xFF);
}

void interrupts_init(void)
{
    for (int i = 0; i < 256; i++)
    {
        idt[i].offset_low = 0;
        idt[i].selector = 0;
        idt[i].zero = 0;
        idt[i].type_attr = 0;
        idt[i].offset_high = 0;
    }

    idt_set_gate(0x21, (uint32_t)irq1_stub);

    idtp.limit = sizeof(idt) - 1;
    idtp.base = (uint32_t)&idt;

    pic_remap();
    idt_load();

    __asm__ volatile ("sti");
}
