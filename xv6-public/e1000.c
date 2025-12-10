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

static char* tx_bufs[TX_RING_SIZE];  // transmit buffer
static char* rx_bufs[RX_RING_SIZE];  // receiver buffer

// remember where the e1000's registers live.
static volatile uint32* regs;

struct spinlock e1000_lock;

// called by pci_init().
// xregs is the memory address at which the
// e1000's registers are mapped.
// this code loosely follows the initialization directions
// in Chapter 14 of Intel's Software Developer's Manual.
void
e1000_init(uint32* xregs)

{
    cprintf("e1000_init: ENTER xregs=%p\n", xregs);
    int i;

    initlock(&e1000_lock, "e1000");

    regs = xregs;

    // Reset the device
    regs[E1000_IMS] = 0;  // disable interrupts
    regs[E1000_CTL] |= E1000_CTL_RST;
    regs[E1000_IMS] = 0;  // redisable interrupts
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
        void* buf = kalloc();
        if (!buf) panic("e1000 rx kalloc");
        rx_bufs[i] = buf;                    // remember kvaddr
        rx_ring[i].addr = (uint64)V2P(buf);  // give NIC phys addr
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
    regs[E1000_RA + 1] = 0x5634 | (1 << 31);
    // multicast table
    for (int i = 0; i < 4096 / 32; i++) regs[E1000_MTA + i] = 0;

    // transmitter control bits.
    regs[E1000_TCTL] = E1000_TCTL_EN |                  // enable
                       E1000_TCTL_PSP |                 // pad short packets
                       (0x10 << E1000_TCTL_CT_SHIFT) |  // collision stuff
                       (0x40 << E1000_TCTL_COLD_SHIFT);
    regs[E1000_TIPG] = 10 | (8 << 10) | (6 << 20);  // inter-pkt gap

    // receiver control bits.
    regs[E1000_RCTL] = E1000_RCTL_EN |       // enable receiver
                       E1000_RCTL_BAM |      // enable broadcast
                       E1000_RCTL_SZ_2048 |  // 2048-byte rx buffers
                       E1000_RCTL_SECRC;     // strip CRC

    // ask e1000 for receive interrupts.
    regs[E1000_RDTR] = 0;  // interrupt after every received packet (no timer)
    regs[E1000_RADV] = 0;  // interrupt after every packet (no timer)
    regs[E1000_IMS] = (1 << 7);  // RXDW -- Receiver Descriptor Write Back

    // ---- debug sanity checks ----
    uint32 status = regs[0x00008 / 4];  // E1000_STATUS register at 0x00008
    cprintf("e1000_init: STATUS=0x%x\n", status);

    uint32 tdt_before = regs[E1000_TDT];
}

int
e1000_transmit(char* buf, int len) {
    // buf contains an ethernet frame; program it into
    // the TX descriptor ring so that the e1000 sends it. Stash
    // a pointer so that it can be freed after send completes.
    //
    // return 0 on success.
    // return -1 on failure (e.g., there is no descriptor available)
    // so that the caller knows to free buf.
    //

    // Disable interrupts to prevent preemption while manipulating NIC state.
    pushcli();

    // Lock the E1000 driver so only one CPU can access TX structures at a time.
    acquire(&e1000_lock);

    // Read the hardware Transmit Descriptor Tail register.
    // This tells us where the NIC expects the next descriptor.
    uint32 t_raw = regs[E1000_TDT];

    // Convert the hardware tail index into our ring buffer index (0–15).
    uint32 t = t_raw % TX_RING_SIZE;

    // Get a pointer to the descriptor entry for this index.
    struct tx_desc* d = &tx_ring[t];

    // If the descriptor’s "Descriptor Done" bit is NOT set,
    // it means the NIC is still using this descriptor → no free slot.
    if ((d->status & E1000_TXD_STAT_DD) == 0) {
        release(&e1000_lock);  // unlock before returning
        popcli();              // re-enable interrupts
        return -1;             // tell caller to retry later
    }

    // If an old buffer is still stored here, free it.
    // Once DD=1, the NIC has finished with it, so it’s safe to release.
    if (tx_bufs[t]) kfree(tx_bufs[t]);

    // Program the descriptor with the new packet info.

    // Physical address of the packet buffer (NIC can only use physical memory)
    d->addr = V2P(buf);

    // Length of the Ethernet frame in bytes
    d->length = len;

    // Command bits:
    // EOP = end of packet (marks the last descriptor in this frame)
    // RS  = request status (NIC will set DD when done)
    d->cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;

    // Clear status bit (hardware will set it when transmission completes)
    d->status = 0;

    // Remember the buffer’s virtual address so we can free it later
    tx_bufs[t] = buf;

    // Advance the NIC’s transmit tail register.
    // This hands the descriptor to the NIC so it can start transmitting.
    regs[E1000_TDT] = (t + 1) % TX_RING_SIZE;

    // Done modifying TX state — release the driver lock.
    release(&e1000_lock);

    // Re-enable interrupts (restore previous interrupt state).
    popcli();

    // Return success.
    return 0;
}

// ------------------------------------------------------------
// e1000_receive()
// Continuously polls for packets that have been received by the NIC.
// For each completed RX descriptor, this function:
//   - Copies the received packet into a freshly allocated kernel buffer
//   - Hands it off to the xv6 network stack via net_rx()
//   - Returns the descriptor to the NIC for reuse
// ------------------------------------------------------------
static void
e1000_recv(void) {
    // Check for packets that have arrived from the e1000.
    // Create and deliver a buf for each packet (using net_rx()).
    //

    // Infinite polling loop — continuously check for received packets
    for (;;) {
        // Lock NIC state so that RX ring and register access are atomic
        acquire(&e1000_lock);

        // Read the NIC’s Receive Descriptor Tail register (RDT)
        // This is where the hardware is currently writing packets up to.
        uint32 r = regs[E1000_RDT];

        // Compute the next index in the RX ring
        // Wraps around (ring buffer of RX_RING_SIZE descriptors)
        uint32 i = (r + 1) % RX_RING_SIZE;

        // Get the descriptor at this index
        struct rx_desc* d = &rx_ring[i];

        // --------------------------------------------------------
        // Check if this descriptor has been marked “done” by the NIC.
        // E1000_RXD_STAT_DD is set when the hardware has finished
        // writing a received packet into this buffer.
        // --------------------------------------------------------
        if ((d->status & E1000_RXD_STAT_DD) == 0) {
            // No new packets available, release lock and break out
            release(&e1000_lock);
            break;
        }

        // --------------------------------------------------------
        // Extract packet info from the descriptor
        // --------------------------------------------------------
        int len = d->length;     // actual number of bytes received
        char* src = rx_bufs[i];  // pointer to the NIC’s receive buffer

        // --------------------------------------------------------
        // Copy the received packet into a new kernel buffer.
        // We must do this before returning the descriptor to the NIC,
        // since the NIC can overwrite the buffer once we clear the DD bit.
        // --------------------------------------------------------
        char* dst = 0;
        if (len > 0 && len <= PGSIZE) {  // sanity check on packet size
            dst = kalloc();              // allocate a fresh page for the packet
            if (dst != 0)
                memmove(dst, src, len);  // copy packet contents safely
        }

        // --------------------------------------------------------
        // Return descriptor back to NIC for reuse.
        // Clearing the DD bit allows the NIC to refill it.
        // The address (src) remains valid, so no need to reset addr.
        // --------------------------------------------------------
        d->status = 0;

        // Advance NIC's receive tail register so hardware knows
        // that descriptor i is now available for future packets.
        regs[E1000_RDT] = i;

        // Unlock after we’ve safely updated the RX ring
        release(&e1000_lock);

        // --------------------------------------------------------
        // Hand the packet to the upper layer (network stack)
        // outside the lock to avoid holding it too long.
        // net_rx() will take ownership of dst and eventually free it.
        // --------------------------------------------------------
        if (dst != 0) net_rx(dst, len);
    }
}

void
e1000_intr(void) {
    // tell the e1000 we've seen this interrupt;
    // without this the e1000 won't raise any
    // further interrupts.
    regs[E1000_ICR] = 0xffffffff;

    e1000_recv();
}
