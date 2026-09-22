#define VGA_WIDTH 80
#define VGA_HEIGHT 25

static volatile unsigned short* const VGA_MEMORY =
    (unsigned short*)0xB8000;

static int cursor_row = 0;
static int cursor_column = 0;


static void terminal_scroll(void)
{
    for (int row = 1; row < VGA_HEIGHT; row++)
    {
        for (int column = 0; column < VGA_WIDTH; column++)
        {
            VGA_MEMORY[
                (row - 1) * VGA_WIDTH + column
            ] =
                VGA_MEMORY[
                    row * VGA_WIDTH + column
                ];
        }
    }

    for (int column = 0; column < VGA_WIDTH; column++)
    {
        VGA_MEMORY[
            (VGA_HEIGHT - 1) * VGA_WIDTH + column
        ] =
            (unsigned short)' ' |
            ((unsigned short)0x07 << 8);
    }

    cursor_row = VGA_HEIGHT - 1;
    cursor_column = 0;
}


void terminal_clear(void)
{
    for (int row = 0; row < VGA_HEIGHT; row++)
    {
        for (int column = 0; column < VGA_WIDTH; column++)
        {
            VGA_MEMORY[
                row * VGA_WIDTH + column
            ] =
                (unsigned short)' ' |
                ((unsigned short)0x07 << 8);
        }
    }

    cursor_row = 0;
    cursor_column = 0;
}


void terminal_putchar(char character)
{
    if (character == '\n')
    {
        cursor_column = 0;
        cursor_row++;

        if (cursor_row >= VGA_HEIGHT)
        {
            terminal_scroll();
        }

        return;
    }


    if (character == '\b')
    {
        if (cursor_column > 0)
        {
            cursor_column--;

            VGA_MEMORY[
                cursor_row * VGA_WIDTH + cursor_column
            ] =
                (unsigned short)' ' |
                ((unsigned short)0x07 << 8);
        }

        return;
    }


    VGA_MEMORY[
        cursor_row * VGA_WIDTH + cursor_column
    ] =
        (unsigned short)character |
        ((unsigned short)0x07 << 8);

    cursor_column++;


    if (cursor_column >= VGA_WIDTH)
    {
        cursor_column = 0;
        cursor_row++;

        if (cursor_row >= VGA_HEIGHT)
        {
            terminal_scroll();
        }
    }
}


void terminal_write(const char* text)
{
    for (int i = 0; text[i] != '\0'; i++)
    {
        terminal_putchar(text[i]);
    }
}


/*
 * Write an unsigned integer in decimal format.
 *
 * Example:
 * terminal_write_dec(123);
 *
 * Output:
 * 123
 */
void terminal_write_dec(unsigned int value)
{
    char buffer[10];
    int index = 0;

    if (value == 0)
    {
        terminal_putchar('0');
        return;
    }

    while (value > 0)
    {
        buffer[index++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (index > 0)
    {
        terminal_putchar(buffer[--index]);
    }
}
