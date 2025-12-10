#pragma once

// ================================================================
// Endianness helpers
//
// The network stack in xv6 uses *host* byte order internally
// (whatever the CPU uses), but network protocols (IP, UDP, TCP,
// etc.) use *network byte order* (big-endian).
//
// These helpers convert between host and network byte order.
// On x86 (little-endian), these functions actually swap bytes.
// ================================================================

static inline ushort
bswaps(ushort val) {
    // Swap the two bytes in a 16-bit value: 0xAABB -> 0xBBAA
    return (((val & 0x00ffU) << 8) | ((val & 0xff00U) >> 8));
}

static inline uint32
bswapl(uint32 val) {
    // Swap the four bytes in a 32-bit value: 0xAABBCCDD -> 0xDDCCBBAA
    return (((val & 0x000000ffUL) << 24) | ((val & 0x0000ff00UL) << 8) |
            ((val & 0x00ff0000UL) >> 8) | ((val & 0xff000000UL) >> 24));
}

// Standard socket-style names:
//   ntoh* = network-to-host
//   hton* = host-to-network
// All are just byte-swaps on x86, but on a big-endian CPU
// these would be no-ops.
#define ntohs bswaps
#define ntohl bswapl
#define htons bswaps
#define htonl bswapl

// ================================================================
// Basic networking headers
// These structs map directly onto bytes "on the wire" in an
// Ethernet frame.  They must match real protocol layouts.
// ================================================================

#define ETHADDR_LEN 6  // MAC addresses are 6 bytes

// ------------------------- Ethernet ----------------------------
// Layout of an Ethernet header (14 bytes):
//   6 bytes dest MAC
//   6 bytes src MAC
//   2 bytes "type" (e.g., IP, ARP)
//
// __attribute__((packed)) stops the compiler from inserting
// padding between fields; we want the in-memory layout to match
// the network layout exactly.
// ----------------------------------------------------------------
struct eth {
    uchar dhost[ETHADDR_LEN];  // destination MAC address
    uchar shost[ETHADDR_LEN];  // source MAC address
    ushort type;               // payload type (ETHTYPE_*)
} __attribute__((packed));

#define ETHTYPE_IP 0x0800   // Ethernet type for IPv4
#define ETHTYPE_ARP 0x0806  // Ethernet type for ARP

// --------------------------- IP ---------------------------------
// IPv4 header (without options).  The fields correspond to the
// standard IPv4 header; see RFC 791.
//
// Note: ip_vhl packs both the version and header length into one
// byte; you must mask/shift to get each piece.
// ----------------------------------------------------------------
struct ip {
    uchar ip_vhl;   // version (high 4 bits) | header length (low 4 bits, in
                    // 32-bit words)
    uchar ip_tos;   // type of service / DSCP
    ushort ip_len;  // total length of IP packet (header + data), in bytes
    ushort ip_id;   // identification field (used for fragmentation)
    ushort ip_off;  // fragment offset + flags (DF/MF)
    uchar ip_ttl;   // time to live (hop limit)
    uchar ip_p;     // protocol (IPPROTO_*)
    ushort ip_sum;  // checksum over the IP header only
    uint32 ip_src;  // source IP address (in network byte order)
    uint32 ip_dst;  // destination IP address (in network byte order)
};

// Values for ip_p (protocol field in IP header)
#define IPPROTO_ICMP 1  // Internet Control Message Protocol
#define IPPROTO_TCP 6   // Transmission Control Protocol
#define IPPROTO_UDP 17  // User Datagram Protocol

// Helper macro to build a 32-bit IPv4 address from 4 octets.
// The result is in network byte order (big-endian).
#define MAKE_IP_ADDR(a, b, c, d)                                      \
    (((uint32)(a) << 24) | ((uint32)(b) << 16) | ((uint32)(c) << 8) | \
     (uint32)(d))

// --------------------------- UDP --------------------------------
// UDP header preceding the payload inside an IP packet.
// All fields are in network byte order.
// ----------------------------------------------------------------
struct udp {
    ushort sport;  // source port
    ushort dport;  // destination port
    ushort ulen;   // length of UDP header + data, in bytes
    ushort sum;    // UDP checksum (optional; can be 0)
};

// --------------------------- ARP --------------------------------
// ARP packet carried inside an Ethernet frame. Used to map an
// IPv4 address to a MAC address on the local network.
// ----------------------------------------------------------------
struct arp {
    ushort hrd;  // hardware type (e.g., ARP_HRD_ETHER for Ethernet)
    ushort pro;  // protocol type (e.g., ETHTYPE_IP for IPv4)
    uchar hln;   // hardware address length (6 for MAC)
    uchar pln;   // protocol address length (4 for IPv4)
    ushort op;   // operation (ARP_OP_REQUEST or ARP_OP_REPLY)

    char sha[ETHADDR_LEN];  // sender hardware (MAC) address
    uint32 sip;             // sender IP address
    char tha[ETHADDR_LEN];  // target hardware (MAC) address
    uint32 tip;             // target IP address
} __attribute__((packed));

#define ARP_HRD_ETHER 1  // hardware type value for Ethernet

// ARP operation codes
enum {
    ARP_OP_REQUEST = 1,  // "Who has IP X? Tell me your MAC."
    ARP_OP_REPLY = 2,    // "IP X is at MAC Y."
};

// --------------------------- DNS --------------------------------
// Minimal DNS message header and supporting structs.
// See RFC 1035.
//
// The header is followed by variable-length sections:
//   - Question(s)
//   - Answer(s)
//   - Authority records
//   - Additional records
//
// Note: the bitfield layout assumes a particular endianness and
// is tightly packed; this is fine for this lab environment but
// would need extra care in portable production code.
// ----------------------------------------------------------------
struct dns {
    ushort id;  // query ID; used to match replies to requests

    // First flags byte (QR, OPCODE, AA, TC, RD)
    uchar rd : 1;      // recursion desired
    uchar tc : 1;      // truncated message
    uchar aa : 1;      // authoritative answer
    uchar opcode : 4;  // kind of query (usually 0 = standard)
    uchar qr : 1;      // query(0) or response(1)

    // Second flags byte (RA, Z, AD, CD, RCODE)
    uchar rcode : 4;  // response code (0 = no error)
    uchar cd : 1;     // checking disabled
    uchar ad : 1;     // authenticated data
    uchar z : 1;      // reserved (must be 0)
    uchar ra : 1;     // recursion available

    // Counts of each section that follow this header
    ushort qdcount;  // number of questions
    ushort ancount;  // number of answers
    ushort nscount;  // number of name server records
    ushort arcount;  // number of additional records
} __attribute__((packed));

// Question section following the DNS header:
//   [qname (variable length)] + dns_question
struct dns_question {
    ushort qtype;   // record type (e.g., ARECORD)
    ushort qclass;  // class (almost always QCLASS = IN = 1)
} __attribute__((packed));

#define ARECORD 0x0001  // DNS A record (IPv4 address)
#define QCLASS 0x0001   // Internet (IN) class

// Resource record metadata (name is before, RDATA after)
struct dns_data {
    ushort type;   // record type (e.g., ARECORD)
    ushort class;  // record class (e.g., QCLASS)
    uint ttl;    // time-to-live in seconds
    ushort len;    // length of the RDATA that follows
} __attribute__((packed));
