# Vyro OS — Project State

**Current release:** v6.0
**Last update:** v6.0 release — production-OS audit, v6 roadmap, ARM64 HAL skeleton, real CPUID, ACPI table walker + MADT, AHCI / E1000 / NVMe controller detection, USB image build target

## v3.11 → v3.30 highlights

| Phase | Title |
|---|---|
| v3.11 | TLS 1.3 handshake over TCP — server Finished MAC verified |
| v3.12 | Preemptive scheduler tick (cooperative-tickle via PIT) |
| v3.13 | FAT32 read-only driver |
| v3.14 | Local APIC initialization |
| v3.15 | Glassmorphism compositor primitives (blur, tint, rounded panels) |
| v3.16 | Bootloader sector budget bumped 192 KB → 384 KB |
| v3.17 | ChaCha20-based CSPRNG, RDRAND + RDTSC seeded |
| v3.18 | FAT32 writes + subdirectory navigation |
| v3.19 | SMP AP bring-up (real-mode trampoline) |
| v3.20 | RSA-2048 PKCS1-v1_5 signature verification |
| v3.21 | HTTP/1.1 GET client |
| v3.22 | xHCI controller detection + capability parse |
| v3.23 | X.509 RSA chain hook + bignum bug fix (binary long division) |
| v3.24 | TLS application traffic keys + client Finished — HTTPS GET works |
| v3.25 | SMP AP long-mode trampoline (real → 32 → 64 bit) |
| v3.26 | TLS Certificate parse + hostname match + RSA self-sign verify |
| v3.27 | ECDSA P-256 verification (RFC 6979 KAT verified) |
| v3.28 | Trust anchors + multi-cert chain validation |
| v3.29 | SMP APs reach C — per-CPU stack + `ap_main()` entry |
| v3.30 | xHCI controller halt + reset |

## Subsystem matrix

| Subsystem | Status | Notes |
|-----------|--------|-------|
| Bootloader (BIOS + UEFI) | shipped | 64-bit long mode, 192 KB kernel headroom |
| GDT / IDT / PIC / ISRs | shipped | int 0x80 syscalls |
| Memory: PMM / heap / paging | shipped | 4 GB identity-mapped |
| Scheduler | cooperative | preemptive lands in Scheduler Phase 1 |
| Ring 3 / ELF64 | shipped | sample `init` user binary |
| Filesystem (VyFS) | shipped | RAM-backed; FAT32 in Filesystem Phase 1 |
| ATA disk driver | shipped | PIO, persistent scratch image |
| Networking — L2 (RTL8139) | shipped | real TX/RX DMA, IRQ-driven |
| Networking — Ethernet/IPv4 | shipped | RFC 1071 checksum |
| Networking — ARP | shipped (v3.0) | 8-entry cache, request/reply |
| Networking — ICMP echo + live ping | shipped (v3.0) | RTT reported |
| Networking — UDP transport | shipped (v3.1) | port-dispatch, RFC 768 checksum |
| Networking — DHCP client | shipped, refactored on UDP | live DHCPDISCOVER/OFFER over RTL8139 |
| Networking — DNS resolver | shipped, refactored on UDP | real UDP/53 queries |
| Networking — TCP active open | shipped (v3.2) | SYN/SYN-ACK/ACK, FIN close, RST, retransmit |
| Networking — TCP listen/accept | shipped (v3.3) | passive open, SYN_RECEIVED, accept queue |
| Networking — TCP data transfer | shipped (v3.4) | send/recv, 1024-byte buffers |
| Networking — TCP reassembly / RTT / fast retx | shipped (v3.5) | 1-slot OoO, RFC 6298 RTO, 3-dupack fast retx |
| Networking — TCP congestion window | shipped (v3.6) | cwnd/ssthresh, slow start, CA, cwnd-throttled emit |
| Networking — ChaCha20-Poly1305 AEAD | **shipped (v3.7)** | RFC 8439, host-verified selftest at boot |
| Networking — X.509 parsing | **shipped (v3.8)** | DER reader, CN/SAN/validity/alg extraction, no verification |
| Crypto — HMAC/HKDF + TLS KDF | **shipped (v3.9)** | RFC 2104/5869, TLS 1.3 Expand-Label / Derive-Secret |
| Crypto — X25519 ECDH | **shipped (v3.9)** | RFC 7748, 51-bit-limb field arithmetic, KAT-verified |
| Networking — TLS 1.3 primitives (records + CH/SH + key schedule) | **shipped (v3.10)** | RFC 8448 §3 vectors verified |
| Networking — TLS 1.3 wire (handshake over TCP) | next | v3.11 |
| Networking — server cert signature verification | future | v3.12 |
| Networking — HTTPS client | future | v3.13 |
| SMP | detection only | bring-up in SMP Phase 3 |
| Desktop / compositor | shipped | dark/light theme, 12+ native apps |
| Browser | scaffolded | HTTP client lands in Browser Phase 1 |
| Package manager | shipped | local index + GUI store |

## Networking layer cake (post-v3.1)

```
┌─────────────────────────────────────────────────┐
│  shell / dhcp_real / dns_real / future tcp      │
├─────────────────────────────────────────────────┤
│  udp.c   ← port dispatch, RFC 768 checksum      │
├─────────────────────────────────────────────────┤
│  arp.c   ← 8-entry cache, request/reply         │
├─────────────────────────────────────────────────┤
│  net_io.c  ← RX queue, raw L2/L3 send helpers   │
├─────────────────────────────────────────────────┤
│  drivers/rtl8139.c  ← real DMA TX/RX, IRQ       │
└─────────────────────────────────────────────────┘
       ▲                ▲
       │                │
  net_pump.c (main-loop pump: ARP → UDP → ICMP/TCP)
```

## Active roadmap pointer
Next phase: **v3.11 — TLS 1.3 handshake over TCP** (wire the v3.10 primitives to `tcp_send`/`tcp_recv`, receive ServerHello, decrypt the first encrypted record; no cert signature verification yet — that's v3.12).
