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
extern void fs_init();
extern int fs_create(const char *name);
extern int fs_write(const char *name, const char *data);
extern const char *fs_read(const char *name);
extern int fs_delete(const char *name);
extern int fs_list(char out[][32], unsigned int sizes[], int max);
extern void task_init();
extern int task_create(void (*entry)(), const char *name);
extern void task_switch();
extern void enable_multitasking();
extern int task_get_count();
extern int task_get_current();
extern const char *task_get_name(int id);
extern void task_kill(int id);

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

int strncmp(const char *a, const char *b, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i]) return a[i] - b[i];
        if (!a[i]) return 0;
    }
    return 0;
}

int strlen(const char *s) {
    int i = 0; while (s[i]) i++; return i;
}

void update_clock() {
    unsigned int seconds = ticks / 100;
    unsigned int minutes = seconds / 60;
    unsigned int hours   = minutes / 60;
    seconds %= 60; minutes %= 60;
    print_at("        ", 0, 68, 0x0B);
    print_int(hours,    0, 68, 0x0B);
    print_at(":",       0, 70, 0x0B);
    print_int(minutes,  0, 71, 0x0B);
    print_at(":",       0, 73, 0x0B);
    print_int(seconds,  0, 74, 0x0B);
}

void spinner_task() {
    const char frames[] = "|/-\\";
    int f = 0;
    while (1) {
        vga[78] = (unsigned short)(0x0C00 | frames[f % 4]);
        f++;
        for (volatile int i = 0; i < 500000; i++);
    }
}

void timer_handler() {
    ticks++;
    update_clock();
    task_switch();
    outb(0x20, 0x20);
}

void handle_command() {
    cmd_buf[cmd_len] = 0;
    cur_row++;
    if (cur_row >= 25) scroll();

    if (strcmp(cmd_buf, "help") == 0) {
        print_line("  Commands: help, clear, about, hello,", 0x0B);
        print_line("    time, mem, ls, touch, read, write,", 0x0B);
        print_line("    rm, ps, kill", 0x0B);
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
        print_at("  Used: ", cur_row, 0, 0x0E);
        print_int(mem_used(), cur_row, 8, 0x0E);
        print_at(" bytes   Free: ", cur_row, 14, 0x0A);
        print_int(mem_free(), cur_row, 29, 0x0A);
        print_at(" bytes  ", cur_row, 35, 0x0A);
        cur_row++;
    } else if (strcmp(cmd_buf, "ls") == 0) {
        char names[32][32];
        unsigned int sizes[32];
        int count = fs_list(names, sizes, 32);
        if (count == 0) {
            print_line("  No files found.", 0x08);
        } else {
            for (int i = 0; i < count; i++) {
                print_at("  ", cur_row, 0, 0x0F);
                print_at(names[i], cur_row, 2, 0x0F);
                print_int(sizes[i], cur_row, 4 + strlen(names[i]), 0x08);
                print_at(" bytes", cur_row, 10 + strlen(names[i]), 0x08);
                cur_row++;
                if (cur_row >= 25) scroll();
            }
        }
    } else if (strncmp(cmd_buf, "touch ", 6) == 0) {
        if (fs_create(cmd_buf + 6) >= 0)
            print_line("  File created.", 0x0A);
        else
            print_line("  Error: file exists or no space.", 0x0C);
    } else if (strncmp(cmd_buf, "rm ", 3) == 0) {
        if (fs_delete(cmd_buf + 3) == 0)
            print_line("  File deleted.", 0x0A);
        else
            print_line("  Error: file not found.", 0x0C);
    } else if (strncmp(cmd_buf, "read ", 5) == 0) {
        const char *data = fs_read(cmd_buf + 5);
        if (data) { print_at("  ", cur_row, 0, 0x0F); print_at(data, cur_row, 2, 0x0F); cur_row++; }
        else print_line("  Error: file not found.", 0x0C);
    } else if (strncmp(cmd_buf, "write ", 6) == 0) {
        char *rest = cmd_buf + 6;
        char name[32];
        int ni = 0;
        while (rest[ni] && rest[ni] != ' ' && ni < 31) { name[ni] = rest[ni]; ni++; }
        name[ni] = 0;
        if (fs_write(name, rest + ni + 1) >= 0)
            print_line("  File written.", 0x0A);
        else
            print_line("  Error: file not found. Use touch first.", 0x0C);
    } else if (strcmp(cmd_buf, "ps") == 0) {
        print_line("  PID  NAME", 0x0B);
        for (int i = 0; i < 8; i++) {
            const char *name = task_get_name(i);
            if (name) {
                print_at("  ", cur_row, 0, 0x0F);
                print_int(i, cur_row, 2, 0x0F);
                print_at("    ", cur_row, 3, 0x0F);
                print_at(name, cur_row, 7, 0x0F);
                if (i == task_get_current())
                    print_at(" <-- running", cur_row, 7 + strlen(name), 0x0A);
                cur_row++;
                if (cur_row >= 25) scroll();
            }
        }
    } else if (strncmp(cmd_buf, "kill ", 5) == 0) {
        int id = cmd_buf[5] - '0';
        if (id <= 0) print_line("  Cannot kill kernel task.", 0x0C);
        else { task_kill(id); print_line("  Task killed.", 0x0A); }
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
        if (cmd_len > 0) { cmd_len--; cur_col--; vga[cur_row * 80 + cur_col] = 0x0F20; }
        outb(0x20, 0x20); return;
    }
    if (sc == 0x1C) { handle_command(); outb(0x20, 0x20); return; }
    if (sc < sizeof(scanmap_lower)) {
        char c = shift_pressed ? scanmap_upper[sc] : scanmap_lower[sc];
        if (c && cmd_len < 78) {
            cmd_buf[cmd_len++] = c;
            vga[cur_row * 80 + cur_col++] = (unsigned short)(0x0F00 | c);
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
    fs_init();
    task_init();
    init_idt();
    init_timer();
    __asm__ __volatile__("sti");

    fs_create("readme.txt");
    fs_write("readme.txt", "Welcome to MyOS! Built by Noah and Claude.");

    task_create(spinner_task, "spinner");
    enable_multitasking();

    new_prompt();

    while (1) {
        __asm__ __volatile__("hlt");
    }
}
