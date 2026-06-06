unsigned short *vga = (unsigned short *)0xB8000;
int cur_row = 4;
int cur_col = 2;
char cmd_buf[80];
int cmd_len = 0;

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

void print_line(const char *str, unsigned char color) {
    print_at(str, cur_row, 0, color);
    cur_row++;
    if (cur_row >= 24) cur_row = 5;
}

void new_prompt() {
    if (cur_row >= 24) cur_row = 5;
    print_at(">                                                                               ", cur_row, 0, 0x0A);
    cur_col = 2;
    cmd_len = 0;
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a - *b;
}

void handle_command() {
    cmd_buf[cmd_len] = 0;

    cur_row++;
    if (cur_row >= 24) cur_row = 5;

    if (strcmp(cmd_buf, "help") == 0) {
        print_line("  Commands: help, clear, about, hello", 0x0B);
    } else if (strcmp(cmd_buf, "clear") == 0) {
        clear_screen();
        print_at("=== MyOS v0.1 - Built by Noah & Claude ===", 0, 0, 0x0A);
        print_at("  32-bit protected mode | Shell ready     ", 1, 0, 0x0F);
        print_at("------------------------------------------", 2, 0, 0x08);
        cur_row = 4;
    } else if (strcmp(cmd_buf, "about") == 0) {
        print_line("  MyOS v0.1 - Built by Noah & Claude", 0x0D);
        print_line("  Written in C and Assembly", 0x0D);
        print_line("  Running on x86 32-bit protected mode", 0x0D);
    } else if (strcmp(cmd_buf, "hello") == 0) {
        print_line("  Hello Noah! Your OS is alive!", 0x0E);
    } else if (cmd_len > 0) {
        print_at("  Unknown command: ", cur_row, 0, 0x0C);
        print_at(cmd_buf, cur_row, 18, 0x0C);
        cur_row++;
    }

    new_prompt();
}

static inline unsigned char inb(unsigned short port) {
    unsigned char val;
    __asm__ __volatile__("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

const char scanmap[] = {
    0, 0, '1','2','3','4','5','6','7','8','9','0','-','=',0,0,
    'q','w','e','r','t','y','u','i','o','p','[',']',0,0,
    'a','s','d','f','g','h','j','k','l',';','\'','`',0,'\\',
    'z','x','c','v','b','n','m',',','.','/',0,'*',0,' '
};

void kernel_main() {
    clear_screen();
    print_at("=== MyOS v0.1 - Built by Noah & Claude ===", 0, 0, 0x0A);
    print_at("  32-bit protected mode | Shell ready     ", 1, 0, 0x0F);
    print_at("------------------------------------------", 2, 0, 0x08);
    print_at("  Type 'help' for commands", 3, 0, 0x08);
    new_prompt();

    while (1) {
        while (!(inb(0x64) & 1));
        unsigned char sc = inb(0x60);
        if (sc & 0x80) continue;

        // Backspace
        if (sc == 0x0E) {
            if (cmd_len > 0) {
                cmd_len--;
                cur_col--;
                vga[cur_row * 80 + cur_col] = (unsigned short)(0x0F00 | ' ');
            }
            continue;
        }

        // Enter
        if (sc == 0x1C) {
            handle_command();
            continue;
        }

        if (sc >= sizeof(scanmap)) continue;
        char c = scanmap[sc];
        if (c == 0) continue;

        if (cmd_len < 78) {
            cmd_buf[cmd_len++] = c;
            vga[cur_row * 80 + cur_col] = (unsigned short)(0x0F00 | c);
            cur_col++;
        }
    }
}
