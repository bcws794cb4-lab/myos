bits 32
global timer_isr
global load_idt
extern timer_handler

timer_isr:
    pusha
    call timer_handler
    popa
    iret

load_idt:
    mov eax, [esp+4]
    lidt [eax]
    ret
