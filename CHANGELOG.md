# Changelog

All notable changes to Vyro OS.

## [v3.1] — Networking Phase 8: UDP Layer

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
