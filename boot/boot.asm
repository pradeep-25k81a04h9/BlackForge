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

    ; Load Stage 2
    ; Stage 2 occupies sectors 2-4
    mov ah, 0x02
    mov al, 0x03
    mov ch, 0x00
    mov cl, 0x02
    mov dh, 0x00
    mov dl, [boot_drive]

    mov bx, 0x8000

    int 0x13
    jc disk_error

    mov dl, [boot_drive]

    jmp 0x0000:0x8000


disk_error:
    mov si, error_message

error_loop:
    lodsb

    cmp al, 0
    je $

    mov ah, 0x0E
    int 0x10

    jmp error_loop


boot_drive:
    db 0

error_message:
    db "BLACKFORGE: Stage 2 load failed!", 0


times 510-($-$$) db 0
dw 0xAA55
