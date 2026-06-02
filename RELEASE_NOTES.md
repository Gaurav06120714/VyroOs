# Release Notes

## v3.2 — TCP Connection Establishment

Vyro OS now speaks TCP. The active-open client path is complete: `tcp_connect()` performs the RFC 793 three-way handshake, transitions through the full client state machine, and tears down cleanly with a FIN exchange.

### What's new
- **`tcp.c`** — full active-open client (16 TCBs), all eight RFC 793 states (CLOSED through TIME_WAIT), TCP/IPv4 checksum with pseudo-header, SYN retransmit at 1s/2s/4s, RST on unknown-tuple inbound.
- **`tcp` shell command** — lists every TCB with state + 4-tuple.
- **`tcpconnect <ip> <port>`** — drives a full handshake-and-close. Useful for verifying connectivity to any LAN host.

### What changed
- `net_pump_run` now dispatches to TCP after ARP and UDP, and calls `tcp_tick()` every 100 ms for retransmits and TIME_WAIT expiry.

### Compatibility
- Wire format: standard RFC 793 (no options yet — no MSS advertised, no window scaling, no SACK).
- Kernel size: 134,662 bytes (was 129,638) — 62 KB headroom remaining.

### Known limitations
- No data transfer yet — connect + close only. Data send/recv lands when paired with listen/accept.
- No passive open — server side lands in v3.3.
- ISN derived from `timer_uptime_ms()` (RFC 6528 random ISN deferred).
- TIME_WAIT shortened to 2 s (vs RFC 2*MSL) so TCBs free quickly during testing.

### Next
**v3.3 — TCP listen/accept.** Server-side state machine, SYN backlog queue, multiple simultaneous connections per listening port.

---

## v3.1 — UDP Transport Layer

Vyro OS now has a real, port-dispatched UDP/IPv4 transport. DHCP and DNS were rewritten on top of it; their wire output is byte-identical to v3.0, but the in-kernel structure is now layered properly: every UDP-bearing protocol registers a port and receives a callback when a matching datagram arrives.

### What's new
- **`udp.c`** — full UDP/IPv4 transport with port dispatch (up to 16 simultaneous listeners), RFC 768 checksum, and ARP-resolved unicast / broadcast sends.
- **`net_pump.c`** — main-loop RX pump (`net_pump_run(ms)`) that drains the RX queue and dispatches to ARP, then UDP. Future ICMP and TCP layers will plug in here.
- **`udp` shell command** — lists active port listeners (`port 68` while DHCP is in flight, ephemeral port while a DNS query is outstanding).

### What changed
- DHCP and DNS no longer poll the raw RX queue. They `udp_listen()` for the duration of their exchange and rely on the pump.
- A compile-time `VYRO_UDP_LEGACY` flag in `dhcp_real.c` and `dns_real.c` falls back to the v3.0 direct-poll path for one release as a safety net.

### Compatibility
- Wire format: unchanged (DHCP, DNS, ICMP, ARP all byte-identical).
- Kernel size: 129,638 bytes (was ~127 KB) — 67 KB headroom remaining.
- No on-disk format changes. No boot sector changes. Rollback is a single `git revert`.

### Known limitations
- UDP checksum is computed on TX and not validated on RX (RFC 768 permits checksum=0; we accept any value).
- No source-IP filtering — spoofed-source datagrams reach the registered callback. Firewall belongs in a later phase.
- Port table is fixed at 16. `udp_listen` returns 0 when full.

### Next
**v3.2 — Networking Phase 13: TCP Connection Establishment.** Three-way handshake (SYN / SYN-ACK / ACK), state machine, per-connection control block. Builds on `net_pump` exactly like UDP.
