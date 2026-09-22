bits 32

global irq1_stub
extern keyboard_interrupt_handler

irq1_stub:
    pusha

    call keyboard_interrupt_handler

    popa
    iretd
