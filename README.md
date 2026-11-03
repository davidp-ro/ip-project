# Internet Protocols Mini-Project - 2026

Simple implementation of a TCP client-server application using sockets in C.

- **Team:** David Pescariu; Alexandro Vidican (TSTen, IV, 2341)
- **IDs:**
  - `g=0 s=1 e=8`
  - `x=0 y=8`
- **Remote Resource:** [www.ietf.org](https://www.ietf.org/), expected command: `08#`

## Resources

- Requirements: https://el.el.obs.utcluj.ro/pi/pdf/miniproject_ip_2026.pdf
- Reference: https://el.el.obs.utcluj.ro/pi/pdf/Lab_5.pdf
- Webpage: https://el.el.obs.utcluj.ro/pi/en_index.htm

## Steps

- [`step1.c`](step1.c) - TCP client application that connects to the remote server using
  TCP over IPv6
- [`step2.c`](step2.c) - todo
- [`step3.c`](step3.c) - todo
- [`step4.c`](step4.c) - todo

## Running

- Run `make step<n>` to compile the corresponding step, for example `make step1` will compile `step1.c` into an executable named `step1`.
- Run `make` to view other available commands.
- **Prequisites:** `gcc` and `make`
