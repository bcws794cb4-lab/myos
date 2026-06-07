bits 32
global timer_isr
global keyboard_isr
global mouse_isr
global net_isr
global load_idt
extern timer_handler
extern keyboard_handler
extern mouse_handler
extern net_handler

timer_isr:
    pusha
    call timer_handler
    popa
    iret

keyboard_isr:
    pusha
    call keyboard_handler
    popa
    iret

mouse_isr:
    pusha
    call mouse_handler
    popa
    iret

net_isr:
    pusha
    call net_handler
    popa
    iret

load_idt:
    mov eax, [esp+4]
    lidt [eax]
    ret
