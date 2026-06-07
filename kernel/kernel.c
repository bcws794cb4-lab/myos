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
extern void draw_string(int x, int y, const char *str, unsigned char color, unsigned char bg);
extern void fill_rect(int x, int y, int w, int h, unsigned char color);
extern void put_pixel(int x, int y, unsigned char color);
extern void mouse_init();
extern void draw_cursor(int x, int y);
extern int  net_init();
extern unsigned char *net_get_mac();

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
    idt_set(44, (unsigned int)mouse_isr);
    load_idt(&idtp);
}

void init_timer() {
    unsigned int divisor = 1193180 / 100;
    outb(0x43, 0x36);
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);
}

void byte_to_hex(unsigned char b, char *out) {
    const char hex[] = "0123456789ABCDEF";
    out[0] = hex[(b >> 4) & 0xF];
    out[1] = hex[b & 0xF];
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

    // Test pixel — bright red dot in top left
    put_pixel(10, 10, 4);
    put_pixel(11, 10, 4);
    put_pixel(12, 10, 4);
    put_pixel(10, 11, 4);
    put_pixel(11, 11, 4);
    put_pixel(12, 11, 4);

    // Network
    if (net_init()) {
        draw_string(80, 50, "NET OK!", 10, 19);
    } else {
        draw_string(80, 50, "NO NET", 12, 19);
    }

    draw_cursor(160, 100);

    while (1) {
        __asm__ __volatile__("hlt");
    }
}
