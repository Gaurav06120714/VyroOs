# Changelog

All notable changes to Vyro OS.

## [v4.0] — Release: 50 phases since v3.0

Vyro OS 4.0 is the culmination of fifty incremental phases shipped on top of the 2.0 desktop foundation. Highlights:

- **Networking stack**: full TCP (handshake, listen/accept, data transfer, reassembly, fast retransmit, congestion control), UDP with port dispatch, DHCP, DNS, ARP, ICMP/ICMPv6 echo, IPv6 link-local with EUI-64.
- **Crypto suite**: ChaCha20-Poly1305 AEAD (RFC 8439), SHA-256, HMAC-SHA-256, HKDF + TLS 1.3 Expand-Label/Derive-Secret (RFC 5869/8446), X25519 ECDH (RFC 7748), RSA-2048 PKCS1-v1_5 verify, **RSA-4096 verify** (parallel bignum_4k), **ECDSA P-256 verify** (RFC 6979). All host-validated against published RFC test vectors.
- **TLS 1.3**: full client handshake (ClientHello, ServerHello, key schedule, decrypt encrypted handshake, server Finished MAC verify, **client Finished** + application traffic keys + HTTPS GET) + server-side ClientHello parser, **ServerHello/Certificate/Finished builders**.
- **X.509**: DER parser, Subject/Issuer CN, SAN, validity, signature algorithm OIDs, RSA pubkey extraction, EC pubkey extraction, **chain validation** with trust anchors (2 built-in: ECDSA test cert + Vyro Root CA RSA).
- **Filesystem**: FAT32 read **and write**, subdirectory navigation, integrated into the Files app.
- **SMP**: Local APIC initialization, **AP bring-up trampoline (real → 32-bit → 64-bit long mode)**, per-CPU stacks, `ap_main()` C entry, per-AP presence map.
- **USB**: xHCI controller detection, halt+reset, DCBAA + Command Ring + Event Ring scaffolding.
- **Storage**: Bootloader sector budget bumped from 192 KB to 384 KB ceiling.
- **Desktop**: Compositor glassmorphism primitives (blur, tint, rounded panels), 6 procedural wallpapers, clock/calendar/weather widgets, glass window chrome, glassmorphism notification toasts.
- **Userspace**: libvyro extended with printf-lite utilities; 2 user ELFs (`init`, `hello`).
- **Crypto-secured random**: ChaCha20 keystream PRNG seeded by RDRAND + RDTSC + timer.
- **Audio**: 5 PC-speaker tunes including a boot chime that plays at startup.
- **Diagnostics**: `bench` micro-benchmarks crypto + scheduler; `tls`/`tlskdf`/`crypto`/`x509`/`x509verify`/`tlsparseclient`/`tlsserverhello`/`tlsservercert`/`smp`/`xhci*`/`fatls`/`fatwrite`/`tlshandshake`/`httpget`/`httpsget` shell commands.

Kernel binary size: ~228 KB / 384 KB ceiling.

## [v3.50] — `bench` shell command (crypto micro-benchmarks)
## [v3.49] — PC-speaker tunes + boot chime
## [v3.48] — TLS server Certificate + Finished message builders
## [v3.47] — Second trust anchor (Vyro Root CA RSA-2048)
## [v3.46] — Glassmorphism notification toasts
## [v3.45] — Wallpaper + widgets default on GUI startup
## [v3.44] — Files app FAT32/VyFS toggle
## [v3.43] — xHCI event ring + Interrupter 0
## [v3.42] — IPv6 + ICMPv6 echo reply (EUI-64 link-local)
## [v3.41] — TLS server-side ServerHello builder
## [v3.40] — Multi-app userspace (hello + init)
## [v3.39] — 4096-bit bignum + RSA-4096 verify
## [v3.38] — Runtime smoke launch (QEMU validation)
## [v3.37] — Aggressive cooperative-preempt hooks
## [v3.36] — Wallpaper engine + desktop widgets
## [v3.35] — xHCI DCBAA + Command Ring + start
## [v3.34] — TLS server-side ClientHello parser
## [v3.33] — Glassmorphism window chrome toggle
## [v3.32] — libvyro userspace utilities
## [v3.31] — docs catch-up

## [v3.30] — xHCI controller halt + reset
Operational-register accessors (USBCMD, USBSTS, CRCR, DCBAAP, CONFIG); `xhci_reset` runs the spec Halt → HCRST → wait-for-CNR sequence; `xhci_status`; `xhcireset` shell command.

## [v3.29] — SMP APs reach C
Trampoline reads LAPIC ID via MMIO at `0xFEE00020`, computes per-CPU stack at `0x100000 + apic_id × 64KB`, calls `ap_main(apic_id)`. Presence bitmap + `aps_in_c_count`; `smp` shell command surfaces live APIC IDs.

## [v3.28] — Trust anchors + multi-cert chain validation
`trust.{h,c}` stores up to 4 anchor certs (DER + parsed). TLS Certificate handler walks full CertificateList per RFC 8446 §4.4.2, verifies leaf → intermediate → trust anchor chain. Embedded ECDSA test cert auto-registered at boot. `trust` shell command.

## [v3.27] — ECDSA P-256 verification
Full Jacobian-coordinate point arithmetic on NIST P-256, modular inverse via Fermat's little theorem, DER signature parser, complete ECDSA verify algorithm. Host-validated against RFC 6979 §A.2.5 (1.2 s). `x509_verify_signature` dispatches to RSA or ECDSA.

## [v3.26] — TLS Certificate handshake parse
`process_certificate_msg` walks RFC 8446 §4.4.2, extracts leaf cert, runs `x509_verify_signature(leaf, leaf, leaf)` for self-signed case. Hostname matched against SAN + Subject CN with case-insensitive + leftmost-`*.` wildcard rules.

## [v3.25] — SMP long-mode AP entry
174-byte trampoline (`smp_trampoline.asm`) goes 16-bit real → 32-bit PM → 64-bit LM. BSP publishes PML4 phys at `0x9008`; AP enables PE, PAE, LME, PG. `lock inc` on flag byte signals alive.

## [v3.24] — TLS app traffic keys + client Finished
`finalize_handshake` computes client Finished MAC, sends as encrypted handshake record; derives `master_secret` and application traffic secrets per RFC 8446 §7.1. `tls_send`/`tls_recv` for app data. `httpsget` now does a real end-to-end HTTPS GET.

## [v3.23] — X.509 RSA signature verification + bignum fix
`x509_parse` extracts RSA (n, e) from SubjectPublicKeyInfo; remembers tbs and signature byte offsets. `x509_verify_signature` runs `rsa_pkcs1_v15_sha256_verify`. **Critical fix:** `bn2_mod` runaway shift-subtract replaced with bounded binary long division + 2049th-bit overflow tracking.

## [v3.22] — xHCI USB 3.0 scaffolding
PCI scan for class 0x0C / subclass 0x03 / prog-if 0x30; MMIO BAR0; capability registers parsed (CAPLENGTH, HCIVERSION, HCSPARAMS1, PAGESIZE). `xhci` shell command.

## [v3.21] — HTTP/1.1 GET client
`http_get` does TCP connect + GET request + response drain until close. `httpget` shell command; `httpsget` placeholder.

## [v3.20] — RSA-PKCS1-v1_5 signature verification
2048-bit bignum library (64 × 32-bit limbs), schoolbook multiply, square-and-multiply modexp. `rsa_pkcs1_v15_sha256_verify` decodes the recovered signature against the SHA-256 DigestInfo prefix.

## [v3.19] — SMP AP bring-up (real-mode)
12-byte trampoline at `0x8000`, INIT-SIPI-SIPI via LAPIC ICR, APs `lock inc` a flag at `0x9000`.

## [v3.18] — FAT32 writes + subdirectory navigation
`fat32_write_file` allocates cluster chain, updates FAT (all copies), creates/overwrites dir entry. `fat32_path_lookup` walks `/`-separated paths. `fatwrite` + `fatpath` shell commands.

## [v3.17] — Cryptographic CSPRNG
ChaCha20 keystream PRNG; SHA-256-derived key; entropy pool seeded by RDRAND, RDTSC, timer ticks. `csprng_reseed` for additional entropy. TLS now uses `csprng_bytes`.

## [v3.16] — Bootloader sector budget bump
24 chunks → 48 chunks: 192 KB → 384 KB kernel ceiling.

## [v3.15] — Glassmorphism compositor primitives
`comp_blur_rect` (multi-pass 3-tap box blur), `comp_tint_rect` (alpha-blend), `comp_rounded_rect` (corner-radius mask), `comp_glass_panel` (blur + tint + rounded edge). `glass` shell command demos.

## [v3.14] — Local APIC initialization
IA32_APIC_BASE MSR read, MMIO enable via SVR, `lapic_id` / `lapic_version` accessors, ICR-driven `send_ipi` primitive. `lapic` shell command.

## [v3.13] — FAT32 read-only driver
BPB parse, FAT chain walk, root directory enumerate, 8.3 filename match, file read via `ata_read_sector`. `fatls` + `fatcat` shell commands.

## [v3.12] — Preemptive scheduler tick (cooperative-tickle)
`sched_tick` called from PIT IRQ tracks quantum; `sched_check_preempt` yields current task at safe kernel boundaries; 20 ms quantum.

## [v3.11] — TLS 1.3 handshake over TCP
`tls_connect` state machine sends ClientHello via `tcp_send`, receives ServerHello, decrypts encrypted handshake records, verifies server Finished MAC against `HMAC(finished_key, transcript_hash)`. `tlshandshake` shell command.

## [v3.10] — TLS 1.3 Primitives (record framing + ClientHello + ServerHello + key schedule)

### Added
- `kernel/tls.{h,c}` — TLS 1.3 building blocks.
- **ClientHello builder** (`tls_build_client_hello`) emits a complete handshake record with extensions: `supported_versions`, `supported_groups`, `signature_algorithms`, `key_share` (X25519), and `server_name` (SNI). Cipher suite is `TLS_CHACHA20_POLY1305_SHA256`.
- **ServerHello parser** (`tls_parse_server_hello`) walks the extensions and extracts the server's X25519 `key_share` pubkey.
- **Key schedule** (`tls_derive_handshake_keys`) implements RFC 8446 §7.1: `Early-Secret → derived → Handshake-Secret → {c,s} hs traffic secret → {c,s} {key,iv}`.
- **Selftest** (`tls_selftest`) validates X25519 self-derivation, ECDH agreement, full key-schedule chain against **RFC 8448 §3** published bytes, and ClientHello round-trip.
- `tls [hostname]` shell command displays selftest result and builds a sample ClientHello for the given hostname.

### Validation
Host-side build of the exact kernel sources produced `tls_selftest=1`:
- `x25519_base(client_priv)` matches RFC 8448 expected `client_pub`
- `x25519(client_priv, server_pub)` matches expected `shared`
- `handshake_secret` after the full HKDF chain matches RFC 8448's published value

### Changed
- Kernel size: 164,710 bytes (was 161,286) — 31 KB headroom remaining.

### Limitations (deferred to v3.11)
- **No network round-trip yet.** TLS primitives are linked but the TLS state machine is not wired to `tcp_send`/`tcp_recv`. That brings ServerHello receipt + first encrypted record decryption.
- No signature verification of the server's `CertificateVerify` — v3.12 (currently planned as v3.12 after the network wire-up).
- No `Finished` message MAC computation/check.

## [v3.9] — X25519 + HMAC/HKDF + TLS Key Schedule

### Added
- `kernel/hkdf.{h,c}` — HMAC-SHA-256 (RFC 2104) and HKDF-Extract/Expand (RFC 5869) on top of the existing `sha256()`.
- TLS 1.3 helpers: `tls13_hkdf_expand_label()` builds the `HkdfLabel` struct per RFC 8446 §7.1; `tls13_derive_secret()` wires SHA-256(messages) into Expand-Label.
- `kernel/x25519.{h,c}` — full RFC 7748 X25519 scalar multiplication. 5 × 51-bit limb field arithmetic, complete Montgomery ladder with constant-time conditional swap, inversion via 2^255-21 addition chain, scalar/u-coord encode/decode.
- `x25519_base()` convenience for public-key derivation (scalar × basepoint 9).
- Two boot selftests: `hkdf_selftest()` runs RFC 4231 HMAC test 1 and RFC 5869 HKDF test case 1; `x25519_selftest()` runs both RFC 7748 §5.2 known-answer tests.
- `tlskdf` shell command exercises HKDF, X25519 ECDH agreement (a*B == b*A), and one round of `HKDF-Expand-Label`.

### Validation
- Host-side build of the exact same `sha256.c` / `hkdf.c` / `x25519.c` sources produced `hkdf_selftest=1` and `x25519_selftest=1`. Both RFC 5869 PRK and OKM bytes match exactly; both X25519 KATs (255-bit scalar × 255-bit u-coordinate) match exactly.

### Changed
- Kernel size: 161,286 bytes (was 153,126) — 35 KB headroom remaining.

### Limitations (deferred to v3.10)
- No TLS record layer yet — record framing, content type, fragmentation, and AEAD encryption with derived traffic keys are the next phase.
- No RSA or ECDSA signature verification — server certificate verify will land in v3.11.
- No SHA-384 — only SHA-256 cipher suites supported.

## [v3.8] — X.509 Certificate Parser

### Added
- `kernel/x509.{h,c}` — minimal DER reader + X.509 v3 parser.
- Extracts: Subject CN, Issuer CN, NotBefore, NotAfter (raw ASCII), DNS SANs (up to 8), signature algorithm enum, public-key algorithm enum.
- Recognizes algorithm OIDs: `sha{256,384,512}WithRSAEncryption`, `ecdsa-with-SHA{256,384}`, `rsaEncryption`, `ecPublicKey`.
- `kernel/x509_testvec.c` — embedded 406-byte ECDSA-P256 self-signed test certificate.
- `x509_selftest()` parses the embedded cert at boot and verifies all extracted fields.
- `x509` shell command — re-parses the embedded cert and displays the result.

### Validation
- Host-side build of the same `x509.c` source ran against an `openssl req -x509`-generated RSA cert: extracted CN, issuer CN, two SANs, sig_alg=SHA256_RSA, pkey_alg=RSA all match.
- Same parser against an openssl-generated ECDSA P-256 cert: extracted CN, SAN, sig_alg=ECDSA_SHA256, pkey_alg=EC all match.
- Both cert formats parse without modification.

### Changed
- Kernel size: 153,126 bytes (was 147,270) — 42 KB headroom remaining.

### Limitations (deferred to v3.9)
- No signature verification (`signatureValue` BIT STRING is skipped).
- No chain building or trust-anchor handling.
- No certificate revocation (CRL, OCSP) checks.
- Public key bytes are not extracted from `SubjectPublicKeyInfo` — only the algorithm OID is recognized.

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
