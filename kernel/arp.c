// MyOS ARP - Address Resolution Protocol

typedef unsigned int   uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char  uint8_t;

extern uint8_t *net_get_mac();
extern int net_send(uint8_t *data, uint16_t len);

// Our IP and gateway IP
#define MY_IP      0x0A00020F   // 10.0.2.15
#define GATEWAY_IP 0x0A000202   // 10.0.2.2

static uint8_t broadcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
static uint8_t gateway_mac[6] = {0};
static int gateway_resolved = 0;

void memcpy_local(void *dst, void *src, int n) {
    uint8_t *d = (uint8_t *)dst;
    uint8_t *s = (uint8_t *)src;
    for (int i = 0; i < n; i++) d[i] = s[i];
}

uint16_t htons(uint16_t v) {
    return (v >> 8) | (v << 8);
}

uint32_t htonl(uint32_t v) {
    return ((v & 0xFF) << 24) | ((v & 0xFF00) << 8) |
           ((v & 0xFF0000) >> 8) | ((v >> 24) & 0xFF);
}

// Send ARP request for target IP
void arp_request(uint32_t target_ip) {
    uint8_t packet[42];
    uint8_t *mac = net_get_mac();

    // Ethernet header
    memcpy_local(packet, broadcast, 6);      // Dst MAC = broadcast
    memcpy_local(packet + 6, mac, 6);        // Src MAC = our MAC
    packet[12] = 0x08; packet[13] = 0x06;   // EtherType = ARP

    // ARP payload
    packet[14] = 0x00; packet[15] = 0x01;   // Hardware type = Ethernet
    packet[16] = 0x08; packet[17] = 0x00;   // Protocol = IPv4
    packet[18] = 6;                          // Hardware size
    packet[19] = 4;                          // Protocol size
    packet[20] = 0x00; packet[21] = 0x01;   // Opcode = request

    memcpy_local(packet + 22, mac, 6);       // Sender MAC
    uint32_t my_ip = htonl(MY_IP);
    memcpy_local(packet + 28, &my_ip, 4);   // Sender IP

    memcpy_local(packet + 32, broadcast, 6); // Target MAC = unknown
    uint32_t tgt = htonl(target_ip);
    memcpy_local(packet + 38, &tgt, 4);     // Target IP

    net_send(packet, 42);
}

// Handle incoming ARP reply
void arp_handle(uint8_t *packet, uint16_t len) {
    if (len < 42) return;

    uint16_t opcode = (packet[20] << 8) | packet[21];
    if (opcode != 2) return; // Only handle replies

    // Extract sender IP
    uint32_t sender_ip = ((uint32_t)packet[28] << 24) |
                         ((uint32_t)packet[29] << 16) |
                         ((uint32_t)packet[30] << 8)  |
                          (uint32_t)packet[31];

    uint32_t gw = MY_IP & 0xFFFFFF00;
    gw |= 2; // 10.0.2.2

    if (sender_ip == gw) {
        memcpy_local(gateway_mac, packet + 22, 6);
        gateway_resolved = 1;
    }
}

int arp_is_resolved() { return gateway_resolved; }
uint8_t *arp_get_gateway_mac() { return gateway_mac; }
uint32_t arp_get_my_ip() { return MY_IP; }
uint32_t arp_get_gateway_ip() { return GATEWAY_IP; }
