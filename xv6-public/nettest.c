//
// network tests
// to be used with nettest.py (run outside of qemu)
//

#include "types.h"
#include "net.h"
#include "stat.h"
#include "user.h"
//#include "string.h"

// ---------- printing & syscall prototypes ----------
// xv6 printf signature is (int fd, char *fmt, ...)
#define uprintf(...) printf(1, __VA_ARGS__)
#define eprintf(...) printf(2, __VA_ARGS__)



// If not provided by Makefile (recommended), fall back to a default so it compiles.
// Makefile preferred line:
//   CFLAGS += -DNET_TESTS_PORT=$(shell expr $$(id -u) % 5000 + 25099)
// #ifndef NET_TESTS_PORT
// #warning "NET_TESTS_PORT not defined; defaulting to 26000 (may not match nettest.py)"
// #define NET_TESTS_PORT 26000
// #endif

//
// send a single UDP packet (but don't recv() the reply).
// python3 nettest.py txone can be used to wait for
// this packet, and you can also see what
// happened with tcpdump -XXnr packets.pcap
//


int
memcmp(const void *v1, const void *v2, uint n)
{
  const uchar *s1, *s2;

  s1 = v1;
  s2 = v2;
  while(n-- > 0){
    if(*s1 != *s2)
      return *s1 - *s2;
    s1++, s2++;
  }

  return 0;
}


void
txone(void)
{
  uprintf("txone: sending one packet\n");
  uint32 dst = 0x0A000202; // 10.0.2.2
  int dport = NET_TESTS_PORT;
  char buf[5];
  buf[0] = 't';
  buf[1] = 'x';
  buf[2] = 'o';
  buf[3] = 'n';
  buf[4] = 'e';
  if (send(2003, dst, dport, buf, 5) < 0) {
    eprintf("txone: send() failed\n");
  }
}

//
// test just receive.
// outside of qemu, run
//   ./nettest.py rx
//
int
rx(char *name)
{
  bind(2000);

  int lastseq = -1;
  int ok = 0;

  while (ok < 4) {
    char ibuf[128];
    uint32 src;
    ushort sport;
    int cc = recv(2000, &src, &sport, ibuf, sizeof(ibuf)-1);
    if (cc < 0) {
      eprintf("nettest %s: recv() failed\n", name);
      return 0;
    }

    if (src != 0x0A000202) { // 10.0.2.2
      uprintf("wrong ip src %x\n", src);
      return 0;
    }

    if (cc < strlen("packet 1")) {
      uprintf("len %d too short\n", cc);
      return 0;
    }

    if (cc > strlen("packet xxxxxx")) {
      uprintf("len %d too long\n", cc);
      return 0;
    }

    if (memcmp(ibuf, "packet ", strlen("packet ")) != 0) {
      uprintf("packet doesn't start with packet\n");
      return 0;
    }

    ibuf[cc] = '\0';
#define isdigit(x) ((x) >= '0' && (x) <= '9')
    if (!isdigit(ibuf[7])) {
      uprintf("packet doesn't contain a number\n");
      return 0;
    }
    for (int i = 7; i < cc; i++) {
      if (!isdigit(ibuf[i])) {
        uprintf("packet contains non-digits in the number\n");
        return 0;
      }
    }
    int seq = ibuf[7] - '0';
    if (isdigit(ibuf[8])) {
      seq *= 10;
      seq += ibuf[8] - '0';
      if (isdigit(ibuf[9])) {
        seq *= 10;
        seq += ibuf[9] - '0';
      }
    }

    if (lastseq != -1) {
      if (seq != lastseq + 1) {
        uprintf("got seq %d, expecting %d\n", seq, lastseq + 1);
        return 0;
      }
    }

    lastseq = seq;
    ok += 1;
  }

  uprintf("%s: OK\n", name);
  return 1;
}

//
// test receive on two different ports, interleaved.
// outside of qemu, run
//   ./nettest.py rx2
//
int
rx2(void)
{
  bind(2000);
  bind(2001);

  for (int i = 0; i < 3; i++) {
    char ibuf[128];
    uint32 src;
    ushort sport;
    int cc = recv(2000, &src, &sport, ibuf, sizeof(ibuf)-1);
    if (cc < 0) {
      eprintf("nettest rx2: recv() failed\n");
      return 0;
    }

    if (src != 0x0A000202) { // 10.0.2.2
      uprintf("wrong ip src %x\n", src);
      return 0;
    }

    if (cc < strlen("one 1")) {
      uprintf("len %d too short\n", cc);
      return 0;
    }

    if (cc > strlen("one xxxxxx")) {
      uprintf("len %d too long\n", cc);
      return 0;
    }

    if (memcmp(ibuf, "one ", strlen("one ")) != 0) {
      uprintf("packet doesn't start with one\n");
      return 0;
    }
  }

  for (int i = 0; i < 3; i++) {
    char ibuf[128];
    uint32 src;
    ushort sport;
    int cc = recv(2001, &src, &sport, ibuf, sizeof(ibuf)-1);
    if (cc < 0) {
      eprintf("nettest rx2: recv() failed\n");
      return 0;
    }

    if (src != 0x0A000202) { // 10.0.2.2
      uprintf("wrong ip src %x\n", src);
      return 0;
    }

    if (cc < strlen("one 1")) {
      uprintf("len %d too short\n", cc);
      return 0;
    }

    if (cc > strlen("one xxxxxx")) {
      uprintf("len %d too long\n", cc);
      return 0;
    }

    if (memcmp(ibuf, "two ", strlen("two ")) != 0) {
      uprintf("packet doesn't start with two\n");
      return 0;
    }
  }

  for (int i = 0; i < 3; i++) {
    char ibuf[128];
    uint32 src;
    ushort sport;
    int cc = recv(2000, &src, &sport, ibuf, sizeof(ibuf)-1);
    if (cc < 0) {
      eprintf("nettest rx2: recv() failed\n");
      return 0;
    }

    if (src != 0x0A000202) { // 10.0.2.2
      uprintf("wrong ip src %x\n", src);
      return 0;
    }

    if (cc < strlen("one 1")) {
      uprintf("len %d too short\n", cc);
      return 0;
    }

    if (cc > strlen("one xxxxxx")) {
      uprintf("len %d too long\n", cc);
      return 0;
    }

    if (memcmp(ibuf, "one ", strlen("one ")) != 0) {
      uprintf("packet doesn't start with one\n");
      return 0;
    }
  }

  uprintf("rx2: OK\n");
  return 1;
}

//
// send some UDP packets to nettest.py tx.
//
int
tx(void)
{
  for (int ii = 0; ii < 5; ii++) {
    uint32 dst = 0x0A000202; // 10.0.2.2
    int dport = NET_TESTS_PORT;
    char buf[3];
    buf[0] = 't';
    buf[1] = ' ';
    buf[2] = '0' + ii;
    if (send(2000, dst, dport, buf, 3) < 0) {
      eprintf("send() failed\n");
      return 0;
    }
    sleep(10);
  }
  // can't actually tell if the packets arrived.
  return 1;
}

//
// send just one UDP packets to nettest.py ping,
// expect a reply.
// nettest.py ping must be started first.
//
int
ping0(void)
{
  uprintf("ping0: starting\n");

  bind(2004);

  uint32 dst = 0x0A000202; // 10.0.2.2
  int dport = NET_TESTS_PORT;
  char buf[5];
  memmove(buf, "ping0", sizeof(buf));
  if (send(2004, dst, dport, buf, sizeof(buf)) < 0) {
    eprintf("ping0: send() failed\n");
    return 0;
  }

  char ibuf[128];
  uint32 src = 0;
  ushort sport = 0;
  memset(ibuf, 0, sizeof(ibuf));
  int cc = recv(2004, &src, &sport, ibuf, sizeof(ibuf)-1);
  if (cc < 0) {
    eprintf("ping0: recv() failed\n");
    return 0;
  }

  if (src != 0x0A000202) { // 10.0.2.2
    uprintf("ping0: wrong ip src %x, expecting %x\n", src, 0x0A000202);
    return 0;
  }

  if (sport != NET_TESTS_PORT) {
    uprintf("ping0: wrong sport %d, expecting %d\n", sport, NET_TESTS_PORT);
    return 0;
  }

  if (memcmp(buf, ibuf, sizeof(buf)) != 0) {
    uprintf("ping0: wrong content\n");
    return 0;
  }

  if (cc != (int)sizeof(buf)) {
    uprintf("ping0: wrong length %d, expecting %d\n", cc, (int)sizeof(buf));
    return 0;
  }

  uprintf("ping0: OK\n");
  return 1;
}

//
// send many UDP packets to nettest.py ping,
// expect a reply to each.
// nettest.py ping must be started first.
//
int
ping1(void)
{
  uprintf("ping1: starting\n");

  bind(2005);

  for (int ii = 0; ii < 20; ii++) {
    uint32 dst = 0x0A000202; // 10.0.2.2
    int dport = NET_TESTS_PORT;
    char buf[3];
    buf[0] = 'p';
    buf[1] = ' ';
    buf[2] = '0' + ii;
    if (send(2005, dst, dport, buf, 3) < 0) {
      eprintf("ping1: send() failed\n");
      return 0;
    }

    char ibuf[128];
    uint32 src = 0;
    ushort sport = 0;
    memset(ibuf, 0, sizeof(ibuf));
    int cc = recv(2005, &src, &sport, ibuf, sizeof(ibuf)-1);
    if (cc < 0) {
      eprintf("ping1: recv() failed\n");
      return 0;
    }

    if (src != 0x0A000202) { // 10.0.2.2
      uprintf("ping1: wrong ip src %x, expecting %x\n", src, 0x0A000202);
      return 0;
    }

    if (sport != NET_TESTS_PORT) {
      uprintf("ping1: wrong sport %d, expecting %d\n", sport, NET_TESTS_PORT);
      return 0;
    }

    if (memcmp(buf, ibuf, 3) != 0) {
      uprintf("ping1: wrong content\n");
      return 0;
    }

    if (cc != 3) {
      uprintf("ping1: wrong length %d, expecting 3\n", cc);
      return 0;
    }
  }

  uprintf("ping1: OK\n");
  return 1;
}

//
// send UDP packets from two different ports to nettest.py ping,
// expect a reply to each to appear on the correct port.
// nettest.py ping must be started first.
//
int
ping2(void)
{
  uprintf("ping2: starting\n");

  bind(2006);
  bind(2007);

  for (int ii = 0; ii < 5; ii++) {
    for (int port = 2006; port <= 2007; port++) {
      uint32 dst = 0x0A000202; // 10.0.2.2
      int dport = NET_TESTS_PORT;
      char buf[4];
      buf[0] = 'p';
      buf[1] = ' ';
      buf[2] = (port == 2006 ? 'a' : 'A') + ii;
      buf[3] = '!';
      if (send(port, dst, dport, buf, 4) < 0) {
        eprintf("ping2: send() failed\n");
        return 0;
      }
    }
  }

  for (int port = 2006; port <= 2007; port++) {
    for (int ii = 0; ii < 5; ii++) {
      char ibuf[128];
      uint32 src = 0;
      ushort sport = 0;
      memset(ibuf, 0, sizeof(ibuf));
      int cc = recv(port, &src, &sport, ibuf, sizeof(ibuf)-1);
      if (cc < 0) {
        eprintf("ping2: recv() failed\n");
        return 0;
      }

      if (src != 0x0A000202) { // 10.0.2.2
        uprintf("ping2: wrong ip src %x\n", src);
        return 0;
      }

      if (sport != NET_TESTS_PORT) {
        uprintf("ping2: wrong sport %d\n", sport);
        return 0;
      }

      if (cc != 4) {
        uprintf("ping2: wrong length %d\n", cc);
        return 0;
      }

      char expect[4];
      expect[0] = 'p';
      expect[1] = ' ';
      expect[2] = (port == 2006 ? 'a' : 'A') + ii;
      expect[3] = '!';
      if (memcmp(expect, ibuf, 3) != 0) {
        uprintf("ping2: wrong content\n");
        return 0;
      }
    }
  }

  uprintf("ping2: OK\n");
  return 1;
}

//
// send a big burst of packets from ports 2008 and 2010,
// causing drops,
// bracketed by two packets from port 2009.
// check that the two packets can be recv()'d on port 2009.
// check that port 2008 had a finite queue length (dropped some).
// nettest.py ping must be started first.
//
int
ping3(void)
{
  uprintf("ping3: starting\n");

  bind(2008);
  bind(2009);

  // send one packet on 2009.
  {
    uint32 dst = 0x0A000202; // 10.0.2.2
    int dport = NET_TESTS_PORT;
    char buf[4];
    buf[0] = 'p';
    buf[1] = ' ';
    buf[2] = 'A';
    buf[3] = '!';
    if (send(2009, dst, dport, buf, 4) < 0) {
      eprintf("ping3: send() failed\n");
      return 0;
    }
  }
  sleep(1);

  // big burst from 2008 and 2010
  for (int ii = 0; ii < 257; ii++) {
    uint32 dst = 0x0A000202; // 10.0.2.2
    int dport = NET_TESTS_PORT;
    char buf[4];
    buf[0] = 'p';
    buf[1] = ' ';
    buf[2] = 'a' + ii;
    buf[3] = '!';
    int port = 2008 + (ii % 2) * 2;
    if (send(port, dst, dport, buf, 4) < 0) {
      eprintf("ping3: send() failed\n");
      return 0;
    }
  }
  sleep(1);

  // another packet from 2009.
  {
    uint32 dst = 0x0A000202; // 10.0.2.2
    int dport = NET_TESTS_PORT;
    char buf[4];
    buf[0] = 'p';
    buf[1] = ' ';
    buf[2] = 'B';
    buf[3] = '!';
    if (send(2009, dst, dport, buf, 4) < 0) {
      eprintf("ping3: send() failed\n");
      return 0;
    }
  }

  // did both reply packets for 2009 arrive?
  for (int ii = 0; ii < 2; ii++) {
    char ibuf[128];
    uint32 src = 0;
    ushort sport = 0;
    memset(ibuf, 0, sizeof(ibuf));
    int cc = recv(2009, &src, &sport, ibuf, sizeof(ibuf)-1);
    if (cc < 0) {
      eprintf("ping3: recv() failed\n");
      return 0;
    }

    if (src != 0x0A000202) { // 10.0.2.2
      uprintf("ping3: wrong ip src %x\n", src);
      return 0;
    }

    if (sport != NET_TESTS_PORT) {
      uprintf("ping3: wrong sport %d\n", sport);
      return 0;
    }

    if (cc != 4) {
      uprintf("ping3: wrong length %d\n", cc);
      return 0;
    }

    char expect[4];
    expect[0] = 'p';
    expect[1] = ' ';
    expect[2] = 'A' + ii;
    expect[3] = '!';
    if (memcmp(expect, ibuf, 3) != 0) {
      uprintf("ping3: wrong content\n");
      return 0;
    }
  }

  // now count how many replies were queued for 2008.
  int fds[2];
  pipe(fds);
  int pid = fork();
  if (pid == 0) {
    close(fds[0]);
    write(fds[1], ":", 1); // ensure parent's read() doesn't block
    while (1) {
      char ibuf[128];
      uint32 src = 0;
      ushort sport = 0;
      memset(ibuf, 0, sizeof(ibuf));
      int cc = recv(2008, &src, &sport, ibuf, sizeof(ibuf)-1);
      if (cc < 0) {
        uprintf("ping3: recv failed\n");
        break;
      }
      write(fds[1], "x", 1);
    }
    exit();
  }
  close(fds[1]);

  sleep(5);
  static char nbuf[512];
  int n = read(fds[0], nbuf, sizeof(nbuf));
  close(fds[0]);
  kill(pid);

  n -= 1; // the ":"
  if (n > 16) {
    uprintf("ping3: too many packets (%d) were queued on a UDP port\n", n);
    return 0;
  }

  uprintf("ping3: OK\n");
  return 1;
}

// Encode a DNS name
void
encode_qname(char *qn, char *host)
{
  char *l = host;

  for (char *c = host; c < host+strlen(host)+1; c++) {
    if (*c == '.') {
      *qn++ = (char) (c-l);
      for (char *d = l; d < c; d++) {
        *qn++ = *d;
      }
      l = c+1; // skip .
    }
  }
  *qn = '\0';
}

// Decode a DNS name
void
decode_qname(char *qn, int max)
{
  char *qnMax = qn + max;
  while (1) {
    if (qn >= qnMax) {
      uprintf("invalid DNS reply\n");
      exit();
    }
    int l = *qn;
    if (l == 0)
      break;
    for (int i = 0; i < l; i++) {
      *qn = *(qn+1);
      qn++;
    }
    *qn++ = '.';
  }
}

// Make a DNS request
int
dns_req(uint *obuf)
{
  int len = 0;

  struct dns *hdr = (struct dns *) obuf;
  hdr->id = htons(6828);
  hdr->rd = 1;
  hdr->qdcount = htons(1);

  len += sizeof(struct dns);

  // qname part of question
  char *qname = (char *) (obuf + sizeof(struct dns));
  char *s = "pdos.csail.mit.edu.";
  encode_qname(qname, s);
  len += strlen(qname) + 1;

  // constants part of question
  struct dns_question *h = (struct dns_question *) (qname+strlen(qname)+1);
  h->qtype = htons(0x1);
  h->qclass = htons(0x1);

  len += sizeof(struct dns_question);
  return len;
}

// Process DNS response
int
dns_rep(uint *ibuf, int cc)
{
  struct dns *hdr = (struct dns *) ibuf;
  int len;
  char *qname = 0;
  int record = 0;

  if (cc < (int)sizeof(struct dns)) {
    uprintf("DNS reply too short\n");
    return 0;
  }

  if (!hdr->qr) {
    uprintf("Not a DNS reply for %d\n", ntohs(hdr->id));
    return 0;
  }

  if (hdr->id != htons(6828)) {
    uprintf("DNS wrong id: %d\n", ntohs(hdr->id));
    return 0;
  }

  if (hdr->rcode != 0) {
    uprintf("DNS rcode error: %x\n", hdr->rcode);
    return 0;
  }

  len = sizeof(struct dns);

  for (int i = 0; i < ntohs(hdr->qdcount); i++) {
    char *qn = (char *) (ibuf+len);
    qname = qn;
    decode_qname(qn, cc - len);
    len += strlen(qn)+1;
    len += sizeof(struct dns_question);
  }

  for (int i = 0; i < ntohs(hdr->ancount); i++) {
    if (len >= cc) {
      uprintf("dns: invalid DNS reply\n");
      return 0;
    }

    char *qn = (char *) (ibuf+len);

    if ((int)qn[0] > 63) {  // compression?
      qn = (char *)(ibuf+qn[1]);
      len += 2;
    } else {
      decode_qname(qn, cc - len);
      len += strlen(qn)+1;
    }

    struct dns_data *d = (struct dns_data *) (ibuf+len);
    len += sizeof(struct dns_data);
    if (ntohs(d->type) == ARECORD && ntohs(d->len) == 4) {
      record = 1;
      uprintf("DNS arecord for %s is ", qname ? qname : "" );
      uint *ip = (ibuf+len);
      uprintf("%d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
      if (ip[0] != 128 || ip[1] != 52 || ip[2] != 129 || ip[3] != 126) {
        uprintf("dns: wrong ip address");
        return 0;
      }
      len += 4;
    }
  }

  // needed for DNS servers with EDNS support
  for (int i = 0; i < ntohs(hdr->arcount); i++) {
    char *qn = (char *) (ibuf+len);
    if (*qn != 0) {
      uprintf("dns: invalid name for EDNS\n");
      return 0;
    }
    len += 1;

    struct dns_data *d = (struct dns_data *) (ibuf+len);
    len += sizeof(struct dns_data);
    if (ntohs(d->type) != 41) {
      uprintf("dns: invalid type for EDNS\n");
      return 0;
    }
    len += ntohs(d->len);
  }

  if (len != cc) {
    uprintf("dns: processed %d data bytes but received %d\n", len, cc);
    return 0;
  }
  if (!record) {
    uprintf("dns: didn't receive an arecord\n");
    return 0;
  }

  return 1;
}

int
dns(void)
{
#define N 1000
  uint obuf[N];
  uint ibuf[N];
  uint32 dst;
  int len;

  uprintf("dns: starting\n");

  memset(obuf, 0, N);
  memset(ibuf, 0, N);

  // 8.8.8.8: google's name server
  dst = (8 << 24) | (8 << 16) | (8 << 8) | (8 << 0);

  len = dns_req(obuf);

  bind(10000);

  if (send(10000, dst, 53, (char*)obuf, len) < 0) {
    eprintf("dns: send() failed\n");
    return 0;
  }

  uint32 src;
  ushort sport;
  int cc = recv(10000, &src, &sport, (char*)ibuf, sizeof(ibuf));
  if (cc < 0) {
    eprintf("dns: recv() failed\n");
    return 0;
  }

  if (dns_rep(ibuf, cc)) {
    uprintf("dns: OK\n");
    return 1;
  } else {
    return 0;
  }
}

void
usage(void)
{
  uprintf("Usage: nettest txone\n");
  uprintf("       nettest tx\n");
  uprintf("       nettest rx\n");
  uprintf("       nettest rx2\n");
  uprintf("       nettest rxburst\n");
  uprintf("       nettest ping1\n");
  uprintf("       nettest ping2\n");
  uprintf("       nettest ping3\n");
  uprintf("       nettest dns\n");
  uprintf("       nettest grade\n");
  exit();
}

//
// use sbrk() to count how many free physical memory pages there are.
//
int
countfree(void)
{
  int fds[2];

  if (pipe(fds) < 0) {
    uprintf("pipe() failed in countfree()\n");
    exit();
  }

  int pid = fork();
  if (pid < 0) {
    uprintf("fork failed in countfree()\n");
    exit();
  }

  if (pid == 0) {
    close(fds[0]);

    while (1) {
      uint64 a = (uint64) sbrk(4096);
      if (a == 0xffffffffffffffffULL) {
        break;
      }
      *(char *)(a + 4096 - 1) = 1;
      if (write(fds[1], "x", 1) != 1) {
        uprintf("write() failed in countfree()\n");
        exit();
      }
    }
    exit();
  }

  close(fds[1]);

  int n = 0;
  while (1) {
    char c;
    int cc = read(fds[0], &c, 1);
    if (cc < 0) {
      uprintf("read() failed in countfree()\n");
      exit();
    }
    if (cc == 0)
      break;
    n += 1;
  }

  close(fds[0]);
  wait();
  return n;
}

int
main(int argc, char *argv[])
{
  if (argc != 2)
    usage();

  if      (strcmp(argv[1], "txone") == 0)  txone();
  else if (strcmp(argv[1], "rx") == 0 || strcmp(argv[1], "rxburst") == 0) rx(argv[1]);
  else if (strcmp(argv[1], "rx2") == 0)   rx2();
  else if (strcmp(argv[1], "tx") == 0)    tx();
  else if (strcmp(argv[1], "ping0") == 0) ping0();
  else if (strcmp(argv[1], "ping1") == 0) ping1();
  else if (strcmp(argv[1], "ping2") == 0) ping2();
  else if (strcmp(argv[1], "ping3") == 0) ping3();
  else if (strcmp(argv[1], "grade") == 0) {
    // "python3 nettest.py grade" must already be running...
    int free0 = countfree();
    int free1 = 0;
    txone(); sleep(2);
    ping0(); sleep(2);
    ping1(); sleep(2);
    ping2(); sleep(2);
    ping3(); sleep(2);
    dns();   sleep(2);
    if ((free1 = countfree()) + 32 < free0) {
      uprintf("free: FAILED -- lost too many free pages %d (out of %d)\n", free1, free0);
    } else {
      uprintf("free: OK\n");
    }
  } else if (strcmp(argv[1], "dns") == 0) {
    dns();
  } else {
    usage();
  }

  exit();
}
