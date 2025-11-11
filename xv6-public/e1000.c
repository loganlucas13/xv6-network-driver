//
// Intel E1000 network driver (x86-64 version)
// Adapted from xv6-labs networking starter code
// Works with QEMU's 82540EM (device 0x100E, vendor 0x8086).
//

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "e1000_dev.h"

#define TX_RING_SIZE 16
static struct tx_desc tx_ring[TX_RING_SIZE] __attribute__((aligned(16)));

#define RX_RING_SIZE 16
static struct rx_desc rx_ring[RX_RING_SIZE] __attribute__((aligned(16)));


static char *tx_bufs[TX_RING_SIZE];
static char *rx_bufs[RX_RING_SIZE];

// remember where the e1000's registers live.
static volatile uint32 *regs;

struct spinlock e1000_lock;

// called by pci_init().
// xregs is the memory address at which the
// e1000's registers are mapped.
// this code loosely follows the initialization directions
// in Chapter 14 of Intel's Software Developer's Manual.
void
e1000_init(uint32 *xregs)

{
  cprintf("e1000_init: ENTER xregs=%p\n", xregs);
  int i;

  initlock(&e1000_lock, "e1000");

  regs = xregs;

  // Reset the device
  regs[E1000_IMS] = 0; // disable interrupts
  regs[E1000_CTL] |= E1000_CTL_RST;
  regs[E1000_IMS] = 0; // redisable interrupts
  __sync_synchronize();

  // [E1000 14.5] Transmit initialization
  memset(tx_ring, 0, sizeof(tx_ring));
  for (i = 0; i < TX_RING_SIZE; i++) {
    tx_ring[i].status = E1000_TXD_STAT_DD;
    tx_ring[i].addr = 0;
    tx_bufs[i] = 0;
  }

  // set up TX ring base (physical address)
  uint64 tx_pa = (uint64)V2P(tx_ring);
  regs[E1000_TDBAL] = (uint32)tx_pa;
  regs[E1000_TDBAH] = (uint32)(tx_pa >> 32);
  regs[E1000_TDLEN] = sizeof(tx_ring);
  regs[E1000_TDH] = regs[E1000_TDT] = 0;
  
  // [E1000 14.4] Receive initialization
  memset(rx_ring, 0, sizeof(rx_ring));
  for (i = 0; i < RX_RING_SIZE; i++) {
    void *buf = kalloc();
    if (!buf)
      panic("e1000 rx kalloc");
    rx_bufs[i] = buf;                     // remember kvaddr
    rx_ring[i].addr = (uint64)V2P(buf);   // give NIC phys addr
    rx_ring[i].status = 0;
  }

  // set up RX ring base (physical address)
  uint64 rx_pa = (uint64)V2P(rx_ring);
  regs[E1000_RDBAL] = (uint32)rx_pa;
  regs[E1000_RDBAH] = (uint32)(rx_pa >> 32);
  regs[E1000_RDLEN] = sizeof(rx_ring);
  regs[E1000_RDH] = 0;
  regs[E1000_RDT] = RX_RING_SIZE - 1;

  // filter by qemu's MAC address, 52:54:00:12:34:56
  regs[E1000_RA] = 0x12005452;
  regs[E1000_RA+1] = 0x5634 | (1<<31);
  // multicast table
  for (int i = 0; i < 4096/32; i++)
    regs[E1000_MTA + i] = 0;

  // transmitter control bits.
  regs[E1000_TCTL] = E1000_TCTL_EN |  // enable
    E1000_TCTL_PSP |                  // pad short packets
    (0x10 << E1000_TCTL_CT_SHIFT) |   // collision stuff
    (0x40 << E1000_TCTL_COLD_SHIFT);
  regs[E1000_TIPG] = 10 | (8<<10) | (6<<20); // inter-pkt gap

  // receiver control bits.
  regs[E1000_RCTL] = E1000_RCTL_EN | // enable receiver
    E1000_RCTL_BAM |                 // enable broadcast
    E1000_RCTL_SZ_2048 |             // 2048-byte rx buffers
    E1000_RCTL_SECRC;                // strip CRC
  
  // ask e1000 for receive interrupts.
  regs[E1000_RDTR] = 0; // interrupt after every received packet (no timer)
  regs[E1000_RADV] = 0; // interrupt after every packet (no timer)
  regs[E1000_IMS] = (1 << 7); // RXDW -- Receiver Descriptor Write Back






    // ask e1000 for receive interrupts.
  regs[E1000_RDTR] = 0; // interrupt after every received packet (no timer)
  regs[E1000_RADV] = 0; // interrupt after every packet (no timer)
  regs[E1000_IMS]  = (1 << 7); // RXDW -- Receiver Descriptor Write Back

  // ---- debug sanity checks ----
  uint32 status = regs[0x00008 / 4];  // E1000_STATUS register at 0x00008
  cprintf("e1000_init: STATUS=0x%x\n", status);

  uint32 tdt_before = regs[E1000_TDT];
  cprintf("e1000_init: TDT before init=0x%x\n", tdt_before);
}

int
e1000_transmit(char *buf, int len)
{
  //
  // Your code here.
  //
  // buf contains an ethernet frame; program it into
  // the TX descriptor ring so that the e1000 sends it. Stash
  // a pointer so that it can be freed after send completes.
  //
  // return 0 on success.
  // return -1 on failure (e.g., there is no descriptor available)
  // so that the caller knows to free buf.
  //
  cprintf("e1000_transmit: enter buf=%p len=%d\n", buf, len);

  acquire(&e1000_lock);

  uint32 t_raw = regs[E1000_TDT];
  uint32 t = t_raw % TX_RING_SIZE;
  cprintf("e1000_transmit: raw TDT=%x -> idx=%d\n", t_raw, t);

  struct tx_desc *d = &tx_ring[t];

  if ((d->status & E1000_TXD_STAT_DD) == 0) {
    cprintf("e1000_transmit: no free desc at idx=%d\n", t);
    release(&e1000_lock);
    return -1;
  }

  if (tx_bufs[t])
    kfree(tx_bufs[t]);

  uint64 pa = V2P(buf);
  d->addr   = pa;
  d->length = len;
  d->cmd    = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;
  d->status = 0;
  tx_bufs[t] = buf;

  regs[E1000_TDT] = (t + 1) % TX_RING_SIZE;

  release(&e1000_lock); 
  return 0;
}

static void
e1000_recv(void)
{
  //
  // Your code here.
  //
  // Check for packets that have arrived from the e1000.
  // Create and deliver a buf for each packet (using net_rx()).
  //
  acquire(&e1000_lock);

  uint32 r = regs[E1000_RDT];
  uint32 i = (r + 1) % RX_RING_SIZE;

  for (;;) {
    struct rx_desc *d = &rx_ring[i];
    if ((d->status & E1000_RXD_STAT_DD) == 0)
      break;                     // no more packets

    int len = d->length;
    char *src = rx_bufs[i];

    // allocate a handoff buffer; if out of memory, drop packet
    char *dst = kalloc();
    if (dst != 0) {
      if (len > 0 && len <= PGSIZE)
        memmove(dst, src, len);
      net_rx(dst, len);          // net_rx takes ownership of dst
    }

    // give descriptor back to NIC
    d->status = 0;
    // (addr already points at src, so no need to change it)

    regs[E1000_RDT] = i;         // tell NIC it can reuse this slot
    i = (i + 1) % RX_RING_SIZE;

  }

  release(&e1000_lock);
}

void
e1000_intr(void)
{
  // tell the e1000 we've seen this interrupt;
  // without this the e1000 won't raise any
  // further interrupts.
  regs[E1000_ICR] = 0xffffffff;

  e1000_recv();
}
