#include "terminal.h"

extern void interrupts_init(void);


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

    terminal_write("Interrupts    : ONLINE\n\n");

    terminal_write("BlackForge Shell\n");
    terminal_write("----------------\n");
    terminal_write("Type 'help' for available commands.\n\n");

    terminal_write("blackforge> ");


    while (1)
    {
        __asm__ volatile ("hlt");
    }
}
