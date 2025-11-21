# xv6 Network Driver

> [!IMPORTANT]
> This implementation is not on RISC-V, and instead uses the x86-64 architecture.

## Overview

This project extends the xv6 operating system by implementing a device driver for a Network Interface Card (NIC) and the receive half of an Ethernet/IP/UDP protocol stack.

Goal: Downloading a web page from the internet from the xv6 operating system!

## Usage

For testing that the implementation works as expected, follow the instructions from [this website](https://pdos.csail.mit.edu/6.1810/2025/labs/net.html).

> [!NOTE]
> The `tcpdump` command is not working yet.

### Instructions to run

1. Open two separate terminal windows (one for the Python testing script, and one for the xv6 operating system).
2. In both terminals, navigate to the `xv6-public` directory.
3. In the first terminal, run the `nettest` Python script.
    - `python3 nettest.py grade`
4. In the second terminal, run the xv6 kernel.
    - `make qemu-nox`
5. Once the second terminal has booted xv6, run the `nettest` program inside of the xv6 command line.
    - `nettest grade`

## Contributors

-   Logan Lucas
-   Ron Pham
-   Andrew Pizano
