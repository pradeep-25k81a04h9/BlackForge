bits 32

global _start
extern kernel_main

section .text

_start:
    cli
    mov esp, 0x90000

    call kernel_main

halt:
    cli
    hlt
    jmp halt
