bits 16
org 0x7C00

start:
    cli

    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    mov [boot_drive], dl

    ; Load kernel from sector 2
    mov ah, 0x02
    mov al, 0x0B
    mov ch, 0x00
    mov cl, 0x02
    mov dh, 0x00
    mov dl, [boot_drive]
    mov bx, 0x8000
    int 0x13

    jc disk_error

    ; Load GDT
    lgdt [gdt_descriptor]

    ; Enter protected mode
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp 0x08:protected_mode


disk_error:
    mov si, error_message

error_loop:
    lodsb
    cmp al, 0
    je $
    mov ah, 0x0E
    int 0x10
    jmp error_loop


bits 32

protected_mode:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax

    mov esp, 0x90000

    ; Display protected-mode checkpoint
    mov edi, 0xB8000
    mov esi, protected_message

print_protected:
    lodsb
    test al, al
    jz run_kernel

    mov ah, 0x07
    stosw
    jmp print_protected


run_kernel:
    mov eax, 0x8000
    call eax

kernel_halt:
    cli
    hlt
    jmp kernel_halt


bits 16

boot_drive db 0

error_message db "BLACKFORGE: Kernel load failed!", 0

protected_message db "BLACKFORGE: PROTECTED MODE OK", 0


gdt_start:

gdt_null:
    dq 0x0000000000000000

gdt_code:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10011010b
    db 11001111b
    db 0x00

gdt_data:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b
    db 11001111b
    db 0x00

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start


times 510-($-$$) db 0
dw 0xAA55
