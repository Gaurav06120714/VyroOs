# Changelog

All notable changes to Vyro OS.

## [v3.3] — TCP Listen / Accept (Passive Open)

### Added
- States `TCP_LISTEN` and `TCP_SYN_RECEIVED` in the TCP state machine.
- `tcp_listen(port)` — create a passive listener TCB.
- `tcp_accept(port)` — pull a freshly ESTABLISHED inbound child, or -1 if none.
- Inbound SYN to a listening local port allocates a child TCB and replies with SYN-ACK; subsequent ACK completes the handshake.
- Retransmitted SYN to a SYN_RECEIVED TCB resends SYN-ACK.
- `tcplisten <port>` and `tcpaccept <port> [wait_ms]` shell commands.

### Changed
- TCB struct: new `is_listener` and `accepted` fields.
- Kernel size: 136,806 bytes (was 134,662) — 58 KB headroom remaining.

### Limitations (deferred)
- No data send/recv yet — accept hands back an established connection that can only be closed.
- Single accept-queue per port via linear scan; no explicit backlog limit beyond the global 16-TCB pool.
- No SYN-cookie defense against SYN floods.

## [v3.2] — Networking Phase 13: TCP Connection Establishment

### Added
- `kernel/tcp.{h,c}` — TCP (RFC 793) active-open client with up to 16 simultaneous TCBs.
  - States: CLOSED, SYN_SENT, ESTABLISHED, FIN_WAIT_1/2, CLOSE_WAIT, LAST_ACK, TIME_WAIT
  - `tcp_connect(ip, port)` returns conn id; `tcp_close(id)` triggers graceful FIN
  - Real TCP/IPv4 checksum with pseudo-header
  - SYN retransmit at 1s/2s/4s, give up after 3 retries
  - RST handling: sent on unknown 4-tuple, processed inbound to teardown TCB
  - TIME_WAIT expires after 2s (placeholder for 2*MSL)
- `kernel/net_pump.c` — hooked `tcp_input()` after `udp_input()`, runs `tcp_tick()` every 100 ms
- `tcp` shell command — lists TCBs with state + 4-tuple
- `tcpconnect <ip> <port>` shell command — drives a full handshake + close

### Changed
- `kernel.c` boot order now ends with TCP layer ack
- Kernel size: 134,662 bytes (was 129,638) — 62 KB headroom remaining

### Limitations (deferred to later phases)
- No data send/recv (handshake + close only)
- No passive open (listen/accept) — that is v3.3
- No window scaling, SACK, or congestion control
- ISN derived from millisecond clock, not RFC 6528 hashed — Networking Phase 8: UDP Layer

### Added
- `kernel/udp.{h,c}` — UDP transport (RFC 768) with a 16-entry port-dispatch table.
  - `udp_listen(port, cb)` / `udp_unlisten(port)`
  - `udp_send_to(ip, sport, dport, buf, len)` (ARP-resolves destination MAC)
  - `udp_send_bcast(sport, dport, buf, len)` (255.255.255.255 / 0.0.0.0 source)
  - `udp_input(frame, len)` parses and dispatches inbound datagrams
  - Real UDP checksum with IPv4 pseudo-header
- `kernel/net_pump.{h,c}` — `net_pump_run(ms)` drains the RX queue and pumps ARP, UDP, and (later) ICMP/TCP.
- `udp` shell command — lists active port listeners.

### Changed
- `kernel/dhcp_real.c` — refactored to register `udp_listen(68, ...)` and drive replies via `net_pump_run`. Compile-time `VYRO_UDP_LEGACY` switch retained as a safety fallback for one phase.
- `kernel/dns_real.c` — refactored identically on an ephemeral port.
- `kernel/kernel.c` — boots `udp_init()` after `net_io_init()`.

### Unchanged (intentional regressions guarded against)
- `cmd_realping` continues to use raw L2/L3 send for ICMP — UDP refactor is scoped to UDP-bearing protocols only.
- Wire format for DHCP and DNS is byte-identical to v3.0.

## [v3.0] — Live networking

- Phase 57: real RTL8139 NIC driver (TX/RX, IRQ, bus master) + `realping`
- Phase 58: real DHCP + DNS over RTL8139 (actual wire packets, responses parsed)
- Phase 59: live ICMP ping with RTT — echo replies parsed from RX queue, DHCP applies leased IP
- Phase 60: ARP request/reply + 8-entry cache — `realping` resolves destination MAC, replies to `who-has`

## [v2.0] — Desktop OS

Phases 31–50. Double-buffered compositor, theme system, widget toolkit, app framework v2, 12 native apps, sockets API + DHCP/DNS architecture, IPC.

## [v1.0] — Kernel core

Phases 0–30. Bootloader, kernel, IDT/PIC, memory, scheduler, ring 3, ELF64, VyFS, ATA, networking foundations, security, package manager, SMP detection, ACPI, UEFI, basic window manager.
