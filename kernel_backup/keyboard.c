#include "keyboard.h"
#include "shell.h"
#include "terminal.h"


static int shift_pressed = 0;


static unsigned char inb(unsigned short port)
{
    unsigned char result;

    __asm__ volatile (
        "inb %1, %0"
        : "=a"(result)
        : "Nd"(port)
    );

    return result;
}


static void outb(unsigned short port, unsigned char value)
{
    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}


static char keyboard_translate(unsigned char scan_code)
{
    static const char normal[] =
        "1234567890-=";

    static const char shifted[] =
        "!@#$%^&*()_+";


    if (scan_code >= 0x02 && scan_code <= 0x0D)
    {
        int index = scan_code - 0x02;

        if (shift_pressed)
        {
            return shifted[index];
        }

        return normal[index];
    }


    switch (scan_code)
    {
        case 0x10: return shift_pressed ? 'Q' : 'q';
        case 0x11: return shift_pressed ? 'W' : 'w';
        case 0x12: return shift_pressed ? 'E' : 'e';
        case 0x13: return shift_pressed ? 'R' : 'r';
        case 0x14: return shift_pressed ? 'T' : 't';
        case 0x15: return shift_pressed ? 'Y' : 'y';
        case 0x16: return shift_pressed ? 'U' : 'u';
        case 0x17: return shift_pressed ? 'I' : 'i';
        case 0x18: return shift_pressed ? 'O' : 'o';
        case 0x19: return shift_pressed ? 'P' : 'p';

        case 0x1E: return shift_pressed ? 'A' : 'a';
        case 0x1F: return shift_pressed ? 'S' : 's';
        case 0x20: return shift_pressed ? 'D' : 'd';
        case 0x21: return shift_pressed ? 'F' : 'f';
        case 0x22: return shift_pressed ? 'G' : 'g';
        case 0x23: return shift_pressed ? 'H' : 'h';
        case 0x24: return shift_pressed ? 'J' : 'j';
        case 0x25: return shift_pressed ? 'K' : 'k';
        case 0x26: return shift_pressed ? 'L' : 'l';

        case 0x2C: return shift_pressed ? 'Z' : 'z';
        case 0x2D: return shift_pressed ? 'X' : 'x';
        case 0x2E: return shift_pressed ? 'C' : 'c';
        case 0x2F: return shift_pressed ? 'V' : 'v';
        case 0x30: return shift_pressed ? 'B' : 'b';
        case 0x31: return shift_pressed ? 'N' : 'n';
        case 0x32: return shift_pressed ? 'M' : 'm';

        case 0x39: return ' ';

        default:
            return 0;
    }
}


void keyboard_interrupt_handler(void)
{
    unsigned char scan_code = inb(0x60);


    if (scan_code == 0x2A || scan_code == 0x36)
    {
        shift_pressed = 1;
    }
    else if (scan_code == 0xAA || scan_code == 0xB6)
    {
        shift_pressed = 0;
    }
    else if (scan_code == 0x1C)
    {
        shell_enter();
    }
    else if (scan_code == 0x0E)
    {
        shell_backspace();
    }
    else if (!(scan_code & 0x80))
    {
        char character = keyboard_translate(scan_code);

        if (character != 0)
        {
            shell_input_character(character);
        }
    }


    /* End Of Interrupt */

    outb(0x20, 0x20);
}
