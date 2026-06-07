// MyOS RTL8139 Network Driver

#define RTL8139_VENDOR  0x10EC
#define RTL8139_DEVICE  0x8139

#define REG_MAC0        0x00
#define REG_TXSTATUS0   0x10
#define REG_TXADDR0     0x20
#define REG_RXBUF       0x30
#define REG_CMD         0x37
#define REG_RXPTR       0x38
#define REG_IMR         0x3C
#define REG_ISR         0x3E
#define REG_TXCFG       0x40
#define REG_RXCFG       0x44

#define CMD_RESET       0x10
#define CMD_RXEN        0x08
#define CMD_TXEN        0x04

#define RX_BUF_SIZE     8192
#define TX_BUF_SIZE     1536

typedef unsigned int   uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char  uint8_t;

static uint16_t rtl_iobase = 0;
static uint8_t  mac[6];
static uint8_t  rx_buffer[RX_BUF_SIZE + 16];
static uint8_t  tx_buffer[TX_BUF_SIZE];
static uint32_t rx_ptr = 0;
static int      tx_cur = 0;
static int      net_ready = 0;

static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ __volatile__("inb %1,%0" : "=a"(val) : "Nd"(port));
    return val;
}
static inline uint16_t inw(uint16_t port) {
    uint16_t val;
    __asm__ __volatile__("inw %1,%0" : "=a"(val) : "Nd"(port));
    return val;
}
static inline uint32_t inl(uint16_t port) {
    uint32_t val;
    __asm__ __volatile__("inl %1,%0" : "=a"(val) : "Nd"(port));
    return val;
}
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__("outb %0,%1" : : "a"(val), "Nd"(port));
}
static inline void outw(uint16_t port, uint16_t val) {
    __asm__ __volatile__("outw %0,%1" : : "a"(val), "Nd"(port));
}
static inline void outl(uint16_t port, uint32_t val) {
    __asm__ __volatile__("outl %0,%1" : : "a"(val), "Nd"(port));
}

uint32_t pci_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t addr = (1<<31)|(bus<<16)|(slot<<11)|(func<<8)|(offset&0xFC);
    outl(0xCF8, addr);
    return inl(0xCFC);
}

void pci_write(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val) {
    uint32_t addr = (1<<31)|(bus<<16)|(slot<<11)|(func<<8)|(offset&0xFC);
    outl(0xCF8, addr);
    outl(0xCFC, val);
}

uint16_t pci_find_rtl8139() {
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint32_t id = pci_read(bus, slot, 0, 0);
            if ((id & 0xFFFF) == 0x10EC && ((id>>16)&0xFFFF) == 0x8139) {
                uint32_t bar0 = pci_read(bus, slot, 0, 0x10);
                uint32_t cmd  = pci_read(bus, slot, 0, 0x04);
                pci_write(bus, slot, 0, 0x04, cmd | 0x07);
                return (uint16_t)(bar0 & 0xFFFC);
            }
        }
    }
    return 0;
}

void memset_local(void *ptr, uint8_t val, uint32_t size) {
    uint8_t *p = (uint8_t *)ptr;
    for (uint32_t i = 0; i < size; i++) p[i] = val;
}

int net_init() {
    rtl_iobase = pci_find_rtl8139();
    if (!rtl_iobase) return 0;

    outb(rtl_iobase + 0x52, 0x00);
    outb(rtl_iobase + REG_CMD, CMD_RESET);
    int timeout = 10000;
    while ((inb(rtl_iobase + REG_CMD) & CMD_RESET) && timeout--);

    for (int i = 0; i < 6; i++)
        mac[i] = inb(rtl_iobase + REG_MAC0 + i);

    memset_local(rx_buffer, 0, RX_BUF_SIZE + 16);
    outl(rtl_iobase + REG_RXBUF, (uint32_t)rx_buffer);
    outb(rtl_iobase + REG_CMD, CMD_RXEN | CMD_TXEN);
    outl(rtl_iobase + REG_RXCFG, 0xF | (1 << 7));
    outw(rtl_iobase + REG_IMR, 0x0005);

    net_ready = 1;
    return 1;
}

uint8_t *net_get_mac() { return mac; }
int net_is_ready() { return net_ready; }

int net_send(uint8_t *data, uint16_t len) {
    if (!net_ready || len > TX_BUF_SIZE) return 0;
    for (int i = 0; i < len; i++) tx_buffer[i] = data[i];
    outl(rtl_iobase + REG_TXADDR0 + tx_cur * 4, (uint32_t)tx_buffer);
    outl(rtl_iobase + REG_TXSTATUS0 + tx_cur * 4, len & 0x1FFF);
    tx_cur = (tx_cur + 1) % 4;
    return 1;
}

static uint8_t  last_packet[1536];
static uint16_t last_packet_len = 0;

void net_handler() {
    uint16_t isr = inw(rtl_iobase + REG_ISR);
    if (isr & 0x01) {
        uint16_t *header = (uint16_t *)(rx_buffer + rx_ptr);
        uint16_t pkt_len = header[1];
        if (pkt_len > 0 && pkt_len < 1536) {
            uint8_t *pkt = (uint8_t *)(rx_buffer + rx_ptr + 4);
            for (int i = 0; i < pkt_len; i++) last_packet[i] = pkt[i];
            last_packet_len = pkt_len;
            rx_ptr = (rx_ptr + pkt_len + 4 + 3) & ~3;
            if (rx_ptr >= RX_BUF_SIZE) rx_ptr -= RX_BUF_SIZE;
            outw(rtl_iobase + REG_RXPTR, rx_ptr - 16);
        }
    }
    outw(rtl_iobase + REG_ISR, isr);
    outb(0xA0, 0x20);
    outb(0x20, 0x20);
}

uint8_t  *net_get_last_packet() { return last_packet; }
uint16_t  net_get_last_len()    { return last_packet_len; }
