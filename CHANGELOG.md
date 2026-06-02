# Changelog

All notable changes to Vyro OS.

## [v3.7] — ChaCha20-Poly1305 AEAD (RFC 8439)

### Added
- `kernel/chacha20.{h,c}` — ChaCha20 stream cipher per RFC 8439 §2.4. `chacha20_block()` and `chacha20_xor()`.
- `kernel/poly1305.{h,c}` — Poly1305 one-shot MAC per RFC 8439 §2.5. 26-bit-limb arithmetic, fully reduced output, constant-time-friendly final masking.
- `kernel/aead.{h,c}` — `aead_seal` / `aead_open` combine per RFC 8439 §2.8 with constant-time tag compare.
- `aead_selftest()` runs the three official RFC 8439 published test vectors (ChaCha20 §2.4.2, Poly1305 §2.5.2, AEAD §2.8.2).
- Boot banner prints AEAD selftest result; failure is surfaced explicitly.
- `crypto` shell command re-runs selftest on demand.

### Validation
- Selftest verified against RFC 8439 vectors on a host-side build (separate translation units, same source files). All three vectors match.

### Changed
- Kernel size: 147,270 bytes (was 141,862) — 48 KB headroom remaining.

### Limitations (deferred)
- HChaCha20 / XChaCha20-Poly1305 extended-nonce variant not implemented.
- AEAD `seal`/`open` buffer the MAC input in a fixed 16 KB static array — enough for TLS records (max 16 KB) but not arbitrarily large payloads.

## [v3.6] — TCP Congestion Window (slow start + congestion avoidance)

### Added
- Per-TCB `cwnd` and `ssthresh` (bytes).
- `try_emit(id)` helper — emits queued bytes from `snd_buf` throttled by `min(queued, cwnd - in_flight)`.
- Slow start: `cwnd += MSS` on each ACK that advances `snd_una` while `cwnd < ssthresh`.
- Congestion avoidance: `cwnd += max(1, MSS²/cwnd)` per such ACK once `cwnd >= ssthresh`.
- RTO collapse (RFC 5681): `ssthresh = max(cwnd/2, 2*MSS)`, `cwnd = MSS`, plus existing exponential RTO back-off.
- Fast-recovery collapse on 3-dup-ACK: `ssthresh = max(cwnd/2, 2*MSS)`, `cwnd = ssthresh` (no inflation).
- RTO retransmit now re-sends **only in-flight bytes** (`snd_nxt - snd_una`), not the entire `snd_buf`.

### Changed
- `tcp_send` no longer transmits inline; appends to `snd_buf` and calls `try_emit`.
- ACK path calls `try_emit` after sliding the send window, so cwnd-released bytes go out immediately.
- Initial `cwnd = MSS (536)`, initial `ssthresh = 65535`.
- Kernel size: 141,862 bytes (was 141,574) — 54 KB headroom remaining.

### Notes
- Bulk transfers > MSS now ramp up exponentially (slow start) and back off on loss.
- For single-MSS shells like `tcpsend`, behaviour is indistinguishable from v3.5.

## [v3.5] — TCP Reassembly + RTT-driven RTO + Fast Retransmit

### Added
- **Out-of-order reassembly**: single-slot per TCB (1 × MSS = 536 B). When an arriving in-order segment makes the stored OoO segment contiguous, it drains automatically into the receive buffer.
- **RFC 6298 RTT estimator**: per-TCB `srtt_ms`, `rttvar_ms`, dynamic `rto_ms` clamped [200 ms, 5 s]. First sample initializes SRTT = sample, RTTVAR = sample/2.
- **Karn's algorithm**: a retransmitted segment cannot be used as an RTT sample; the probe is cleared on RTO timeout.
- **Exponential RTO back-off**: timeouts double `rto_ms` up to `TCP_RTO_MAX_MS`.
- **Fast retransmit**: 3 duplicate ACKs trigger immediate retransmit of the `snd_una` segment.

### Changed
- `tcp_tick` retransmit now uses the dynamic `t->rto_ms` instead of a fixed 1 s.
- TCB struct gains 7 fields (RTT vars, dup-ACK counter, last-ACK).
- Kernel size: 141,574 bytes (was 140,550) — 54 KB headroom remaining.

### Limitations (deferred to v3.6)
- Single out-of-order slot — multiple gaps still drop.
- No congestion window: cwnd/slow-start/congestion-avoidance not yet wired into send pacing.
- No SACK.

## [v3.4] — TCP Data Transfer

### Added
- `tcp_send(id, data, len)` — append to per-TCB send buffer and transmit as PSH+ACK segments, MSS-sized chunks.
- `tcp_recv(id, buf, max)` — drain received bytes into caller's buffer.
- Per-TCB 1024-byte send and receive linear buffers (32 KB total static).
- Inbound data handling in ESTABLISHED state: in-order data appended to recv buffer, advances `rcv_nxt`, sends cumulative ACK. Out-of-order segments trigger duplicate ACK (no reassembly buffer yet).
- ACK processing slides the send window forward by `(seg_ack - snd_una)` bytes.
- 1 s fixed RTO data retransmit driven by `tcp_tick()`.
- `tcpsend <id> <text...>` and `tcprecv <id> [wait_ms]` shell commands.

### Changed
- TCB allocation paths (active open, passive listener, child from listener) now reset send/recv buffers explicitly.
- Kernel size: 140,550 bytes (was 136,806) — 55 KB headroom remaining.

### Limitations (deferred)
- No out-of-order reassembly (drops + duplicate ACK only).
- No window scaling, SACK, fast retransmit, or RTT estimation — single fixed 1 s RTO.
- No congestion control (cwnd, slow start).
- Buffers are linear `memcpy`-shifted, not ring buffers — O(N) drain.

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
