static int mouse_x = 160;
static int mouse_y = 100;
static int mouse_cycle = 0;
static unsigned char mouse_byte[3];
static int last_x = 160;
static int last_y = 100;

static inline unsigned char inb(unsigned short port) {
    unsigned char val;
    __asm__ __volatile__("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static inline void outb(unsigned short port, unsigned char val) {
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

extern void draw_cursor(int x, int y);
extern void clear_cursor(int x, int y);

void mouse_wait_input() {
    unsigned int timeout = 100000;
    while (timeout--) if (!(inb(0x64) & 2)) return;
}

void mouse_wait_output() {
    unsigned int timeout = 100000;
    while (timeout--) if (inb(0x64) & 1) return;
}

void mouse_write(unsigned char data) {
    mouse_wait_input();
    outb(0x64, 0xD4);
    mouse_wait_input();
    outb(0x60, data);
}

unsigned char mouse_read() {
    mouse_wait_output();
    return inb(0x60);
}

void mouse_init() {
    // Enable auxiliary device
    mouse_wait_input();
    outb(0x64, 0xA8);

    // Enable IRQ12
    mouse_wait_input();
    outb(0x64, 0x20);
    mouse_wait_output();
    unsigned char status = (inb(0x60) | 2);
    mouse_wait_input();
    outb(0x64, 0x60);
    mouse_wait_input();
    outb(0x60, status);

    // Use default settings
    mouse_write(0xF6);
    mouse_read();

    // Enable data reporting
    mouse_write(0xF4);
    mouse_read();
}

int mouse_get_x() { return mouse_x; }
int mouse_get_y() { return mouse_y; }

void mouse_handler() {
    // Send EOI to slave PIC first
    outb(0xA0, 0x20);
    outb(0x20, 0x20);

    unsigned char data = inb(0x60);

    mouse_byte[mouse_cycle] = data;
    mouse_cycle++;

    if (mouse_cycle == 1) {
        if (!(data & 0x08)) { mouse_cycle = 0; return; }
    }

    if (mouse_cycle == 3) {
        mouse_cycle = 0;

        clear_cursor(last_x, last_y);

        int dx = (int)mouse_byte[1] - ((mouse_byte[0] & 0x10) ? 256 : 0);
        int dy = (int)mouse_byte[2] - ((mouse_byte[0] & 0x20) ? 256 : 0);

        mouse_x += dx;
        mouse_y -= dy;

        if (mouse_x < 0) mouse_x = 0;
        if (mouse_x >= 320) mouse_x = 319;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_y >= 200) mouse_y = 199;

        last_x = mouse_x;
        last_y = mouse_y;

        draw_cursor(mouse_x, mouse_y);
    }
}
