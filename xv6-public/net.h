#pragma once

//
// endianness support
//

static inline ushort bswaps(ushort val)
{
  return (((val & 0x00ffU) << 8) |
          ((val & 0xff00U) >> 8));
}

static inline uint32 bswapl(uint32 val)
{
  return (((val & 0x000000ffUL) << 24) |
          ((val & 0x0000ff00UL) << 8) |
          ((val & 0x00ff0000UL) >> 8) |
          ((val & 0xff000000UL) >> 24));
}

#define ntohs bswaps
#define ntohl bswapl
#define htons bswaps
#define htonl bswapl


//
// useful networking headers
//

#define ETHADDR_LEN 6

// Ethernet packet header
struct eth {
  uchar  dhost[ETHADDR_LEN];
  uchar  shost[ETHADDR_LEN];
  ushort type;
} __attribute__((packed));

#define ETHTYPE_IP  0x0800 // Internet protocol
#define ETHTYPE_ARP 0x0806 // Address resolution protocol

// IP packet header
struct ip {
  uchar  ip_vhl; // version << 4 | header length >> 2
  uchar  ip_tos; // type of service
  ushort ip_len; // total length, including this IP header
  ushort ip_id;  // identification
  ushort ip_off; // fragment offset field
  uchar  ip_ttl; // time to live
  uchar  ip_p;   // protocol
  ushort ip_sum; // checksum, covers just IP header
  uint32 ip_src, ip_dst;
};

#define IPPROTO_ICMP 1   // Control message protocol
#define IPPROTO_TCP  6   // Transmission control protocol
#define IPPROTO_UDP  17  // User datagram protocol

#define MAKE_IP_ADDR(a, b, c, d) \
  (((uint32)(a) << 24) | ((uint32)(b) << 16) | \
   ((uint32)(c) << 8)  | (uint32)(d))

// UDP packet header
struct udp {
  ushort sport; // source port
  ushort dport; // destination port
  ushort ulen;  // length, including udp header
  ushort sum;   // checksum
};

// ARP packet
struct arp {
  ushort hrd; // hardware address format
  ushort pro; // protocol address format
  uchar  hln; // hardware address length
  uchar  pln; // protocol address length
  ushort op;  // operation

  char   sha[ETHADDR_LEN]; // sender hardware address
  uint32 sip;              // sender IP address
  char   tha[ETHADDR_LEN]; // target hardware address
  uint32 tip;              // target IP address
} __attribute__((packed));

#define ARP_HRD_ETHER 1 // Ethernet

enum {
  ARP_OP_REQUEST = 1,
  ARP_OP_REPLY   = 2,
};

// DNS packet
struct dns {
  ushort id;  // request ID

  uchar rd:1;
  uchar tc:1;
  uchar aa:1;
  uchar opcode:4;
  uchar qr:1;
  uchar rcode:4;
  uchar cd:1;
  uchar ad:1;
  uchar z:1;
  uchar ra:1;

  ushort qdcount;
  ushort ancount;
  ushort nscount;
  ushort arcount;
} __attribute__((packed));

struct dns_question {
  ushort qtype;
  ushort qclass;
} __attribute__((packed));

#define ARECORD 0x0001
#define QCLASS  0x0001

struct dns_data {
  ushort type;
  ushort class;
  uint32 ttl;
  ushort len;
} __attribute__((packed));
