#define REG_MAC0      0x00
#define REG_TXSTATUS0 0x10
#define REG_TXADDR0   0x20
#define REG_RXBUF     0x30
#define REG_CMD       0x37
#define REG_CAPR      0x38
#define REG_IMR       0x3C
#define REG_ISR       0x3E
#define REG_RXCFG     0x44

#define CMD_RESET     0x10
#define CMD_RXEN      0x08
#define CMD_TXEN      0x04

typedef unsigned int   uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char  uint8_t;

// Use fixed safe memory addresses above our kernel
#define RX_BUFFER_ADDR  0x200000   // 2MB mark - safe
#define TX_BUFFER_ADDR  0x202000   // just after RX
#define RX_BUF_SIZE     8192

static uint16_t rtl_iobase = 0;
static uint8_t  mac[6];
static uint32_t rx_ptr = 0;
static int      tx_cur = 0;
static int      net_ready = 0;

static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ __volatile__("inb %1,%0":"=a"(val):"Nd"(port));
    return val;
}
static inline uint16_t inw(uint16_t port) {
    uint16_t val;
    __asm__ __volatile__("inw %1,%0":"=a"(val):"Nd"(port));
    return val;
}
static inline uint32_t inl(uint16_t port) {
    uint32_t val;
    __asm__ __volatile__("inl %1,%0":"=a"(val):"Nd"(port));
    return val;
}
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__("outb %0,%1"::"a"(val),"Nd"(port));
}
static inline void outw(uint16_t port, uint16_t val) {
    __asm__ __volatile__("outw %0,%1"::"a"(val),"Nd"(port));
}
static inline void outl(uint16_t port, uint32_t val) {
    __asm__ __volatile__("outl %0,%1"::"a"(val),"Nd"(port));
}

uint32_t pci_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off) {
    outl(0xCF8,(1<<31)|(bus<<16)|(slot<<11)|(func<<8)|(off&0xFC));
    return inl(0xCFC);
}
void pci_write(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off, uint32_t v) {
    outl(0xCF8,(1<<31)|(bus<<16)|(slot<<11)|(func<<8)|(off&0xFC));
    outl(0xCFC,v);
}

uint16_t pci_find_rtl8139() {
    for (uint16_t bus=0;bus<256;bus++)
        for (uint8_t slot=0;slot<32;slot++) {
            uint32_t id=pci_read(bus,slot,0,0);
            if((id&0xFFFF)==0x10EC&&((id>>16)&0xFFFF)==0x8139) {
                uint32_t bar0=pci_read(bus,slot,0,0x10);
                pci_write(bus,slot,0,0x04,pci_read(bus,slot,0,0x04)|0x07);
                return (uint16_t)(bar0&0xFFFC);
            }
        }
    return 0;
}

int net_init() {
    rtl_iobase = pci_find_rtl8139();
    if (!rtl_iobase) return 0;

    outb(rtl_iobase+0x52, 0x00);
    outb(rtl_iobase+REG_CMD, CMD_RESET);
    int t=100000; while((inb(rtl_iobase+REG_CMD)&CMD_RESET)&&t--);

    for(int i=0;i<6;i++) mac[i]=inb(rtl_iobase+REG_MAC0+i);

    // Zero RX buffer
    uint8_t *rxbuf=(uint8_t*)RX_BUFFER_ADDR;
    for(int i=0;i<RX_BUF_SIZE+16;i++) rxbuf[i]=0;

    outl(rtl_iobase+REG_RXBUF, RX_BUFFER_ADDR);
    outb(rtl_iobase+REG_CMD, CMD_RXEN|CMD_TXEN);
    outl(rtl_iobase+REG_RXCFG, 0x0000000F); // Accept all, 8K buf
    outw(rtl_iobase+REG_ISR, 0xFFFF);
    outw(rtl_iobase+REG_IMR, 0x0005);

    rx_ptr=0;
    net_ready=1;
    return 1;
}

uint8_t *net_get_mac() { return mac; }
int net_is_ready() { return net_ready; }

int net_send(uint8_t *data, uint16_t len) {
    if(!net_ready||len>1536) return 0;
    uint8_t *txbuf=(uint8_t*)TX_BUFFER_ADDR;
    for(int i=0;i<len;i++) txbuf[i]=data[i];
    outl(rtl_iobase+REG_TXADDR0+tx_cur*4, TX_BUFFER_ADDR);
    outl(rtl_iobase+REG_TXSTATUS0+tx_cur*4, len&0x1FFF);
    tx_cur=(tx_cur+1)%4;
    return 1;
}

static uint8_t  last_packet[1536];
static uint16_t last_packet_len=0;

void net_poll() {
    if(!net_ready) return;
    last_packet_len=0;

    if(inb(rtl_iobase+REG_CMD)&0x01) return; // RX empty

    uint16_t isr=inw(rtl_iobase+REG_ISR);
    if(!(isr&0x01)) return;
    outw(rtl_iobase+REG_ISR, isr);

    uint8_t *rxbuf=(uint8_t*)RX_BUFFER_ADDR;
    uint32_t off=rx_ptr%RX_BUF_SIZE;
    uint16_t status =*(uint16_t*)(rxbuf+off);
    uint16_t pkt_len=*(uint16_t*)(rxbuf+off+2);

    if((status&0x01)&&pkt_len>0&&pkt_len<1536) {
        uint8_t *pkt=rxbuf+off+4;
        for(int i=0;i<pkt_len;i++) last_packet[i]=pkt[i];
        last_packet_len=pkt_len;
        rx_ptr=(rx_ptr+pkt_len+4+3)&~3;
        outw(rtl_iobase+REG_CAPR,(uint16_t)(rx_ptr-16));
    }
}

uint8_t  *net_get_last_packet() { return last_packet; }
uint16_t  net_get_last_len()    { return last_packet_len; }
