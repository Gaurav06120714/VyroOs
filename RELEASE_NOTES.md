# Release Notes

## v3.6 — TCP Congestion Window

Vyro OS's TCP stack now paces its sends. Each connection starts with `cwnd = 1 MSS` and ramps up exponentially through slow start until `ssthresh`, then switches to linear growth (congestion avoidance). Any loss signal — RTO or 3 duplicate ACKs — halves `ssthresh` and either collapses `cwnd` to one MSS (RTO) or to the new `ssthresh` (fast recovery).

### What's new
- **`try_emit(id)`** — central emit helper that gates outbound bytes by `cwnd - in_flight`. Called from `tcp_send` after append, and from the ACK path after the send window slides.
- **Slow start**: `cwnd += MSS` per fresh ACK while `cwnd < ssthresh`.
- **Congestion avoidance**: `cwnd += MSS²/cwnd` (minimum 1) per fresh ACK once `cwnd >= ssthresh`.
- **RTO collapse**: `ssthresh = max(cwnd/2, 2*MSS)`, `cwnd = MSS`; retransmit only in-flight bytes.
- **Fast-recovery collapse**: 3 dup-ACK retransmit halves `ssthresh`, sets `cwnd = ssthresh`.

### Compatibility
- Wire format: unchanged; only the timing/pacing of segments changes.
- `tcp_send` semantics unchanged from the caller's perspective: returns bytes accepted into the send buffer (not bytes on the wire).
- Kernel size: 141,862 bytes (was 141,574).

### Known limitations
- No NewReno cwnd inflation during fast recovery.
- No appropriate-byte-counting; fixed 1-MSS increase per ACK regardless of `newly_acked`.
- No PRR, no CUBIC, no BBR.

### Next
**v3.7 — TLS cryptography (ChaCha20-Poly1305).** Lands the symmetric building blocks for TLS 1.3. X.509 parsing and the handshake follow in v3.8/v3.9, then HTTPS in v3.10.

---

## v3.5 — TCP Reassembly + RTT-driven RTO + Fast Retransmit

Vyro OS's TCP stack adopts three of the classic Jacobson/Karn improvements at once: out-of-order segments are no longer dropped, the retransmit timer adapts to the connection's actual round-trip time, and duplicate ACKs trigger an immediate retransmit instead of waiting for the timer to fire.

### What's new
- **Out-of-order reassembly slot** (one per TCB). When an in-order arrival closes the gap, the stored segment drains automatically and the cumulative ACK reflects both.
- **RFC 6298 RTT estimator**: per-TCB `SRTT` / `RTTVAR` updated on every fresh sample; `RTO = SRTT + 4*RTTVAR`, clamped [200 ms, 5 s].
- **Karn's algorithm**: a retransmitted segment never updates RTT; on RTO timeout the in-flight probe is invalidated and RTO doubles up to the ceiling.
- **Fast retransmit**: 3 consecutive duplicate ACKs immediately re-emit the `snd_una` segment.

### Compatibility
- Wire format: unchanged (no SACK option yet).
- Kernel size: 141,574 bytes (was 140,550).
- ~9 KB additional static buffer space (1 OoO slot × MSS × 16 TCBs).

### Known limitations
- Single OoO slot per connection — multiple simultaneous holes still drop segments.
- No `cwnd` yet — the send path emits the whole snd_buf without pacing. Slow start + congestion avoidance land in v3.6.

### Next
**v3.6 — TCP congestion window.** `cwnd` / `ssthresh`, slow start, congestion avoidance, cwnd-throttled emit; pairs with the new RTT machinery from v3.5.

---

## v3.4 — TCP Data Transfer

Vyro OS now sends and receives bytes over established TCP connections. Each TCB carries a 1 KB send buffer (unacked bytes still in flight) and a 1 KB receive buffer (in-order bytes waiting for the application). Outbound writes go on the wire immediately as PSH+ACK segments at one MSS each; inbound data advances `rcv_nxt` and triggers a cumulative ACK.

### What's new
- **`tcp_send(id, data, len)`** — returns bytes accepted into the send buffer.
- **`tcp_recv(id, buf, max)`** — drains the receive buffer.
- **`tcpsend <id> <text>`** and **`tcprecv <id> [wait_ms]`** shell commands.
- **1 s RTO retransmit** of unacked data, driven by the 100 ms `tcp_tick()`.

### Compatibility
- Wire format: standard PSH+ACK data segments, MSS=536, no options.
- Kernel size: 140,550 bytes (was 136,806) — 55 KB headroom remaining.
- 32 KB of static buffer space for TCBs (1 KB send + 1 KB recv × 16).

### Known limitations
- Strict in-order delivery — out-of-order arrivals dropped with duplicate ACK.
- Fixed 1 s RTO; no RTT estimation.
- No congestion window — always sends as much as fits in the send buffer.
- No SACK, no window scaling, no fast retransmit.

### Next
**v3.5 — TCP reassembly + congestion control.** Out-of-order reassembly queue, RTT-estimated RTO, simple congestion window with slow start and fast retransmit.

---

## v3.3 — TCP Listen / Accept

Vyro OS can now act as a TCP server. The state machine gains `LISTEN` and `SYN_RECEIVED`. Inbound SYNs to a listening port allocate a child TCB, the kernel replies with SYN-ACK, and the completing ACK transitions the child to ESTABLISHED — at which point `tcp_accept(port)` returns the conn id.

### What's new
- **Passive open** — `tcp_listen(port)` creates a LISTEN-state TCB.
- **SYN_RECEIVED state** handled with SYN-ACK retransmit on duplicate SYN.
- **`tcp_accept(port)`** returns the next freshly ESTABLISHED unaccepted child.
- **`tcplisten <port>`** and **`tcpaccept <port> [wait_ms]`** shell commands.

### Compatibility
- Wire format: unchanged from v3.2 (still no TCP options).
- Kernel size: 136,806 bytes (was 134,662) — 58 KB headroom remaining.

### Known limitations
- No data send/recv yet — accepted connections are silent until close.
- No backlog cap beyond the global 16-TCB pool.
- No SYN-cookie protection.

### Next
**v3.4 — TCP data transfer.** `tcp_send(id, buf, len)` / `tcp_recv(id, buf, len)` with a simple sliding window and basic retransmit.

---

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
