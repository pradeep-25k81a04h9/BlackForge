bits 16
org 0x8000

%ifndef KERNEL_SECTORS
    %define KERNEL_SECTORS 17
%endif


stage2_start:

    cli

    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    mov [boot_drive], dl


    ; ==========================================
    ; Show Stage 2 message
    ; ==========================================

    mov si, stage2_message
    call print_string


    ; ==========================================
    ; Detect RAM using BIOS E820
    ; ==========================================

    xor ebx, ebx

    mov di, 0x5000

    mov word [memory_entries], 0


e820_next:

    mov eax, 0xE820
    mov edx, 0x534D4150
    mov ecx, 24

    int 0x15

    jc e820_done

    cmp eax, 0x534D4150
    jne e820_failed

    inc word [memory_entries]

    add di, 24

    cmp ebx, 0
    jne e820_next


e820_done:

    ; ==========================================
    ; Pass E820 information to the kernel
    ;
    ; 0x7000 = number of entries
    ; 0x7004 = pointer to entries
    ; ==========================================

    movzx eax, word [memory_entries]
    mov [0x7000], eax

    mov dword [0x7004], 0x5000

    mov si, memory_ok_message
    call print_string

    mov ax, [memory_entries]
    call print_number

    mov si, entries_message
    call print_string


    ; ==========================================
    ; Load kernel using BIOS EDD / LBA
    ;
    ; Disk layout:
    ;
    ; Sector 1       Bootloader
    ; Sector 2-4     Stage 2
    ; Sector 5+      Kernel
    ;
    ; LBA is zero-based:
    ; Bootloader = LBA 0
    ; Stage 2    = LBA 1-3
    ; Kernel     = LBA 4
    ;
    ; Kernel destination = physical 0xA000
    ; ==========================================

    mov si, kernel_message
    call print_string

    ; Check BIOS Extended Disk Drive support
    mov ah, 0x41
    mov bx, 0x55AA
    mov dl, [boot_drive]

    int 0x13

    jc kernel_load_failed

    cmp bx, 0xAA55
    jne kernel_load_failed

    test cx, 1
    jz kernel_load_failed


    ; ==========================================
    ; Prepare Disk Address Packet
    ; ==========================================

    mov word [disk_packet + 2], KERNEL_SECTORS

    mov word [disk_packet + 4], 0xA000
    mov word [disk_packet + 6], 0x0000

    ; Kernel starts at LBA 4
    mov dword [disk_packet + 8], 4
    mov dword [disk_packet + 12], 0


    ; ==========================================
    ; Read kernel
    ; ==========================================

    mov si, disk_packet
    mov dl, [boot_drive]
    mov ah, 0x42

    int 0x13

    jc kernel_load_failed


    mov si, kernel_ok_message
    call print_string


    ; ==========================================
    ; Load GDT
    ; ==========================================

    lgdt [gdt_descriptor]


    ; ==========================================
    ; Enter protected mode
    ; ==========================================

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp 0x08:protected_mode


kernel_load_failed:

    mov si, kernel_error_message
    call print_string

kernel_error_loop:

    cli
    hlt
    jmp kernel_error_loop


e820_failed:

    mov si, memory_error_message
    call print_string

memory_error_loop:

    cli
    hlt
    jmp memory_error_loop


; ==========================================
; Protected Mode
; ==========================================

bits 32

protected_mode:

    mov ax, 0x10

    mov ds, ax
    mov es, ax
    mov ss, ax

    mov esp, 0x90000


    ; ==========================================
    ; Show protected mode message
    ; ==========================================

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

    mov eax, 0xA000

    call eax


kernel_halt:

    cli
    hlt

    jmp kernel_halt


; ==========================================
; Print string
; ==========================================

bits 16

print_string:

    lodsb

    cmp al, 0

    je print_done

    mov ah, 0x0E
    mov bh, 0
    mov bl, 0x07

    int 0x10

    jmp print_string


print_done:

    ret


; ==========================================
; Print AX decimal
; ==========================================

print_number:

    xor cx, cx

    mov bx, 10


convert_number:

    xor dx, dx

    div bx

    push dx

    inc cx

    cmp ax, 0

    jne convert_number


print_digits:

    pop dx

    add dl, '0'

    mov ah, 0x0E
    mov al, dl

    int 0x10

    loop print_digits

    ret


; ==========================================
; GDT
; ==========================================

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


; ==========================================
; Variables
; ==========================================

boot_drive:
    db 0

memory_entries:
    dw 0


; ==========================================
; BIOS Extended Disk Address Packet
; ==========================================
;
; Offset 0: packet size
; Offset 1: reserved
; Offset 2: number of sectors
; Offset 4: buffer offset
; Offset 6: buffer segment
; Offset 8: starting LBA low
; Offset 12: starting LBA high
;
; ==========================================

disk_packet:

    db 0x10
    db 0x00

    dw KERNEL_SECTORS

    dw 0xA000
    dw 0x0000

    dd 0x00000004
    dd 0x00000000


; ==========================================
; Messages
; ==========================================

stage2_message:

    db 13, 10
    db "BLACKFORGE STAGE 2", 13, 10
    db "Memory detection starting...", 13, 10
    db 0


memory_ok_message:

    db "Memory detection OK - entries: ", 0


entries_message:

    db 13, 10
    db "E820 memory map stored at 0x5000", 13, 10
    db 0


kernel_message:

    db "Loading BlackForge kernel...", 13, 10
    db 0


kernel_ok_message:

    db "Kernel loaded successfully.", 13, 10
    db 0


memory_error_message:

    db "ERROR: BIOS E820 memory detection failed!", 13, 10
    db 0


kernel_error_message:

    db "ERROR: Kernel load failed!", 13, 10
    db 0


protected_message:

    db "BLACKFORGE: PROTECTED MODE OK", 0


times 1536-($-$$) db 0
