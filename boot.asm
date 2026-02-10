bits 32

section .multiboot
align 4
    dd 0x1BADB002
    dd 0x00
    dd -(0x1BADB002 + 0x00)

global start
global load_idt
extern OSmain
extern keyboard_handler_main

struc idt_entry
    .offset_low:   resw 1
    .selector:     resw 1
    .zero:         resb 1
    .flags:        resb 1
    .offset_high:  resw 1
endstruc

section .text

load_idt:
    mov eax, [esp + 4]
    lidt [eax]
    ret

keyboard_interrupt:
    pushad
    call keyboard_handler_main
    popad
    iretd

default_interrupt:
    iretd

section .data
global idt
idt:
    times 256 * 8 db 0

idt_descriptor:
    dw 256 * 8 - 1
    dd idt

start:
    cli
    mov esp, stack_top
    
    mov eax, keyboard_interrupt
    
    mov edi, idt + 33 * 8
    
    mov word [edi + idt_entry.offset_low], ax
    mov word [edi + idt_entry.selector], 0x08
    mov byte [edi + idt_entry.zero], 0
    mov byte [edi + idt_entry.flags], 0x8E
    
    shr eax, 16
    mov word [edi + idt_entry.offset_high], ax
    
    mov eax, idt_descriptor
    push eax
    call load_idt
    add esp, 4
    
    call remap_pic
    
    sti
    
    call OSmain
    
    jmp $

remap_pic:
    mov al, 0x11
    out 0x20, al
    out 0xA0, al
    
    mov al, 0x20
    out 0x21, al
    mov al, 0x28
    out 0xA1, al
    
    mov al, 0x04
    out 0x21, al
    mov al, 0x02
    out 0xA1, al
    
    mov al, 0x01
    out 0x21, al
    out 0xA1, al
    
    mov al, 0xFD
    out 0x21, al
    mov al, 0xFF
    out 0xA1, al
    
    ret

section .bss
align 16
stack_bottom:
    resb 16384
stack_top: