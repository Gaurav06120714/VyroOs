# Release Notes

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
