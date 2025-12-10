// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.

#include <stdarg.h>

#include "types.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "kbd.h"

static void
consputc(int);

static int panicked = 0;

static struct {
    struct spinlock lock;
    int locking;
} cons;

static char digits[] = "0123456789abcdef";

static void
print_x64(addr_t x) {
    int i;
    for (i = 0; i < (sizeof(addr_t) * 2); i++, x <<= 4)
        consputc(digits[x >> (sizeof(addr_t) * 8 - 4)]);
}

static void
print_x32(uint x) {
    int i;
    for (i = 0; i < (sizeof(uint) * 2); i++, x <<= 4)
        consputc(digits[x >> (sizeof(uint) * 8 - 4)]);
}

static void
print_d(int v) {
    char buf[16];
    int64 x = v;

    if (v < 0) x = -x;

    int i = 0;
    do {
        buf[i++] = digits[x % 10];
        x /= 10;
    } while (x != 0);

    if (v < 0) buf[i++] = '-';

    while (--i >= 0) consputc(buf[i]);
}
// PAGEBREAK: 50

// Print to the console. only understands %d, %x, %p, %s.
void
cprintf(char* fmt, ...) {
    va_list ap;
    int i, c, locking;
    char* s;

    va_start(ap, fmt);

    locking = cons.locking;
    if (locking) acquire(&cons.lock);

    if (fmt == 0) panic("null fmt");

    for (i = 0; (c = fmt[i] & 0xff) != 0; i++) {
        if (c != '%') {
            consputc(c);
            continue;
        }
        c = fmt[++i] & 0xff;
        if (c == 0) break;
        switch (c) {
            case 'd':
                print_d(va_arg(ap, int));
                break;
            case 'x':
                print_x32(va_arg(ap, uint));
                break;
            case 'p':
                print_x64(va_arg(ap, addr_t));
                break;
            case 's':
                if ((s = va_arg(ap, char*)) == 0) s = "(null)";
                while (*s) consputc(*(s++));
                break;
            case '%':
                consputc('%');
                break;
            default:
                // Print unknown % sequence to draw attention.
                consputc('%');
                consputc(c);
                break;
        }
    }

    if (locking) release(&cons.lock);
}

__attribute__((noreturn)) void
panic(char* s) {
    int i;
    addr_t pcs[10];

    cli();
    cons.locking = 0;
    cprintf("cpu%d: panic: ", cpu->id);
    cprintf(s);
    cprintf("\n");
    getcallerpcs(&s, pcs);
    for (i = 0; i < 10; i++) cprintf(" %p\n", pcs[i]);
    panicked = 1;  // freeze other CPU
    for (;;) hlt();
}

// PAGEBREAK: 50
#define BACKSPACE 0x100
#define CRTPORT 0x3d4
static ushort* crt = (ushort*)P2V(0xb8000);  // CGA memory

static void
cgaputc(int c) {
    int pos;

    // Cursor position: col + 80*row.
    outb(CRTPORT, 14);
    pos = inb(CRTPORT + 1) << 8;
    outb(CRTPORT, 15);
    pos |= inb(CRTPORT + 1);

    if (c == '\n')
        pos += 80 - pos % 80;
    else if (c == BACKSPACE) {
        if (pos > 0) --pos;
    }
    else
        crt[pos++] = (c & 0xff) | 0x0700;  // gray on black

    if ((pos / 80) >= 24) {  // Scroll up.
        memmove(crt, crt + 80, sizeof(crt[0]) * 23 * 80);
        pos -= 80;
        memset(crt + pos, 0, sizeof(crt[0]) * (24 * 80 - pos));
    }

    outb(CRTPORT, 14);
    outb(CRTPORT + 1, pos >> 8);
    outb(CRTPORT, 15);
    outb(CRTPORT + 1, pos);
    crt[pos] = ' ' | 0x0700;
}

void
consputc(int c) {
    if (panicked) {
        cli();
        for (;;) hlt();
    }

    if (c == BACKSPACE) {
        uartputc('\b');
        uartputc(' ');
        uartputc('\b');
    }
    else
        uartputc(c);
    cgaputc(c);
}

#define INPUT_BUF 128
struct {
    struct spinlock lock;
    char buf[INPUT_BUF];
    uint r;  // Read index
    uint w;  // Write index
    uint e;  // Edit index
} input;

#define HISTORY_SIZE 16
static char history[HISTORY_SIZE][INPUT_BUF];
static int history_len = 0;   // number of stored commands
static int history_pos = -1;  // -1 = not browsing history
static uint line_start =
    0;  // offset in input.e space where current line begins

static uint cursor = 0;  // current cursor position (logical index)

#define C(x) ((x) - '@')  // Control-x

// Erase current line from screen and from input.buf back to line_start
static void
clear_line(void) {
    while (input.e != line_start) {
        input.e--;
        consputc(BACKSPACE);
    }
    cursor = line_start;
}

// Load history[idx] into the current editing line
static void
load_history(int idx) {
    int j;

    if (idx < 0 || idx >= history_len) return;

    clear_line();

    for (j = 0; history[idx][j] != 0 && input.e - input.r < INPUT_BUF; j++) {
        char c = history[idx][j];
        input.buf[input.e % INPUT_BUF] = c;
        input.e++;
        consputc(c);
    }
    cursor = input.e;
}

void
consoleintr(int (*getc)(void)) {
    int c;

    acquire(&input.lock);
    while ((c = getc()) >= 0) {
        switch (c) {
            case C('Z'):  // reboot
                lidt(0, 0);
                break;
            case C('P'):  // Process listing.
                procdump();
                break;
            case C('U'):  // Kill line.
                while (input.e != input.w &&
                       input.buf[(input.e - 1) % INPUT_BUF] != '\n') {
                    input.e--;
                    consputc(BACKSPACE);
                }
                break;
            case C('H'):
            case '\x7f':  // Backspace
                if (input.e != input.w) {
                    input.e--;
                    consputc(BACKSPACE);
                }
                break;
            case '\t':
                // Queue TAB into input buffer
                if (input.e - input.r < INPUT_BUF) {
                    input.buf[input.e++ % INPUT_BUF] = c;
                }
                break;
            case KEY_UP:
                if (history_len > 0) {
                    // first time: jump to most recent command
                    if (history_pos == -1)
                        history_pos = history_len - 1;
                    else if (history_pos > 0)
                        history_pos--;  // move to older command

                    load_history(history_pos);
                }
                break;

            case KEY_DN:
                if (history_pos != -1) {
                    if (history_pos < history_len - 1) {
                        history_pos++;  // move to newer command
                        load_history(history_pos);
                    }
                    else {
                        // at newest; leave history and go back to empty "live"
                        // line
                        history_pos = -1;
                        clear_line();
                    }
                }
                break;
            default:
                if (c != 0 && input.e - input.r < INPUT_BUF) {
                    c = (c == '\r') ? '\n' : c;
                    input.buf[input.e++ % INPUT_BUF] = c;
                    consputc(c);
                    if (c == '\n' || c == C('D') ||
                        input.e == input.r + INPUT_BUF) {
                        input.w = input.e;
                        wakeup(&input.r);

                        if (c == '\n') {
                            // Extract the line we just finished (without '\n')
                            char line[INPUT_BUF];
                            int len = 0;
                            uint i;

                            for (i = line_start;
                                 i != input.e - 1 && len < INPUT_BUF - 1; i++) {
                                line[len++] = input.buf[i % INPUT_BUF];
                            }
                            line[len] = 0;

                            if (len > 0) {
                                // If history full, drop oldest (index 0) by
                                // shifting left
                                if (history_len == HISTORY_SIZE) {
                                    for (int j = 1; j < HISTORY_SIZE; j++)
                                        safestrcpy(history[j - 1], history[j],
                                                   INPUT_BUF);
                                    history_len--;
                                }
                                safestrcpy(history[history_len], line,
                                           INPUT_BUF);
                                history_len++;
                            }

                            history_pos = -1;  // leave history browse mode
                            line_start =
                                input.e;  // next char is start of new line
                            cursor = input.e;
                        }
                    }
                }
                break;
        }
    }
    release(&input.lock);
}

int
consoleread(struct inode* ip, uint off, char* dst, int n) {
    uint target;
    int c;

    iunlock(ip);
    target = n;
    acquire(&input.lock);
    while (n > 0) {
        while (input.r == input.w) {
            if (proc->killed) {
                release(&input.lock);
                ilock(ip);
                return -1;
            }
            sleep(&input.r, &input.lock);
        }
        c = input.buf[input.r++ % INPUT_BUF];
        if (c == C('D')) {  // EOF
            if (n < target) {
                // Save ^D for next time, to make sure
                // caller gets a 0-byte result.
                input.r--;
            }
            break;
        }
        *dst++ = c;
        --n;
        if (c == '\n') break;
    }
    release(&input.lock);
    ilock(ip);

    return target - n;
}

int
consolewrite(struct inode* ip, uint off, char* buf, int n) {
    int i;

    iunlock(ip);
    acquire(&cons.lock);
    for (i = 0; i < n; i++) consputc(buf[i] & 0xff);
    release(&cons.lock);
    ilock(ip);

    return n;
}

void
consoleinit(void) {
    initlock(&cons.lock, "console");
    initlock(&input.lock, "input");

    devsw[CONSOLE].write = consolewrite;
    devsw[CONSOLE].read = consoleread;
    cons.locking = 1;

    input.r = input.w = input.e = 0;
    line_start = 0;
    cursor = 0;

    ioapicenable(IRQ_KBD, 0);
}
