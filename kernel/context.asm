bits 32

global context_switch
global context_start
global context_return_to_stack

extern kernel_base_pointer

section .text


; ============================================================
; Cooperative context switch
; ============================================================

context_switch:
    push ebp
    push ebx
    push esi
    push edi

    mov eax, [esp + 20]
    mov [eax], esp

    mov esp, [esp + 24]

    pop edi
    pop esi
    pop ebx
    pop ebp

    ret


; ============================================================
; Start a preemptive task
;
; [esp + 4] = address of kernel_stack_pointer
; [esp + 8] = task IRQ stack
; ============================================================

context_start:
    cli

    ; Save the CURRENT kernel ESP value.
    mov eax, [esp + 4]
    mov [eax], esp

    ; Save the kernel EBP.
    mov eax, ebp
    mov [kernel_base_pointer], eax

    ; Switch to the task's prepared IRQ stack.
    mov esp, [esp + 8]

    ; Restore task registers.
    popa

    ; Restore EIP, CS and EFLAGS.
    iretd


; ============================================================
; Return from idle to the saved kernel context
;
; Argument is the ACTUAL saved ESP value.
; ============================================================

context_return_to_stack:
    cli

    ; [esp + 4] already contains the saved ESP value.
    mov esp, [esp + 4]

    ; Restore the kernel frame pointer.
    mov ebp, [kernel_base_pointer]

    ; ESP now points to the return address that was
    ; saved when context_start() was called.
    ret
