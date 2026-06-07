unsigned short *vga = (unsigned short *)0xB8000;
int cur_row = 4;
int cur_col = 2;
char cmd_buf[80];
int cmd_len = 0;
int shift_pressed = 0;
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
extern void *malloc(unsigned int size);
extern void free(void *ptr);
extern unsigned int mem_used();
extern unsigned int mem_free();

void clear_screen() {
    for (int i = 0; i < 80 * 25; i++)
        vga[i] = 0x0720;
}

void print_at(const char *str, int row, int col, unsigned char color) {
    int i = 0;
    while (str[i]) {
        vga[row * 80 + col + i] = (unsigned short)((color << 8) | (unsigned char)str[i]);
        i++;
    }
}

void print_int(unsigned int n, int row, int col, unsigned char color) {
    char buf[12];
    int i = 11;
    buf[i] = 0;
    if (n == 0) { buf[--i] = '0'; }
    while (n > 0) { buf[--i] = '0' + (n % 10); n /= 10; }
    print_at(buf + i, row, col, color);
}

void scroll() {
    for (int row = 5; row < 25; row++)
        for (int col = 0; col < 80; col++)
            vga[(row-1) * 80 + col] = vga[row * 80 + col];
    for (int col = 0; col < 80; col++)
        vga[24 * 80 + col] = 0x0720;
    cur_row = 24;
}

void new_prompt() {
    if (cur_row >= 25) scroll();
    print_at(">                                                                               ", cur_row, 0, 0x0A);
    cur_col = 2;
    cmd_len = 0;
}

void print_line(const char *str, unsigned char color) {
    if (cur_row >= 25) scroll();
    print_at(str, cur_row, 0, color);
    cur_row++;
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a - *b;
}

void update_clock() {
    unsigned int seconds = ticks / 100;
    unsigned int minutes = seconds / 60;
    unsigned int hours   = minutes / 60;
    seconds %= 60;
    minutes %= 60;
    print_at("        ", 0, 68, 0x0B);
    print_int(hours,    0, 68, 0x0B);
    print_at(":",       0, 70, 0x0B);
    print_int(minutes,  0, 71, 0x0B);
    print_at(":",       0, 73, 0x0B);
    print_int(seconds,  0, 74, 0x0B);
}

void timer_handler() {
    ticks++;
    update_clock();
    outb(0x20, 0x20);
}

void handle_command() {
    cmd_buf[cmd_len] = 0;
    cur_row++;
    if (cur_row >= 25) scroll();

    if (strcmp(cmd_buf, "help") == 0) {
        print_line("  Commands: help, clear, about, hello, time, mem", 0x0B);
    } else if (strcmp(cmd_buf, "clear") == 0) {
        clear_screen();
        print_at("=== MyOS v0.1 - Built by Noah & Claude ===", 0, 0, 0x0A);
        print_at("------------------------------------------", 2, 0, 0x08);
        print_at("  Type 'help' for commands                ", 3, 0, 0x08);
        cur_row = 4;
    } else if (strcmp(cmd_buf, "about") == 0) {
        print_line("  MyOS v0.1 - Built by Noah & Claude", 0x0D);
        print_line("  Written in C and Assembly", 0x0D);
        print_line("  Running on x86 32-bit protected mode", 0x0D);
    } else if (strcmp(cmd_buf, "hello") == 0) {
        print_line("  Hello Noah! Your OS is alive!", 0x0E);
    } else if (strcmp(cmd_buf, "time") == 0) {
        print_at("  Uptime: ", cur_row, 0, 0x0B);
        print_int(ticks / 100, cur_row, 9, 0x0B);
        print_at(" seconds  ", cur_row, 12, 0x0B);
        cur_row++;
    } else if (strcmp(cmd_buf, "mem") == 0) {
        print_at("  Memory used: ", cur_row, 0, 0x0E);
        print_int(mem_used(), cur_row, 15, 0x0E);
        print_at(" bytes  ", cur_row, 22, 0x0E);
        cur_row++;
        print_at("  Memory free: ", cur_row, 0, 0x0A);
        print_int(mem_free(), cur_row, 15, 0x0A);
        print_at(" bytes  ", cur_row, 22, 0x0A);
        cur_row++;
    } else if (cmd_len > 0) {
        print_at("  Unknown: ", cur_row, 0, 0x0C);
        print_at(cmd_buf, cur_row, 10, 0x0C);
        cur_row++;
    }

    new_prompt();
}

const char scanmap_lower[] = {
    0, 0, '1','2','3','4','5','6','7','8','9','0','-','=',0,0,
    'q','w','e','r','t','y','u','i','o','p','[',']',0,0,
    'a','s','d','f','g','h','j','k','l',';','\'','`',0,'\\',
    'z','x','c','v','b','n','m',',','.','/',0,'*',0,' '
};

const char scanmap_upper[] = {
    0, 0, '!','@','#','$','%','^','&','*','(',')','_','+',0,0,
    'Q','W','E','R','T','Y','U','I','O','P','{','}',0,0,
    'A','S','D','F','G','H','J','K','L',':','"','~',0,'|',
    'Z','X','C','V','B','N','M','<','>','?',0,'*',0,' '
};

void keyboard_handler() {
    unsigned char sc = inb(0x60);

    if (sc == 0x2A || sc == 0x36) { shift_pressed = 1; outb(0x20, 0x20); return; }
    if (sc == 0xAA || sc == 0xB6) { shift_pressed = 0; outb(0x20, 0x20); return; }
    if (sc & 0x80) { outb(0x20, 0x20); return; }

    if (sc == 0x0E) {
        if (cmd_len > 0) {
            cmd_len--;
            cur_col--;
            vga[cur_row * 80 + cur_col] = (unsigned short)(0x0F00 | ' ');
        }
        outb(0x20, 0x20);
        return;
    }

    if (sc == 0x1C) {
        handle_command();
        outb(0x20, 0x20);
        return;
    }

    if (sc < sizeof(scanmap_lower)) {
        char c = shift_pressed ? scanmap_upper[sc] : scanmap_lower[sc];
        if (c && cmd_len < 78) {
            cmd_buf[cmd_len++] = c;
            vga[cur_row * 80 + cur_col] = (unsigned short)(0x0F00 | c);
            cur_col++;
        }
    }

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
extern void load_idt(struct idt_ptr *);

void idt_set(unsigned char num, unsigned int base) {
    idt[num].base_low  = base & 0xFFFF;
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].sel       = 0x08;
    idt[num].zero      = 0;
    idt[num].flags     = 0x8E;
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
    load_idt(&idtp);
}

void init_timer() {
    unsigned int divisor = 1193180 / 100;
    outb(0x43, 0x36);
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);
}

void kernel_main() {
    clear_screen();
    print_at("=== MyOS v0.1 - Built by Noah & Claude ===", 0, 0, 0x0A);
    print_at("  32-bit protected mode | Shell ready     ", 1, 0, 0x0F);
    print_at("------------------------------------------", 2, 0, 0x08);
    print_at("  Type 'help' for commands                ", 3, 0, 0x08);

    mem_init();
    init_idt();
    init_timer();
    __asm__ __volatile__("sti");

    new_prompt();

    // Just halt and wait for interrupts
    while (1) {
        __asm__ __volatile__("hlt");
    }
}
