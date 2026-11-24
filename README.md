# Internet Protocols Mini-Project - 2026

Simple implementation of a TCP client-server application using sockets in C.

- **Team:** David Pescariu; Alexandro Vidican (TSTen, IV, 2341)
- **IDs:**
  - `g=0 s=1 e=8`
  - `x=0 y=8`
- **Remote Resource:** [www.ietf.org](https://www.ietf.org/), expected command: `08#`
  - www.ietf.org doesn't support HTTP/1.0, so instead:
  - [www.google.com](https://www.google.com/) is used instead from step 2 onwards.

## Resources

- Requirements: https://el.el.obs.utcluj.ro/pi/pdf/miniproject_ip_2026.pdf
- Reference: https://el.el.obs.utcluj.ro/pi/pdf/Lab_5.pdf
- Webpage: https://el.el.obs.utcluj.ro/pi/en_index.htm

## Steps

- [`step1.c`](step1.c) - TCP client application that connects to the remote server using
  TCP over IPv6
- [`step2.c`](step2.c) - IPv6 client that sends `GET / HTTP/1.0` to the remote server and
  saves the reply to `index.html`
- [`step3.c`](step3.c) - IPv4 server on port `22018` that answers `08#` with a confirmation
  and any other command with `Command not implemented`
- [`step4.c`](step4.c) - final version: the IPv4 server fetches the page over IPv6 for every
  `08#`, saves it as `~/index.html` (HTTP headers stripped) and sends it back to the client

## Running

- Run `make step<n>` to compile the corresponding step, for example `make step1` will compile `step1.c` into an executable named `step1`.
- Run `make` to view other available commands.
- **Prequisites:** `gcc` and `make`
