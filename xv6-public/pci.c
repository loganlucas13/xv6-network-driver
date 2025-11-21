// ------------------------------------------------------------
// simple PCI initialization for x86_64 xv6
// Scans the PCI bus to find the Intel e1000 network device
// and initializes it via e1000_init().
// Works when QEMU is launched with: -device e1000
// ------------------------------------------------------------

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "defs.h"
#include "x86.h"
#include "traps.h"

// ------------------------------------------------------------
// PCI Configuration Space Access
// The CPU communicates with PCI devices using I/O ports 0xCF8 and 0xCFC.
// ------------------------------------------------------------
#define PCI_CONFIG_ADDR 0xCF8  // Port to specify the PCI config address
#define PCI_CONFIG_DATA 0xCFC  // Port to read/write the actual data

// ------------------------------------------------------------
// Macro to construct a PCI configuration address
// Fields:
//   bus:  always 0 for QEMU’s simple bus
//   dev:  device number (0–31)
//   func: function number (0–7), usually 0
//   off:  register offset within config space (must be 4-byte aligned)
// ------------------------------------------------------------
#define PCI_ADDR(bus, dev, func, off)                             \
    (0x80000000 | ((bus) << 16) | ((dev) << 11) | ((func) << 8) | \
     ((off) & 0xFC))

// ------------------------------------------------------------
// Common PCI register offsets (within each device’s config space)
// ------------------------------------------------------------
#define PCI_VENDOR_ID 0x00  // Vendor + Device ID
#define PCI_COMMAND 0x04  // Command register: enable I/O, memory, bus mastering
#define PCI_BAR0 0x10     // Base Address Register 0: MMIO base for device
#define PCI_INT_LINE 0x3C  // Interrupt line (IRQ number)

// ------------------------------------------------------------
// Intel E1000’s known PCI vendor/device identifiers
// ------------------------------------------------------------
#define E1000_VENDOR 0x8086
#define E1000_DEVICE 0x100E

// ------------------------------------------------------------
// Bits for PCI command register
// ------------------------------------------------------------
#define PCI_CMD_IO 0x1       // Enable I/O port space access
#define PCI_CMD_MEM 0x2      // Enable memory-mapped I/O
#define PCI_CMD_BUSMSTR 0x4  // Enable bus mastering (DMA)

// The e1000 driver init function, implemented elsewhere.
extern void
e1000_init(uint32* regs);

// ------------------------------------------------------------
// Read a 32-bit value from PCI configuration space
// ------------------------------------------------------------
static uint
pci_read(uint bus, uint dev, uint func, uint off) {
    // Write the configuration address to 0xCF8
    outl(PCI_CONFIG_ADDR, PCI_ADDR(bus, dev, func, off));

    // Then read the 32-bit value from 0xCFC
    return inl(PCI_CONFIG_DATA);
}

// ------------------------------------------------------------
// Write a 32-bit value to PCI configuration space
// ------------------------------------------------------------
static void
pci_write(uint bus, uint dev, uint func, uint off, uint val) {
    // Write the target address
    outl(PCI_CONFIG_ADDR, PCI_ADDR(bus, dev, func, off));

    // Write the data to the config data port
    outl(PCI_CONFIG_DATA, val);
}

// ------------------------------------------------------------
// pci_init()
// Scan PCI devices on bus 0 for Intel’s e1000 Ethernet controller.
// If found, configure it for memory access + DMA, then call e1000_init().
// ------------------------------------------------------------
void
pci_init(void) {
    // Iterate through all possible device slots on bus 0 (0–31)
    for (int dev = 0; dev < 32; dev++) {
        // Read the vendor/device ID (first 32 bits of config space)
        uint id = pci_read(0, dev, 0, PCI_VENDOR_ID);

        // If value is 0xFFFFFFFF → empty slot (no device present)
        if (id == 0xFFFFFFFF) continue;

        // Extract vendor and device IDs
        uint vendor = id & 0xFFFF;
        uint device = id >> 16;

        // Check if it matches Intel’s E1000 NIC
        if (vendor == E1000_VENDOR && device == E1000_DEVICE) {
            cprintf("pci: found e1000 device at slot %d\n", dev);

            // --------------------------------------------------------
            // Enable memory-mapped I/O (MMIO) and bus mastering (DMA)
            // so the NIC can access main memory directly.
            // --------------------------------------------------------
            uint cmd = pci_read(0, dev, 0, PCI_COMMAND);
            cmd |= (PCI_CMD_MEM | PCI_CMD_BUSMSTR);
            pci_write(0, dev, 0, PCI_COMMAND, cmd);

            // --------------------------------------------------------
            // Read the BAR0 register: this holds the *physical address*
            // of the NIC’s memory-mapped I/O region (MMIO).
            // Lower 4 bits are flags → mask them out.
            // --------------------------------------------------------
            uint bar0 = pci_read(0, dev, 0, PCI_BAR0) & ~0xF;
            // cprintf("pci: e1000 BAR0 phys=0x%x\n", bar0);

            // Convert physical MMIO address into a kernel virtual address
            // (xv6 maps all physical memory starting at KERNBASE)
            addr_t vbar = (addr_t)p2v((addr_t)bar0);
            // cprintf("pci: e1000 regs virt=%p\n", (void*)vbar);

            // Cast the mapped address into a pointer to NIC registers
            volatile uint32* regs = (volatile uint32*)vbar;

            // Initialize the NIC driver (sets up TX/RX rings, etc.)
            e1000_init((uint32*)regs);

            // --------------------------------------------------------
            // Get the IRQ line (interrupt number) assigned to this device
            // and enable it through the I/O APIC for CPU 0.
            // --------------------------------------------------------
            int irq = pci_read(0, dev, 0, PCI_INT_LINE) & 0xFF;
            ioapicenable(irq, 0);

            // Stop scanning after successful detection
            return;
        }
    }

    // If we reach this point, no supported device was found
    cprintf("pci: no e1000 device found\n");
}
