unsigned int ticks = 0;

static inline unsigned char inb(unsigned short port) {
    unsigned char val;
    __asm__ __volatile__("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static inline void outb(unsigned short port, unsigned char val) {
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

extern void mem_init();
extern void fs_init();
extern void task_init();
extern void task_switch();
extern void draw_desktop();
extern void gfx_update_clock(unsigned int ticks);
extern void mouse_init();
extern void draw_cursor(int x, int y);

void timer_handler() {
    ticks++;
    gfx_update_clock(ticks);
    task_switch();
    outb(0x20, 0x20);
}

void keyboard_handler() {
    inb(0x60);
    outb(0x20, 0x20);
}

struct idt_entry {
    unsigned short base_low;
    unsigned short sel;
    unsigned char zero;
    unsigned char flags;
    unsigned short base_high;
} __attribute__((packed));

struct idt_ptr {
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));

struct idt_entry idt[256];
struct idt_ptr idtp;

extern void timer_isr();
extern void keyboard_isr();
extern void mouse_isr();
extern void load_idt(struct idt_ptr *);

void idt_set(unsigned char num, unsigned int base) {
    idt[num].base_low  = base & 0xFFFF;
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].sel = 0x08; idt[num].zero = 0; idt[num].flags = 0x8E;
}

void init_idt() {
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (unsigned int)&idt;
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);
    outb(0x21, 0x00); outb(0xA1, 0x00);
    idt_set(32, (unsigned int)timer_isr);
    idt_set(33, (unsigned int)keyboard_isr);
    idt_set(44, (unsigned int)mouse_isr);  // IRQ12 = interrupt 44
    load_idt(&idtp);
}

void init_timer() {
    unsigned int divisor = 1193180 / 100;
    outb(0x43, 0x36);
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);
}

void kernel_main() {
    mem_init();
    fs_init();
    task_init();
    init_idt();
    init_timer();
    mouse_init();
    __asm__ __volatile__("sti");

    draw_desktop();
    draw_cursor(160, 100);

    while (1) {
        __asm__ __volatile__("hlt");
    }
}
