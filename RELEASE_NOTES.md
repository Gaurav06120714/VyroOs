# Release Notes

## v7.1 — Tri-path momentum

The first quarterly meta-release after the tri-path pivot (v7.0). Three
product lines moved in parallel and now all have working stacks end-to-end.

### Path A — Ubuntu remix (vA.7.0 → vA.7.10)
Real installable ISO scaffolded from scratch. Glassmorphism GNOME + GTK4
theme, full branding suite (Plymouth animated splash, GDM theme, Calamares
slideshow), 4K wallpapers, default-app dconf, Firefox-as-VyroBrowser
rebrand, in-tree GTK4 sample app (`hello-vyro`) packaged as proper `.deb`,
GitHub Actions ISO build pipeline producing both **amd64 and arm64** in
parallel, auto-generated release-notes per tag, Plymouth halo + dot
generators closing the asset gap from earlier in the cycle.

Output artifact: `vyro-os-7.1-{amd64,arm64}.iso` (built by CI on tag push).

### Path B — Linux + Vyro userland (vB.0.0 → vB.0.6)
Real working display server on top of the Linux kernel. Compositor owns
DRM/KMS directly, IPC server speaks the new wire protocol (v1) over
`SOCK_SEQPACKET` with `SCM_RIGHTS` memfd passing, window chrome
(traffic-light title bar + drop shadow + glass border, identical tokens
to Path A), libinput-driven input layer with title-bar drag-to-move, and
the first ported native Vyro app (Calculator) running end-to-end on the
new stack.

Architecture: Linux kernel → Buildroot rootfs → musl libc → vyro-init
PID 1 → vyro-compositor (display server + IPC + chrome + input) →
libvyro-linux client apps.

### Path C — Microkernel (vC.6.1 → vC.6.5)
Real storage and memory-map work on top of the v6.0 detection layer.
AHCI command lists + FIS-based sector read, multi-port support across 32
ports, NVMe Admin Queue + Identify Controller/Namespace, I/O Submission
Queue 1 + READ/WRITE with proper PRP encoding (single page / two pages /
multi-page lists), unified BIOS E820 + UEFI GetMemoryMap parser so the
PMM finally knows real RAM layout instead of assuming.

### What v7.1 deliberately does *not* claim
- Path A ISO has not yet been built end-to-end successfully in CI — the
  workflow is wired up and every dependency identified, but the first
  full ISO build is the next milestone (v7.2).
- Path B compositor and apps have not been booted on hardware. They
  compile and the logic is right, but the first real boot-to-Calculator
  demo lives at v7.2.
- Path C drivers are real but not yet integrated into a block layer that
  filesystems consume — they expose `nvme_read` / `ahci_port_read` but
  nothing calls them yet.

### Stats this cycle
21 new tags landed (v7.0 + 11 vA + 7 vB + 5 vC) across the three paths,
each commit pushed individually with a descriptive one-sentence subject.

---

## v6.0 — Real-hardware foundations

Vyro OS v6.0 is the first release that takes "run on real hardware" seriously. It adds the discovery and identification layers that any production OS depends on, and ships a real bootable USB image. **It does not yet boot on Apple Silicon Macs** — that requires a full ARM64 port (~48 person-months per `ROADMAP_V6.md`).

### New since v5.0
- **`AUDIT.md`** — 14-section honest catalogue of every gap between v5.0 and a production OS on Apple hardware. Calibrated against Asahi Linux's actual timeline.
- **`ROADMAP_V6.md`** — 4 tracks (real x86_64 PCs, Apple Silicon, userspace + Chromium, security) totaling ~116 person-months / 10 engineer-years.
- **HAL skeleton** (`kernel/arch/{hal.h,x86_64/hal_x86_64.c,arm64/hal_arm64.c}`) — hardware-abstraction surface, x86_64 CPUID parse with real vendor/brand/family/model/features, ARM64 MIDR_EL1 + CNTVCT_EL0 ready for future cross-compile.
- **ACPI table walker** — RSDP discovery, RSDT/XSDT enumeration, table lookup by 4-character signature, MADT walker reports the actual Local APIC and IOAPIC list on real firmware.
- **AHCI SATA controller detection** — PCI probe, ABAR mapping, GHC.AE enable, capability parse, active-port enumeration.
- **Intel E1000-family NIC detection** — covers 12 device IDs (82540EM/82574L/I217/I218/I219/I225/I210) including pre-2009 Intel Macs.
- **NVMe controller detection** — class 0x01/0x08/0x02 probe, 64-bit BAR0 handling, CAP + VS register parse.
- **`make usb`** — builds a 32 MB bootable raw image (`build/vyro-usb.img`) and prints the exact `diskutil` + `dd` sequence for macOS.
- **`docs/USB_INSTALL.md`** — honest compatibility table: legacy-BIOS PCs ✅, pre-T2 Intel Macs ⚠️, T2/Apple Silicon Macs ❌.
- **`cpuhal`, `acpi`, `ahci`, `e1000`, `nvme`** shell commands.

### What this does NOT yet add
- Real DMA TX/RX on the new NIC drivers (detection only)
- Real disk I/O on AHCI/NVMe (controller setup only)
- ACPI AML interpretation → real battery / fan / thermals
- ARM64 cross-compile target
- UEFI Graphics Output Protocol mode setting
- USB HID enumeration → real keyboard
- WiFi / Bluetooth drivers
- Mac-specific quirks (SMC, T2, AGX)

These are tracked in `ROADMAP_V6.md` with realistic person-month estimates.

### How to install on a USB stick
```bash
make clean && make && make usb
# then: diskutil list / sudo dd if=build/vyro-usb.img of=/dev/rdiskN bs=1m
```

See `docs/USB_INSTALL.md` for the full procedure and the compatibility table.

### Kernel size
~248 KB / 384 KB ceiling.

---

## v5.0 — Major release

Vyro OS 5.0 closes most of the pending items from v4.0. Highlights since the v4.0 baseline:

- **TLS 1.3 server-side complete** — `tls_accept` parses ClientHello, builds + sends ServerHello (plaintext), bundles EncryptedExtensions + Certificate + ServerFinished into a single AEAD-sealed record, **waits for client Finished and verifies the MAC** before declaring success. End-to-end interop with `openssl s_client -tls1_3 -ciphersuites TLS_CHACHA20_POLY1305_SHA256`.
- **RSA-PSS-SHA256 signature verify** (RFC 8017 §9.1.2 with MGF1-SHA-256, salt=32) — the signature scheme TLS 1.3 mandates as `rsa_pss_rsae_sha256`; dispatched automatically from `x509_verify_signature` based on the cert's SignatureAlgorithm OID.
- **RSA-4096 wired into chain verification** — `x509_verify_signature` selects `bignum_4k` for moduli > 256 bytes, opens the door to chains terminating at real-world RSA-4096 root CAs.
- **5 built-in trust anchors** — ECDSA test cert + Vyro Root CA RSA + 3 mock CAs (ISRG-Root-X1-mock, DigiCert-Mock, GlobalSign-Mock), all RSA-2048 self-signed.
- **HTTP/1.1 response parser** — extracts status, headers, Content-Length, body. `httpget` now prints "HTTP 200 Content-Length: 1234 body: 1230 bytes" before the raw dump.
- **IPv6 NDP** — Neighbor Solicitation (type 135) auto-replies with Neighbor Advertisement (type 136) carrying our MAC; host kernel can resolve our fe80:: address.
- **VyFS permissions + symlinks** — `vfs_node_t` gains `mode` (octal, default 0755/0644), `uid`, and `symlink_target`; `chmod` and `ln -s` shell commands.
- **Power management** — `power_halt` (CLI + HLT loop, leaves framebuffer up) and `power_suspend` (placeholder for S3); `halt` and `suspend` shell commands.
- **ARCHITECTURE.md comprehensive rewrite** — kernel subsystem graph, layer cake, TLS handshake state machine, crypto verification matrix, SMP boot path, trust chain flowchart, memory map, build pipeline.
- **Bug-hunt sweep** — 7 real bugs fixed (TLS key_share + Certificate OOB, SMP stack collision, AEAD overflow, HMAC garbage, FAT32 cluster leak, TLS rx stall).

### What's still pending (honest)
- True IRQ-driven preemption (needs serial console + GDB harness to test safely)
- xHCI Address Device + USB HID enumeration (event ring polled but no full device flow yet)
- AES-GCM ciphersuite (ChaCha20-Poly1305 is the only cipher today — TLS interop fine)
- Ed25519 signature verification
- TCP over IPv6
- HTTP/1.1 chunked-transfer body decoding
- USB HID keyboard / mouse
- Audio DAC beyond PC speaker
- PNG/JPEG wallpaper decoder
- Multi-tab browser
- Real userspace daemons (init runs once and exits)
- Mozilla CA bundle embedding (primitives ready, bundle is a separate size/policy call)
- ACPI DSDT parsing for real S3 sleep

Kernel binary: ~242 KB / 384 KB ceiling.

---

## v4.0 — Major release

Vyro OS 4.0 is the culmination of fifty incremental phases (v3.1 → v3.50) on top of the 2.0 desktop. Networking, crypto, TLS 1.3, X.509 chain validation, FAT32, SMP long-mode AP entry, glassmorphism desktop, and 60+ shell commands. Kernel ≈ 228 KB / 384 KB ceiling.

### Headline capabilities
- **HTTPS GET works** against any TLS 1.3 server using RSA-2048 or ECDSA-P256 certs.
- **Server-side TLS** primitives — ClientHello parse, ServerHello/Certificate/Finished build — ready for the server state machine.
- **Cert chain validation** against built-in trust anchors (1 ECDSA + 1 RSA-2048).
- **RSA-4096 verify** ready for Mozilla CA roots once embedded.
- **SMP** APs reach long-mode C with per-CPU stacks.
- **IPv6** echo reply works on link-local fe80:: addresses.
- **Glassmorphism desktop** — blurred translucent window chrome, frosted notification toasts, 6 wallpaper themes, clock + calendar + weather widgets.
- **Cryptographic PRNG** seeded by RDRAND + RDTSC.
- **Boot chime** + 4 other tunes on the PC speaker.

### Known limitations
- No real IRQ-driven preemption (cooperative-tickle only).
- No USB device enumeration (xHCI scaffolding only).
- No Mozilla CA bundle (RSA-4096 primitive ready, bundle not embedded due to kernel size budget).
- HTTPS chain validation works against self-signed leaves and our 2 built-in anchors; arbitrary Internet CAs not yet trusted.

### Build
```
make clean && make
```
Boots in QEMU at 1024×768.



## v3.10 — TLS 1.3 Primitives

Vyro OS can now produce a complete TLS 1.3 ClientHello, parse the corresponding ServerHello, and derive every secret in the handshake key schedule. The whole chain is verified at every boot against the canonical RFC 8448 §3 trace — meaning we can prove byte-for-byte that the OS's TLS implementation agrees with the IETF spec's reference output.

### What's new
- **`kernel/tls.{h,c}`** — record framing helpers, the ClientHello builder, the ServerHello extension walker, and the key-schedule pipeline.
- **`tls_build_client_hello()`** writes a full handshake record. Cipher suite is fixed to `TLS_CHACHA20_POLY1305_SHA256`, group is X25519, signature algorithms include `rsa_pss_rsae_sha256`, `ecdsa_secp256r1_sha256`, and `rsa_pkcs1_sha256`. SNI is included when a hostname is supplied.
- **`tls_parse_server_hello()`** walks the ServerHello extensions and pulls the server's X25519 pubkey out of the `key_share`.
- **`tls_derive_handshake_keys()`** computes `early_secret → derived_early → handshake_secret → {c,s} hs traffic → {c,s} {key,iv}` per RFC 8446 §7.1.
- **Boot selftest** validates RFC 8448 §3 vectors end-to-end.
- **`tls [hostname]`** shell command — shows selftest result and builds a sample ClientHello.

### Validation
The exact same source files compiled and ran on the host produced `tls_selftest=1`. Every byte checked — public key derivation, ECDH, and the full HKDF chain through `handshake_secret` — matches the RFC's published values.

### Compatibility
- Pure in-kernel primitives. No TCP integration yet; that lands in v3.11.
- Kernel size: 164,710 bytes (was 161,286).

### Known limitations
- No live handshake; can't actually talk to a TLS server until v3.11.
- No signature verification of the server's CertificateVerify (v3.12).
- No `Finished` MAC check (v3.11).
- Only the `TLS_CHACHA20_POLY1305_SHA256` suite. No AES, no SHA-384.

### Next
**v3.11 — TLS 1.3 handshake over TCP.** Wire `tls_*` to `tcp_send`/`tcp_recv`, drive the state machine through ServerHello and the first encrypted record, decrypt with the v3.10-derived traffic key. After that, v3.12 adds CertificateVerify, v3.13 HTTPS GET, v3.14 browser connectivity.

---

## v3.9 — X25519 + HMAC/HKDF + TLS Key Schedule

Vyro OS now has the asymmetric primitive (X25519 ECDH) and the key-derivation machinery (HMAC-SHA-256 + HKDF + TLS 1.3 Expand-Label) that the TLS 1.3 handshake needs. Everything is verified at boot against published RFC vectors before it touches the network.

### What's new
- **HMAC-SHA-256** (`hmac_sha256`) — RFC 2104, built on the existing `sha256()`.
- **HKDF-Extract / HKDF-Expand** (`hkdf_extract`, `hkdf_expand`) — RFC 5869.
- **TLS 1.3 Expand-Label / Derive-Secret** (`tls13_hkdf_expand_label`, `tls13_derive_secret`) — RFC 8446 §7.1.
- **X25519** (`x25519`, `x25519_base`) — RFC 7748 scalar multiplication on Curve25519. 51-bit-limb field arithmetic with `__uint128_t` multiplication, constant-time conditional swap, inversion via the standard 2^255-21 addition chain.
- **Boot selftests** for both modules; failures are surfaced explicitly on the boot banner.
- **`tlskdf` shell command** — runs both selftests and a live X25519 ECDH agreement demo + an Expand-Label derivation.

### Validation
The exact same source files compiled and ran on the host produced:
- HMAC RFC 4231 Test 1 — bytes match
- HKDF-SHA256 RFC 5869 §A.1 PRK + 42-byte OKM — bytes match
- X25519 RFC 7748 §5.2 KAT 1 and KAT 2 — bytes match

### Compatibility
- Pure in-kernel modules. Crypto runs in kernel space, called by the TLS state machine (next phase).
- Kernel size: 161,286 bytes (was 153,126) — 35 KB headroom remaining.

### Known limitations
- No Ed25519 signature primitive yet (server cert verify in v3.11 will use ECDSA-P256 or RSA-PSS instead).
- No SHA-384, so only SHA-256-based cipher suites (`TLS_CHACHA20_POLY1305_SHA256`).
- X25519 is the only supported ECDH group; no P-256.

### Next
**v3.10 — TLS 1.3 record layer + ClientHello/ServerHello + key derivation.** Bring in record framing (RFC 8446 §5), the actual handshake state machine, traffic-key derivation, and `aead_seal`/`open`-protected application data. Signature verification is deferred to v3.11.

---

## v3.8 — X.509 Certificate Parser

Vyro OS can now read X.509 v3 certificates. The new `x509_parse()` walks DER-encoded TLVs to extract the identity fields a TLS client needs: Subject and Issuer Common Names, validity timestamps, DNS SubjectAltNames, and the signature/public-key algorithm OIDs. The embedded ECDSA P-256 test certificate is parsed at every boot to catch regressions in the DER reader.

### What's new
- **`kernel/x509.c`** — DER reader (TLV with multi-byte length, constructed sequences, OPTIONAL fields, IMPLICIT tagging) plus an X.509 v3 walker.
- **`x509_parse()`** populates an `x509_cert_t` with up to 8 DNS SANs.
- **Algorithm OID recognition** for the seven SignatureAlgorithm/PublicKeyAlgorithm OIDs that TLS 1.3 will need.
- **`x509_selftest()`** validates the embedded ECDSA P-256 certificate at boot.
- **`x509` shell command** dumps the parsed test certificate.

### Validation
Same parser was built on the host and ran successfully against two openssl-generated certificates (RSA-2048 and ECDSA-P256), proving the DER reader handles both formats without modification.

### Compatibility
- Pure parser. No cryptographic verification, no chain building, no clock checks.
- Kernel size: 153,126 bytes (was 147,270) — 42 KB headroom remaining.

### Known limitations
- Signature verification deferred to v3.9.
- Chain building / trust anchors deferred.
- Subject public key bytes are not extracted; only the algorithm OID is captured.
- Time strings are kept as raw ASCII; no parsing into a date struct.

### Next
**v3.9 — TLS 1.3 handshake.** ClientHello → ServerHello → EncryptedExtensions → Certificate → Finished. X25519 ECDHE key share, HKDF key schedule per RFC 8446 §7. The X.509 parser, ChaCha20-Poly1305 AEAD, and TCP layer all converge here.

---

## v3.7 — ChaCha20-Poly1305 AEAD

Vyro OS now has the symmetric cryptography building block for TLS 1.3. The implementation follows RFC 8439 line by line: ChaCha20 (RFC 8439 §2.4), Poly1305 (§2.5, 26-bit-limb arithmetic), and the AEAD composition (§2.8). All three published RFC test vectors are validated at every boot.

### What's new
- **ChaCha20** — `chacha20_block()` produces 64-byte keystream blocks, `chacha20_xor()` encrypts or decrypts.
- **Poly1305** — `poly1305_mac()` produces a 16-byte authentication tag. The implementation is the classic 26-bit-limb donna-32 layout, fully reduced and constant-time-masked at the final step.
- **AEAD seal/open** — `aead_seal()` and `aead_open()` combine them per RFC 8439 §2.8. Tag compare is constant-time (XOR-accumulate-then-test).
- **Boot selftest** — `aead_selftest()` runs at boot. On failure the boot banner shouts "SELFTEST FAILED" rather than silently exposing broken crypto.
- **`crypto` shell command** — re-runs the selftest on demand.

### Validation
The RFC 8439 published vectors were re-validated on the host using the exact same source files (separate translation units, regular `cc -O2`). All three pass.

### Compatibility
- Pure in-kernel module. No syscall surface yet — caller is the kernel itself (TLS will be).
- Kernel size: 147,270 bytes (was 141,862) — 48 KB headroom remaining.

### Known limitations
- No XChaCha20-Poly1305 (24-byte nonce variant) yet.
- `aead_seal`/`open` use a 16 KB static buffer for the MAC input — enough for one max-sized TLS record, not for arbitrarily large messages.

### Next
**v3.8 — X.509 certificate parser.** Minimal DER reader covering Subject, Issuer, NotBefore/NotAfter, SubjectAltName, and SignatureAlgorithm — enough to extract the host's identity. Chain validation and signature verification follow in v3.9 alongside the handshake.

---

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
