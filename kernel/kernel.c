#include "../hindsight/event.h"
#include "../hindsight/history.h"
#include "terminal.h"
#include "memory.h"
#include "paging.h"
#include "frame.h"
#include "heap.h"
#include "shell.h"
#include "task.h"
#include "scheduler.h"
#include "storage/storage.h"
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

    interrupts_init();

    terminal_write("Interrupts    : ONLINE\n");

    memory_init();
    terminal_write("Memory        : ONLINE\n");
    terminal_write("Frames        : INITIALIZING...\n\n");
    frame_init();
    terminal_write("Frames        : ONLINE\n\n");
    paging_init();
    heap_init();
    task_init();
    scheduler_init();
  
storage_init();

hindsight_history_init();

if (hindsight_history_load() == 0)
{
    hindsight_set_timestamp(
        hindsight_history_last_timestamp()
    );
}
  /*
 * All core kernel subsystems are now initialized.
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
