#include "shell.h"
#include "terminal.h"

#define INPUT_BUFFER_SIZE 128

static char input_buffer[INPUT_BUFFER_SIZE];
static int input_length = 0;


static int string_equals(const char* a, const char* b)
{
    int i = 0;

    while (a[i] != '\0' && b[i] != '\0')
    {
        if (a[i] != b[i])
        {
            return 0;
        }

        i++;
    }

    return a[i] == '\0' && b[i] == '\0';
}


static int string_starts_with(const char* text, const char* prefix)
{
    int i = 0;

    while (prefix[i] != '\0')
    {
        if (text[i] != prefix[i])
        {
            return 0;
        }

        i++;
    }

    return 1;
}


static void shell_help(void)
{
    terminal_write("\nAvailable commands:\n");
    terminal_write("  help     - Show this help\n");
    terminal_write("  clear    - Clear the screen\n");
    terminal_write("  about    - About BlackForge OS\n");
    terminal_write("  version  - Show OS version\n");
    terminal_write("  echo     - Print text\n\n");
}


static void shell_about(void)
{
    terminal_write("\nBlackForge OS\n");
    terminal_write("------------------------------\n");
    terminal_write("A custom operating system.\n");
    terminal_write("Architecture : x86 32-bit\n");
    terminal_write("Kernel       : BlackForge Kernel\n");
    terminal_write("Terminal     : VGA Text Mode\n");
    terminal_write("Input        : PS/2 Keyboard\n");
    terminal_write("Interrupts   : IRQ1\n\n");
}


static void shell_version(void)
{
    terminal_write("\nBlackForge OS version 0.1\n");
    terminal_write("Kernel version 0.1.0\n\n");
}


static void shell_echo(const char* command)
{
    terminal_write("\n");
    terminal_write(command + 5);
    terminal_write("\n\n");
}


static void shell_execute(void)
{
    if (input_length == 0)
    {
        terminal_putchar('\n');
        terminal_write("blackforge> ");
        return;
    }


    if (string_equals(input_buffer, "help"))
    {
        shell_help();
    }
    else if (string_equals(input_buffer, "clear"))
    {
        terminal_clear();
    }
    else if (string_equals(input_buffer, "about"))
    {
        shell_about();
    }
    else if (string_equals(input_buffer, "version"))
    {
        shell_version();
    }
    else if (string_starts_with(input_buffer, "echo "))
    {
        shell_echo(input_buffer);
    }
    else
    {
        terminal_write("\nUnknown command: ");
        terminal_write(input_buffer);
        terminal_write("\nType 'help' for available commands.\n\n");
    }


    input_length = 0;
    input_buffer[0] = '\0';

    terminal_write("blackforge> ");
}


void shell_input_character(char character)
{
    if (input_length < INPUT_BUFFER_SIZE - 1)
    {
        input_buffer[input_length] = character;
        input_length++;

        input_buffer[input_length] = '\0';

        terminal_putchar(character);
    }
}


void shell_backspace(void)
{
    if (input_length > 0)
    {
        input_length--;

        input_buffer[input_length] = '\0';

        terminal_putchar('\b');
    }
}


void shell_enter(void)
{
    shell_execute();
}
