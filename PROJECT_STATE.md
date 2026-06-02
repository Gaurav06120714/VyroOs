# Vyro OS — Project State

**Current release:** v3.5
**Last update:** Phase v3.5 (TCP Reassembly + RTT-driven RTO + Fast Retransmit)

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
| Networking — TCP reassembly / RTT / fast retx | **shipped (v3.5)** | 1-slot OoO, RFC 6298 RTO, 3-dupack fast retx |
| Networking — TCP congestion window | next | v3.6 |
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
Next phase: **v3.6 — TCP congestion control** (cwnd / ssthresh, slow start, congestion avoidance, cwnd-throttled emit pacing).
