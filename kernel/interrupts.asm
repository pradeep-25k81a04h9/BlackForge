bits 32

global irq0_stub
global irq1_stub

extern timer_interrupt_handler
extern keyboard_interrupt_handler

section .text

; ------------------------------------------------
; IRQ0 - PIT timer
; ------------------------------------------------

irq0_stub:

    ; Save all general-purpose registers.
    pusha

    ; Pass the current stack pointer to C.
    ;
    ; Stack at this point:
    ;
    ; ESP -> EDI
    ;        ESI
    ;        EBP
    ;        ESP (original)
    ;        EBX
    ;        EDX
    ;        ECX
    ;        EAX
    ;        CPU interrupt frame
    ;
    push esp

    call timer_interrupt_handler

    add esp, 4

    ; EAX contains the stack pointer that
    ; should be restored.
    mov esp, eax

    ; Restore registers from selected task.
    popa

    ; Return from hardware interrupt.
    iretd


; ------------------------------------------------
; IRQ1 - Keyboard
; ------------------------------------------------

irq1_stub:

    pusha

    call keyboard_interrupt_handler

    popa

    iretd
