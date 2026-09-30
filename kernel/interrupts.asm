bits 32

global irq0_stub
global irq1_stub
global page_fault_stub

extern timer_interrupt_handler
extern keyboard_interrupt_handler
extern page_fault_handler

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
; ------------------------------------------------
; Page Fault - CPU Exception 14
; ------------------------------------------------

page_fault_stub:

    ; Save all general-purpose registers.
    pusha

    ; After PUSHA:
    ;
    ; ESP -> EDI
    ;        ESI
    ;        EBP
    ;        ESP (original)
    ;        EBX
    ;        EDX
    ;        ECX
    ;        EAX
    ;        error code
    ;        EIP
    ;        CS
    ;        EFLAGS
    ;
    ; Pass the page-fault error code to C.
    mov eax, [esp + 32]
    push eax

    call page_fault_handler

    add esp, 4

    ; Restore registers.
    popa

    ; Remove the CPU-pushed page-fault error code.
    add esp, 4

    ; Return from exception.
    iretd
