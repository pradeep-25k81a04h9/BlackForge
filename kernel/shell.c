#include "timer.h"
#include "shell.h"
#include "terminal.h"
#include "memory.h"
#include "frame.h"
#include "paging.h"
#include "heap.h"
#include "task.h"
#include "scheduler.h"
#include "../hindsight/history.h"
#include "../hindsight/incident.h"
#define INPUT_BUFFER_SIZE 128

static char input_buffer[INPUT_BUFFER_SIZE];
static int input_length = 0;
static volatile int command_pending = 0;

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



static void shell_print_uint(uint32_t value)
{
    char buffer[12];
    int position = 0;

    if (value == 0)
    {
        terminal_putchar('0');
        return;
    }

    while (value > 0)
    {
        buffer[position++] = '0' + (value % 10);
        value /= 10;
    }

    while (position > 0)
    {
        terminal_putchar(buffer[--position]);
    }
}

static void shell_print_hex(uint32_t value)
{
    const char* hex = "0123456789ABCDEF";

    terminal_write("0x");

    for (int shift = 28; shift >= 0; shift -= 4)
    {
        terminal_putchar(hex[(value >> shift) & 0xF]);
    }
}

static const char* shell_event_name(uint32_t type)
{
    switch (type)
    {
        case HINDSIGHT_EVENT_HEAP_ALLOC:
            return "HEAP_ALLOC";

        case HINDSIGHT_EVENT_HEAP_FREE:
            return "HEAP_FREE";

        case HINDSIGHT_EVENT_FRAME_ALLOC:
            return "FRAME_ALLOC";

        case HINDSIGHT_EVENT_FRAME_FREE:
            return "FRAME_FREE";

        case HINDSIGHT_EVENT_TASK_CREATE:
            return "TASK_CREATE";

        case HINDSIGHT_EVENT_TASK_EXIT:
            return "TASK_EXIT";

        case HINDSIGHT_EVENT_TASK_SWITCH:
            return "TASK_SWITCH";

        case HINDSIGHT_EVENT_PAGE_FAULT:
            return "PAGE_FAULT";

        default:
            return "UNKNOWN";
    }
}

static void shell_history(void)
{
    uint32_t count = hindsight_history_count();

    terminal_write("\nBlackForge Hindsight History\n");
    terminal_write("----------------------------\n");
    terminal_write("Recorded events: ");
    shell_print_uint(count);
    terminal_write("\n\n");

    if (count == 0)
    {
        terminal_write("No Hindsight events recorded.\n\n");
        return;
    }

    for (uint32_t i = 0; i < count; i++)
    {
        const hindsight_event_t* event =
            hindsight_history_get(i);

        if (event == 0)
            continue;

        terminal_write("[");
        shell_print_uint(i);
        terminal_write("] ");

        terminal_write(shell_event_name(event->type));

        terminal_write(" seq=");
        shell_print_uint(event->sequence);

        terminal_write(" pid=");
        shell_print_uint(event->pid);

        terminal_write(" addr=");
        shell_print_hex(event->address);

        terminal_write(" size=");
        shell_print_uint(event->size);

        terminal_write(" extra=");
        shell_print_hex(event->extra);

        terminal_write("\n");
    }

    terminal_write("\nHindsight history complete.\n\n");
}
static uint32_t shell_parse_uint(const char* text)
{
    uint32_t value = 0;

    while (*text >= '0' && *text <= '9')
    {
        value = (value * 10) + (uint32_t)(*text - '0');
        text++;
    }

    return value;
}

static int shell_has_pid_argument(const char* command)
{
    return string_starts_with(command, "incident ") &&
           command[9] >= '0' &&
           command[9] <= '9';
}

static void shell_incident(const char* command)
{
    uint32_t pid = shell_parse_uint(command + 9);
    uint32_t count = hindsight_incident_count(pid);

    terminal_write("\nBlackForge Hindsight Incident\n");
    terminal_write("----------------------------\n");
    terminal_write("PID: ");
    shell_print_uint(pid);
    terminal_write("\nEvents: ");
    shell_print_uint(count);
    terminal_write("\n\n");

    if (count == 0)
    {
        terminal_write("No events found for this PID.\n\n");
        return;
    }

    for (uint32_t i = 0; i < count; i++)
    {
        const hindsight_event_t* event =
            hindsight_incident_get(pid, i);

        if (event == 0)
            continue;

        terminal_write("[");
        shell_print_uint(event->sequence);
        terminal_write("] ");
        terminal_write(shell_event_name(event->type));

        terminal_write(" addr=");
        shell_print_hex(event->address);

        terminal_write(" size=");
        shell_print_uint(event->size);

        terminal_write(" extra=");
        shell_print_hex(event->extra);

        terminal_write("\n");
    }

    terminal_write("\nIncident reconstruction complete.\n\n");
}
static void shell_help(void)
{
terminal_write("  meminfo  - Show memory information\n");
terminal_write("  frametest - Test physical frame allocator\n");
terminal_write("  timertest  - Test PIT timer\n");
terminal_write("  preempttest  - Test timer preemptive multitasking\n");
terminal_write("  tasktest  - Test task manager\n");
terminal_write("  schedtest - Test round-robin scheduler\n");
terminal_write("  switchtest   - Test context switching\n");
terminal_write("  heaptest   - Test kernel heap\n");
terminal_write("  pagemaptest - Test dynamic virtual mapping\n");
terminal_write("  pagingtest - Test virtual memory paging\n");
terminal_write("  lifecycletest - Test task lifecycle and cleanup\n");
terminal_write("  stresssched  - Stress test preemptive scheduler\n");
    terminal_write("\nAvailable commands:\n");
    terminal_write("  help     - Show this help\n");
    terminal_write("  clear    - Clear the screen\n");
    terminal_write("  about    - About BlackForge OS\n");
    terminal_write("  version  - Show OS version\n");
    terminal_write("  echo     - Print text\n");
    terminal_write("  history  - Show Hindsight event history\n");
    terminal_write("  incident <pid> - Show events for a task\n\n");
    terminal_write("  faulttest - Trigger a page fault test\n");
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

static void shell_frame_test(void)
{
    terminal_write("\nBlackForge Frame Allocator Test\n");
    terminal_write("--------------------------------\n");

    terminal_write("Initial free frames : ");

    uint32_t initial_free = frame_get_free();

    char buffer[12];
    int position = 0;

    if (initial_free == 0)
    {
        terminal_write("0\n");
        terminal_write("\nFRAME ALLOCATOR TEST: FAIL\n\n");
        return;
    }

    uint32_t value = initial_free;

    while (value > 0)
    {
        buffer[position++] = '0' + (value % 10);
        value /= 10;
    }

    while (position > 0)
    {
        terminal_putchar(buffer[--position]);
    }

    terminal_write("\n\n");

    if (frame_test())
    {
        terminal_write("Allocation test   : PASS\n");
        terminal_write("Free test         : PASS\n");
        terminal_write("Counter test      : PASS\n");
        terminal_write("\nFRAME ALLOCATOR TEST: PASS\n\n");
    }
    else
    {
        terminal_write("Allocation test   : FAIL\n");
        terminal_write("\nFRAME ALLOCATOR TEST: FAIL\n\n");
    }
}
static void shell_faulttest(void)
{
    terminal_write("\nBlackForge Page Fault Test\n");
    terminal_write("-------------------------\n");
    terminal_write("Triggering invalid memory access...\n\n");

    volatile uint32_t* invalid =
        (volatile uint32_t*)0xFFFFFFFF;

    uint32_t value = *invalid;

    /*
     * This line should never execute.
     * The page-fault handler should stop
     * the CPU before reaching here.
     */
    (void)value;

    terminal_write("ERROR: page fault did not occur.\n\n");
}
static void shell_paging_map_test(void)
{
    const uint32_t test_virtual =
        0x00F00000;

    terminal_write("\nBlackForge Dynamic Paging Test\n");
    terminal_write("--------------------------------\n");

    /*
     * Save the original physical mapping.
     *
     * Since paging currently uses identity mapping,
     * 0x00F00000 originally maps to itself.
     */
    uint32_t original_physical =
        test_virtual;

    terminal_write("Test virtual address : 0x00F00000\n");

    /*
     * Allocate a real physical frame.
     */
    uint32_t test_physical =
        frame_allocate();

    if (test_physical == 0)
    {
        terminal_write("Physical allocation : FAIL\n");
        terminal_write("\nDYNAMIC PAGING TEST: FAIL\n\n");
        return;
    }

    terminal_write("Physical frame       : ALLOCATED\n");


    /*
     * Map the virtual address to the newly
     * allocated physical frame.
     */
    if (!map_page(
            test_virtual,
            test_physical))
    {
        terminal_write("Page mapping         : FAIL\n");

        frame_free(test_physical);

        terminal_write("\nDYNAMIC PAGING TEST: FAIL\n\n");
        return;
    }

    terminal_write("Page mapping         : PASS\n");


    /*
     * Access memory through the virtual address.
     */
    volatile uint32_t* virtual_memory =
        (volatile uint32_t*)test_virtual;

    *virtual_memory =
        0xBF2026AA;

    if (*virtual_memory !=
        0xBF2026AA)
    {
        terminal_write("Virtual access       : FAIL\n");

        map_page(
            test_virtual,
            original_physical
        );

        frame_free(test_physical);

        terminal_write("\nDYNAMIC PAGING TEST: FAIL\n\n");
        return;
    }

    terminal_write("Virtual access       : PASS\n");


    /*
     * Remove the dynamic mapping.
     */
    if (!unmap_page(test_virtual))
    {
        terminal_write("Page unmapping       : FAIL\n");

        map_page(
            test_virtual,
            original_physical
        );

        frame_free(test_physical);

        terminal_write("\nDYNAMIC PAGING TEST: FAIL\n\n");
        return;
    }

    terminal_write("Page unmapping       : PASS\n");


    /*
     * Restore the original identity mapping.
     */
    if (!map_page(
            test_virtual,
            original_physical))
    {
        terminal_write("Mapping restore      : FAIL\n");

        frame_free(test_physical);

        terminal_write("\nDYNAMIC PAGING TEST: FAIL\n\n");
        return;
    }

    terminal_write("Mapping restore      : PASS\n");


    /*
     * Return the physical frame to the allocator.
     */
    frame_free(test_physical);

    terminal_write("Physical frame       : FREED\n");

    terminal_write("\nDYNAMIC PAGING TEST: PASS\n\n");
}
static void shell_timer_test(void)
{
    terminal_write("\nBlackForge Timer Test\n");
    terminal_write("---------------------\n");

    uint32_t start_ticks =
        timer_get_ticks();

    terminal_write("Starting tick count : ");

    char buffer[12];
    int position = 0;

    uint32_t value = start_ticks;

    if (value == 0)
    {
        terminal_putchar('0');
    }
    else
    {
        while (value > 0)
        {
            buffer[position++] =
                '0' + (value % 10);

            value /= 10;
        }

        while (position > 0)
        {
            terminal_putchar(
                buffer[--position]
            );
        }
    }

    terminal_write("\n");

    /*
     * Wait using a CPU loop rather than HLT.
     *
     * This prevents the test from appearing
     * completely frozen if IRQ0 is not arriving.
     */
    for (volatile uint32_t delay = 0;
         delay < 50000000;
         delay++)
    {
        __asm__ volatile ("pause");
    }

    uint32_t end_ticks =
        timer_get_ticks();

    terminal_write("Ending tick count   : ");

    position = 0;
    value = end_ticks;

    if (value == 0)
    {
        terminal_putchar('0');
    }
    else
    {
        while (value > 0)
        {
            buffer[position++] =
                '0' + (value % 10);

            value /= 10;
        }

        while (position > 0)
        {
            terminal_putchar(
                buffer[--position]
            );
        }
    }

    terminal_write("\n");

    if (end_ticks > start_ticks)
    {
        terminal_write("Timer interrupt     : PASS\n");
        terminal_write("\nTIMER TEST: PASS\n\n");
    }
    else
    {
        terminal_write("Timer interrupt     : FAIL\n");
        terminal_write("\nTIMER TEST: FAIL\n\n");
    }
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
    else if (string_equals(input_buffer, "history"))
    {
        shell_history();
    }
else if (string_equals(input_buffer, "faulttest"))
{
    shell_faulttest();
}
else if (shell_has_pid_argument(input_buffer))
{
    shell_incident(input_buffer);
}
else if (shell_has_pid_argument(input_buffer))
{
    shell_incident(input_buffer);
}
else if (string_equals(input_buffer, "pagemaptest"))
{
    shell_paging_map_test();
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
else if (string_equals(input_buffer, "schedtest"))
{
    terminal_write("\nBlackForge Scheduler Test\n");
    terminal_write("-------------------------\n");

    if (scheduler_test())
    {
        terminal_write("Task selection : PASS\n");
        terminal_write("Round robin    : PASS\n");
        terminal_write("State tracking : PASS\n");
        terminal_write("\nSCHEDULER TEST: PASS\n\n");
    }
    else
    {
        terminal_write("Scheduler test : FAIL\n");
        terminal_write("\nSCHEDULER TEST: FAIL\n\n");
    }
}
else if (string_equals(input_buffer, "timertest"))
{
    shell_timer_test();
}
else if (string_equals(input_buffer, "heaptest"))
{
    terminal_write("\nBlackForge Kernel Heap Test\n");
    terminal_write("---------------------------\n");

    terminal_write("Heap pages      : 256\n");
    terminal_write("Small allocation: ");

    if (heap_test())
    {
        terminal_write("PASS\n");
        terminal_write("\nKERNEL HEAP TEST: PASS\n\n");
    }
    else
    {
        terminal_write("FAIL\n");
        terminal_write("\nKERNEL HEAP TEST: FAIL\n\n");
    }
}
 else if (string_equals(input_buffer, "meminfo"))
{
    terminal_write("\nMEMINFO COMMAND STARTED\n");
    terminal_write("Calling memory_print_info...\n");

    memory_print_info();

    terminal_write("MEMINFO COMMAND FINISHED\n\n");
}
else if (string_equals(input_buffer, "preempttest"))
{
    terminal_write("\nBlackForge Preemptive Multitasking Test\n");
    terminal_write("---------------------------------------\n");
    terminal_write("Starting CPU-bound Task A and Task B...\n");
    terminal_write("Neither task calls task_yield().\n");
    terminal_write("Watch for A/B execution:\n\n");

    task_preempt_test();
}
else if (string_equals(input_buffer, "lifecycletest"))
{
    terminal_write("\nBlackForge Task Lifecycle Test\n");
    terminal_write("--------------------------------\n");
    terminal_write("Creating test tasks...\n");

    if (task_lifecycle_test())
    {
        terminal_write("Lifecycle test: PASS\n");
    }
    else
    {
        terminal_write("Lifecycle test: FAIL\n");
    }

    terminal_write("\n");
}
else if (string_equals(input_buffer, "stresssched"))
{
    terminal_write("\nBlackForge Scheduler Stress Test\n");
    terminal_write("--------------------------------\n");
    terminal_write("Creating Tasks A, B and C...\n");
    terminal_write("Starting preemptive scheduler...\n\n");

    if (task_scheduler_stress_test())
    {
        terminal_write("\nScheduler stress test: PASS\n");
    }
    else
    {
        terminal_write("\nScheduler stress test: FAIL\n");
    }

    terminal_write("\n");
}
else if (string_equals(input_buffer, "frametest"))
{
    shell_frame_test();
}
else if (string_equals(input_buffer, "pagingtest"))
{
    terminal_write("\nBlackForge Paging Test\n");
    terminal_write("----------------------\n");

    if (paging_test())
    {
        terminal_write("Page directory : PASS\n");
        terminal_write("Page tables    : PASS\n");
        terminal_write("Identity maps  : PASS\n");
        terminal_write("\nPAGING TEST: PASS\n\n");
    }
    else
    {
        terminal_write("Page directory : FAIL\n");
        terminal_write("\nPAGING TEST: FAIL\n\n");
    }
}
else if (string_equals(input_buffer, "tasktest"))
{
    terminal_write("\nBlackForge Task Manager Test\n");
    terminal_write("----------------------------\n");

    if (task_test())
    {
        terminal_write("Task creation : PASS\n");
        terminal_write("Task stacks   : PASS\n");
        terminal_write("Task cleanup  : PASS\n");
        terminal_write("\nTASK MANAGER TEST: PASS\n\n");
    }
    else
    {
        terminal_write("Task manager test : FAIL\n");
        terminal_write("\nTASK MANAGER TEST: FAIL\n\n");
    }
}
else if (string_equals(input_buffer, "switchtest"))
{
    terminal_write("\nBlackForge Context Switch Test\n");
    terminal_write("------------------------------\n");

    terminal_write("Starting Task A and Task B...\n");

    if (task_switch_test())
    {
        terminal_write("Task A execution : PASS\n");
        terminal_write("Task B execution : PASS\n");
        terminal_write("Context switching: PASS\n");
        terminal_write("Task termination : PASS\n");
        terminal_write("\nCONTEXT SWITCH TEST: PASS\n\n");
    }
    else
    {
        terminal_write("Context switch test : FAIL\n");
        terminal_write("\nCONTEXT SWITCH TEST: FAIL\n\n");
    }
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
    command_pending = 1;
}


void shell_process_pending(void)
{
    if (command_pending)
    {
        command_pending = 0;
        shell_execute();
    }
}
