#include "terminal.h"
#include "memory.h"
#include "paging.h"
#include "frame.h"
#include "heap.h"
#include "shell.h"
#include "task.h"
#include "scheduler.h"
#include "../hindsight/event.h"

extern void interrupts_init(void);
extern void interrupts_enable(void);
extern uint32_t timer_get_ticks(void);

void kernel_main(void)
{
    terminal_clear();

    terminal_write("========================================\n");
    terminal_write("          BLACKFORGE OS\n");
    terminal_write("========================================\n\n");

    terminal_write("Kernel status : ONLINE\n");
    terminal_write("Terminal      : ONLINE\n");
    terminal_write("Keyboard      : ONLINE\n");
    terminal_write("Interrupts    : INITIALIZING...\n\n");

    /*
     * Initialize Hindsight before any
     * instrumented kernel subsystem starts.
     */
    hindsight_event_init();

    /*
     * Initialize interrupt infrastructure.
     *
     * interrupts_init() does NOT enable
     * hardware interrupts yet.
     */
    interrupts_init();

    terminal_write("Interrupts    : ONLINE\n");

    /*
     * Initialize physical memory information.
     */
    memory_init();

    terminal_write("Memory        : ONLINE\n");

    /*
     * Initialize the physical frame allocator.
     *
     * Hindsight is already active here, so
     * frame allocation events can be recorded.
     */
    terminal_write("Frames        : INITIALIZING...\n\n");

    frame_init();

    terminal_write("Frames        : ONLINE\n\n");

    /*
     * Initialize paging.
     */
    paging_init();

    /*
     * Initialize the kernel heap.
     *
     * Hindsight is already active, so heap
     * allocation/free events can be recorded.
     */
    heap_init();

    /*
     * Initialize task management.
     */
    task_init();

    /*
     * Initialize the scheduler.
     */
    scheduler_init();

    /*
     * All core kernel subsystems are now
     * initialized.
     *
     * Hardware interrupts can safely begin.
     */
    interrupts_enable();

    terminal_write("BlackForge Shell\n");
    terminal_write("----------------\n");
    terminal_write("Type 'help' for available commands.\n\n");

    terminal_write("blackforge> ");

    while (1)
    {
        __asm__ volatile ("hlt");

        shell_process_pending();
    }
}
