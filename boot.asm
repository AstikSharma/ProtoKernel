bits 32

MBOOT_PAGE_ALIGN    equ 1 << 0
MBOOT_MEM_INFO      equ 1 << 1
MBOOT_HEADER_MAGIC  equ 0x1BADB002
MBOOT_HEADER_FLAGS  equ MBOOT_PAGE_ALIGN | MBOOT_MEM_INFO
MBOOT_CHECKSUM      equ -(MBOOT_HEADER_MAGIC + MBOOT_HEADER_FLAGS)

section .multiboot
    align 4
    dd MBOOT_HEADER_MAGIC
    dd MBOOT_HEADER_FLAGS
    dd MBOOT_CHECKSUM

section .bss
    align 16
    stack_bottom:
        resb 16384 ;
    stack_top:

section .text
global _start

_start:
    mov esp, stack_top
    extern kernel_main
    call kernel_main
    cli

.hang:
    hlt
    jmp .hang

extern keyboard_handler_main

global keyboard_handler_wrapper
keyboard_handler_wrapper:
    pusha
    
    call keyboard_handler_main
    
    popa
    iret

global load_idt
load_idt:
    mov eax, [esp + 4]
    lidt [eax]
    sti
    ret

global switch_task_context
global get_esp
switch_task_context:

    push ebp
    push ebx
    push esi
    push edi

    mov eax, [esp + 20]
    mov [eax], esp

    mov eax, [esp + 24]
    mov esp, [eax]

    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

get_esp:
    mov eax, esp
    ret