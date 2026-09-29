#include <stdint.h>

#include "timer.h"
#include "scheduler.h"
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


/*
 * Write a byte to an I/O port.
 */
static void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}


/*
 * Read CR2.
 *
 * CR2 contains the virtual address
 * that caused the page fault.
 */
static uint32_t read_cr2(void)
{
    uint32_t address;

    __asm__ volatile (
        "mov %%cr2, %0"
        : "=r"(address)
    );

    return address;
}


/*
 * Configure one IDT entry.
 */
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


/*
 * Load the IDT.
 */
static void idt_load(void)
{
    __asm__ volatile (
        "lidt %0"
        :
        : "m"(idtp)
    );
}


/*
 * Remap the Programmable
 * Interrupt Controller.
 */
static void pic_remap(void)
{
    /*
     * Start PIC initialization.
     */
    outb(0x20, 0x11);
    outb(0xA0, 0x11);

    /*
     * Master PIC:
     *
     * IRQ0-7 -> vectors 0x20-0x27
     *
     * Slave PIC:
     *
     * IRQ8-15 -> vectors 0x28-0x2F
     */
    outb(0x21, 0x20);
    outb(0xA1, 0x28);

    /*
     * Tell master PIC that
     * slave is connected to IRQ2.
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


/*
 * Page Fault Handler
 *
 * CPU exception frame after PUSHA:
 *
 * PUSHA frame:
 *
 * stack[0]  = EDI
 * stack[1]  = ESI
 * stack[2]  = EBP
 * stack[3]  = original ESP
 * stack[4]  = EBX
 * stack[5]  = EDX
 * stack[6]  = ECX
 * stack[7]  = EAX
 *
 * CPU page-fault frame:
 *
 * stack[8]  = ERROR CODE
 * stack[9]  = EIP
 * stack[10] = CS
 * stack[11] = EFLAGS
 *
 * IMPORTANT:
 *
 * Page faults push the error code first.
 */
void page_fault_handler(uint32_t* stack)
{
    /*
     * CR2 contains the address that caused
     * the page fault.
     */
    uint32_t fault_address =
        read_cr2();


    /*
     * Correct page-fault error-code position.
     */
    uint32_t error_code =
        stack[8];


    /*
     * Get the currently running task.
     */
    uint32_t pid = 0;

    task_t* current =
        scheduler_get_current();

    if (current != 0)
    {
        pid = current->pid;
    }


    /*
     * Record the page fault in Hindsight.
     *
     * address = faulting virtual address
     * size    = 4 bytes
     * extra   = CPU page-fault error code
     */
    hindsight_event_record(
        HINDSIGHT_EVENT_PAGE_FAULT,
        pid,
        fault_address,
        4,
        error_code
    );


    /*
     * Display the captured fault information.
     */
    terminal_write(
        "\n*** BLACKFORGE PAGE FAULT ***\n"
    );

    terminal_write("PID: ");
    terminal_write_dec(pid);

    terminal_write("\nFault address: ");
    terminal_write_dec(fault_address);

    terminal_write("\nError code: ");
    terminal_write_dec(error_code);

    terminal_write(
        "\nHindsight event recorded.\n"
    );


    /*
     * Stop execution for now.
     *
     * The current BlackForge page-fault
     * policy is fatal-stop.
     *
     * Later we can replace this with
     * recovery / incident reconstruction.
     */
    __asm__ volatile ("cli");

    while (1)
    {
        __asm__ volatile ("hlt");
    }
}


/*
 * Initialize interrupt infrastructure.
 */
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
     * IRQ1 -> interrupt vector 0x21.
     */
    idt_set_gate(
        0x21,
        (uint32_t)irq1_stub
    );


    /*
     * Page fault -> interrupt vector 0x0E.
     */
    idt_set_gate(
        0x0E,
        (uint32_t)page_fault_stub
    );


    /*
     * Configure IDT pointer.
     */
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
 * Enable hardware interrupts after
 * kernel initialization is complete.
 */
void interrupts_enable(void)
{
    __asm__ volatile ("sti");
}
