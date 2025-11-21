//
// E1000 hardware definitions: registers and DMA ring format.
// from the Intel 82540EP/EM &c manual.
//
// NOTE: Register macros use (byte_offset/4) because the driver
//       indexes a volatile uint32* MMIO window (regs[idx]).

//
#pragma once
void
e1000_init(uint32* xregs);
int
e1000_transmit(char* buf, int len);

/* Registers */
#define E1000_CTL (0x00000 / 4)  /* Device Control Register - RW */
#define E1000_ICR (0x000C0 / 4)  /* Interrupt Cause Read - R */
#define E1000_IMS (0x000D0 / 4)  /* Interrupt Mask Set - RW */
#define E1000_RCTL (0x00100 / 4) /* RX Control - RW */
#define E1000_TCTL (0x00400 / 4) /* TX Control - RW */
#define E1000_TIPG (0x00410 / 4) /* TX Inter-packet gap - RW */

/* RX ring base/len/head/tail */
#define E1000_RDBAL (0x02800 / 4) /* RX Descriptor Base Address Low  - RW */
#define E1000_RDBAH (0x02804 / 4) /* RX Descriptor Base Address High - RW */
#define E1000_RDLEN (0x02808 / 4) /* RX Descriptor Length            - RW */
#define E1000_RDH (0x02810 / 4)   /* RX Descriptor Head              - RW */
#define E1000_RDT (0x02818 / 4)   /* RX Descriptor Tail              - RW */
#define E1000_RDTR (0x02820 / 4)  /* RX Delay Timer                  - RW */
#define E1000_RADV (0x0282C / 4)  /* RX Interrupt Absolute Delay     - RW */

/* TX ring base/len/head/tail */
#define E1000_TDBAL (0x03800 / 4) /* TX Descriptor Base Address Low  - RW */
#define E1000_TDBAH (0x03804 / 4) /* TX Descriptor Base Address High - RW */
#define E1000_TDLEN (0x03808 / 4) /* TX Descriptor Length            - RW */
#define E1000_TDH (0x03810 / 4)   /* TX Descriptor Head              - RW */
#define E1000_TDT (0x03818 / 4)   /* TX Descriptor Tail              - RW */

#define E1000_MTA (0x05200 / 4) /* Multicast Table Array - RW Array */
#define E1000_RA (0x05400 / 4)  /* Receive Address        - RW Array */

/* Device Control */
#define E1000_CTL_RST 0x04000000 /* full reset */

/* Transmit Control */
#define E1000_TCTL_EN 0x00000002  /* enable tx */
#define E1000_TCTL_PSP 0x00000008 /* pad short packets */
#define E1000_TCTL_CT_SHIFT 4
#define E1000_TCTL_COLD_SHIFT 12

/* Receive Control */
#define E1000_RCTL_EN 0x00000002      /* enable */
#define E1000_RCTL_BAM 0x00008000     /* broadcast enable */
#define E1000_RCTL_SZ_2048 0x00000000 /* rx buffer size 2048 */
#define E1000_RCTL_SECRC 0x04000000   /* Strip Ethernet CRC */

/* Transmit Descriptor command definitions [E1000 3.3.3.1] */
#define E1000_TXD_CMD_EOP 0x01 /* End of Packet */
#define E1000_TXD_CMD_RS 0x08  /* Report Status */

/* Transmit Descriptor status definitions [E1000 3.3.3.2] */
#define E1000_TXD_STAT_DD 0x00000001 /* Descriptor Done */

/* [E1000 3.3.3] Legacy Transmit Descriptor Format */
struct tx_desc {
    uint64 addr;
    ushort length;
    uchar cso;
    uchar cmd;
    uchar status;
    uchar css;
    ushort special;
};

/* Receive Descriptor bit definitions [E1000 3.2.3.1] */
#define E1000_RXD_STAT_DD 0x01  /* Descriptor Done */
#define E1000_RXD_STAT_EOP 0x02 /* End of Packet */

/* [E1000 3.2.3] Legacy Receive Descriptor Format */
struct rx_desc {
    uint64 addr;   /* Address of the descriptor's data buffer (PHYS) */
    ushort length; /* Length of data DMAed into data buffer */
    ushort csum;   /* Packet checksum */
    uchar status;  /* Descriptor status */
    uchar errors;  /* Descriptor Errors */
    ushort special;
};
