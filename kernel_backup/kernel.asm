bits 16
org 0x8000

start:
    mov si, message

print_loop:
    lodsb
    cmp al, 0
    je done

    mov ah, 0x0E
    int 0x10
    jmp print_loop

done:
    cli
    hlt
    jmp done

message db "BLACKFORGE KERNEL - Running!", 0

times 512-($-$$) db 0
