# Vyro OS — Roadmap

## ✅ v1.0 (shipped) — Phases 0-30

Full 64-bit OS: bootloader, kernel, interrupts, memory, scheduler, ring 3,
ELF loader, VyFS, ATA disk, networking stack, security, package manager,
SMP detection, ACPI power, UEFI boot, basic window manager.

## ✅ v2.0 (shipped) — Phases 31-50

Modern desktop OS layer:

- Double-buffered compositor + theme system (dark/light)
- macOS-style dock, Windows-style top bar, live clock
- Bitmap icons, drop shadows, window animations
- Full window controls: min/max/resize/snap
- Widget toolkit + app framework v2
- **12 native apps**: Files, Settings, Terminal, TextEdit, Calculator, Clock,
  Task Manager, Launchpad, Notification Center, Control Center, Browser,
  App Store
- Sockets API (TCP state machine), DHCP & DNS client architecture
- IPC: pipes, message queues, signals
- Notification toasts + notification center history

---

## ✅ v3.0 (shipped) — Live RTL8139 networking

Real DMA TX/RX on the NIC, ICMP echo with RTT, ARP request/reply with 8-entry cache, DHCP/DNS over actual wire packets.

## ✅ v3.6 (shipped) — TCP congestion window

- Per-TCB `cwnd` and `ssthresh`
- Slow start + congestion avoidance
- RTO collapse (cwnd → MSS, ssthresh halved)
- Fast-recovery collapse on 3 dup-ACKs
- Throttled emit via `try_emit(id)`; RTO retransmit limited to in-flight bytes

## ✅ v3.5 (shipped) — TCP reassembly + RTT-driven RTO + fast retransmit

- Single-slot out-of-order reassembly per TCB
- RFC 6298 SRTT/RTTVAR/RTO estimator (clamp 200 ms..5 s)
- Karn's algorithm; exponential back-off on timeout
- 3-dupACK fast retransmit

## ✅ v3.4 (shipped) — TCP data transfer

- `tcp_send` / `tcp_recv` with 1 KB linear send/recv buffers per TCB
- PSH+ACK segments at MSS-sized chunks
- Send window slides on inbound ACK
- 1 s fixed RTO retransmit
- `tcpsend` / `tcprecv` shell commands

## ✅ v3.3 (shipped) — TCP listen / accept

- `TCP_LISTEN` and `TCP_SYN_RECEIVED` states
- `tcp_listen(port)` / `tcp_accept(port)`
- Inbound SYN spawns child TCB, kernel replies SYN-ACK, ACK completes handshake
- `tcplisten` / `tcpaccept` shell commands

## ✅ v3.2 (shipped) — TCP connection establishment

- `tcp.c` / `tcp.h` — RFC 793 active-open client
- 16-TCB table, full client state machine (CLOSED → SYN_SENT → ESTABLISHED → FIN_WAIT/CLOSE_WAIT → TIME_WAIT → CLOSED)
- TCP/IPv4 checksum with pseudo-header
- SYN retransmit (1s / 2s / 4s)
- RST on unknown 4-tuple, RST-driven teardown inbound
- `tcp` and `tcpconnect` shell commands

## ✅ v3.1 (shipped) — UDP transport layer

- `udp.c` / `udp.h` — port-dispatch UDP/IPv4 (RFC 768) with checksum
- `net_pump.c` — main-loop RX dispatcher (ARP → UDP → ICMP/TCP hooks)
- DHCP and DNS refactored on top of `udp_listen` + `net_pump_run`
- `udp` shell command for listener visibility
- Compile-time `VYRO_UDP_LEGACY` fallback retained one release

## 🔭 v3.2+ — TCP and beyond

| Area | v2.0 status | v3.0 goal |
|------|-------------|-----------|
| **Networking transport** | API + state machines | Real RTL8139 DMA TX/RX, live ping |
| **DHCP** | packet structures | Live DHCPDISCOVER over real NIC |
| **DNS** | static hosts table | Real UDP/53 queries |
| **TLS** | architecture-only | Real X.509 + ChaCha20-Poly1305 |
| **TCP** | state machine | Full segment reassembly, congestion control |
| **USB** | controller detected | xHCI ring buffers, real device enumeration |
| **Scheduler** | cooperative | Preemptive (timer-driven), per-process VMs |
| **SMP** | cores detected | AP bring-up (INIT-SIPI), per-core queues |
| **Disk FS** | raw sectors | FAT32 read+write, persistent VyFS across boots |

## 🌐 v4.0 — portability + self-hosting

- ARM64 (AArch64) and RISC-V ports
- Dynamic linking + shared libraries
- A real userland: coreutils compiled against libvyro
- Self-hosting: build Vyro OS *on* Vyro OS
- Real font rasterizer (TrueType subset)
- GPU acceleration via virtio-gpu
