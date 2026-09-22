#ifndef TERMINAL_H
#define TERMINAL_H

void terminal_clear(void);
void terminal_putchar(char character);
void terminal_write(const char* text);
void terminal_write_dec(unsigned int value);

#endif
