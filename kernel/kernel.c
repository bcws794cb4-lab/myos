unsigned int ticks = 0;
static int arp_sent = 0;

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
extern void mouse_init();
extern void draw_cursor(int x, int y);
extern int  net_init();
extern unsigned char *net_get_mac();
extern void arp_request(unsigned int target_ip);
extern int  arp_is_resolved();
extern unsigned char *arp_get_gateway_mac();
extern void arp_handle(unsigned char *packet, unsigned short len);
extern unsigned char *net_get_last_packet();
extern unsigned short net_get_last_len();

void timer_handler() {
    ticks++;
    gfx_update_clock(ticks);

    // Send ARP request after 1 second
    if (ticks == 100 && !arp_sent) {
        arp_request(0x0A000202); // 10.0.2.2
        arp_sent = 1;
    }

    // Check for ARP reply
    if (arp_sent && !arp_is_resolved()) {
        unsigned short len = net_get_last_len();
        if (len > 0) {
            arp_handle(net_get_last_packet(), len);
            if (arp_is_resolved()) {
                unsigned char *gmac = arp_get_gateway_mac();
                // Show gateway MAC on screen
                fill_rect(60, 60, 200, 10, 19);
                draw_string(60, 60, "GW MAC:", 10, 19);
                // Show first 3 bytes
                const char hex[] = "0123456789ABCDEF";
                char m[3];
                for (int i = 0; i < 6; i++) {
                    m[0] = hex[(gmac[i]>>4)&0xF];
                    m[1] = hex[gmac[i]&0xF];
                    m[2] = 0;
                    draw_string(100 + i*18, 60, m, 15, 19);
                }
            }
        }
    }

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

void kernel_main() {
    mem_init();
    fs_init();
    task_init();
    init_idt();
    init_timer();
    mouse_init();
    __asm__ __volatile__("sti");

    draw_desktop();

    if (net_init()) {
        draw_string(80, 50, "NET: RTL8139 OK!", 10, 19);
        draw_string(80, 60, "Sending ARP...", 14, 19);
    } else {
        draw_string(80, 50, "NET: No card found", 12, 19);
    }

    draw_cursor(160, 100);

    while (1) {
        __asm__ __volatile__("hlt");
    }
}
