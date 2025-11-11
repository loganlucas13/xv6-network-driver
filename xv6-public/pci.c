// simple PCI initialization for x86_64 xv6
// finds the Intel e1000 NIC and calls e1000_init()
// works in QEMU (-device e1000)

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "defs.h"
#include "x86.h"

// PCI config space I/O ports
#define PCI_CONFIG_ADDR 0xCF8
#define PCI_CONFIG_DATA 0xCFC

// compose a config address (bus=0, dev 0–31, func 0)
#define PCI_ADDR(bus, dev, func, off) \
    (0x80000000 | ((bus) << 16) | ((dev) << 11) | ((func) << 8) | ((off) & 0xFC))

// PCI register offsets
#define PCI_VENDOR_ID 0x00
#define PCI_COMMAND   0x04
#define PCI_BAR0      0x10
#define PCI_INT_LINE  0x3C

// e1000 vendor/device IDs
#define E1000_VENDOR 0x8086
#define E1000_DEVICE 0x100E

// command register bits
#define PCI_CMD_IO       0x1
#define PCI_CMD_MEM      0x2
#define PCI_CMD_BUSMSTR  0x4
extern void e1000_init(uint32 *regs);

// read a 32-bit PCI config register
static uint
pci_read(uint bus, uint dev, uint func, uint off)
{
  outl(PCI_CONFIG_ADDR, PCI_ADDR(bus, dev, func, off));
  return inl(PCI_CONFIG_DATA);
}

// write a 32-bit PCI config register
static void
pci_write(uint bus, uint dev, uint func, uint off, uint val)
{
  outl(PCI_CONFIG_ADDR, PCI_ADDR(bus, dev, func, off));
  outl(PCI_CONFIG_DATA, val);
}

void
pci_init(void)
{
  for (int dev = 0; dev < 32; dev++) {
    uint id = pci_read(0, dev, 0, PCI_VENDOR_ID);
    if (id == 0xFFFFFFFF)
      continue; // empty slot

    uint vendor = id & 0xFFFF;
    uint device = id >> 16;

    if (vendor == E1000_VENDOR && device == E1000_DEVICE) {
      cprintf("pci: found e1000 device at slot %d\n", dev);

      // enable MMIO and bus mastering
      uint cmd = pci_read(0, dev, 0, PCI_COMMAND);
      cmd |= (PCI_CMD_MEM | PCI_CMD_BUSMSTR);
      pci_write(0, dev, 0, PCI_COMMAND, cmd);

      uint bar0 = pci_read(0, dev, 0, PCI_BAR0) & ~0xF;
      cprintf("pci: e1000 BAR0 phys=0x%x\n", bar0);

      addr_t vbar = (addr_t)p2v((addr_t)bar0);
      cprintf("pci: e1000 regs virt=%p\n", (void*)vbar);

      volatile uint32 *regs = (volatile uint32*) vbar;
      e1000_init((uint32*)regs);

      return;
    }
  }

  cprintf("pci: no e1000 device found\n");
}
