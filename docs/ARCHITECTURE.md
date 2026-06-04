# Vyro OS — Architecture (v5.0)

## Top-level layout
```
boot/        bootloaders (BIOS asm + UEFI C)
kernel/      kernel + subsystems (~80 .c/.h files)
kernel/apps/ native desktop applications
drivers/     hardware drivers (rtl8139, ata, ps2, framebuffer, speaker, ...)
user/        ELF user programs (init, hello) + libvyro framework
include/     shared headers
docs/        design documents
```

## Kernel subsystems

```
boot.asm → kernel_entry.asm → kernel.c
                                  │
       ┌──────────────────────────┼──────────────────────────────────┐
       ▼                          ▼                                  ▼
   CPU / interrupts          Memory                            Subsystem init
   ─────────────────         ──────────────                    (boot banner)
   gdt, idt, pic, isr        pmm, heap, paging                       │
   timer, rtc, sched         (4 GiB identity)                        │
       │                          │                                  │
       └──────────────────────────┴──────── task, switch, syscall ───┤
                                              elf, usermode          │
                                                                     ▼
                                         pci → ata, rtl8139, usb (xhci)
                                         vfs ─── fat32 ── files app
                                         net → arp → udp → tcp → tls → http
                                         crypto: sha256, hmac, hkdf,
                                                 chacha20, poly1305, aead,
                                                 x25519, csprng, bignum,
                                                 bignum_4k, rsa, rsa_pss,
                                                 ecdsa, x509, trust
                                         smp_boot (LAPIC + AP trampoline)
                                         compositor → wallpaper → widgets →
                                                       gui → apps/*
```

## Networking + TLS layer cake

| Layer | Files | Responsibility |
|---|---|---|
| L1/L2 driver | `drivers/rtl8139.c` | DMA TX/RX, IRQ ack, RX ring drain |
| L2 queue | `kernel/net_io.c` | RX queue, raw L2 send |
| L2.5 ARP | `kernel/arp.c` | IP→MAC cache, request/reply |
| L3 IPv4 | `kernel/net.c` | Identity, RFC 1071 checksum |
| L3 IPv6 | `kernel/ipv6.c` | Link-local fe80::/10 via EUI-64; ICMPv6 echo + NDP |
| L4 UDP | `kernel/udp.c` | 16-port dispatch, RFC 768 checksum |
| L4 TCP | `kernel/tcp.c` | RFC 793 client+server, RTT (RFC 6298), congestion (RFC 5681) |
| Crypto | `chacha20`, `poly1305`, `aead`, `sha256`, `hkdf`, `x25519`, `csprng`, `bignum`, `bignum_4k`, `rsa`, `rsa_pss`, `ecdsa` | RFC 8439, 5869, 7748, 8017, 6979 |
| X.509 | `kernel/x509.c`, `trust.c`, `trust_anchors.c` | DER reader, chain validation, 5 built-in anchors |
| TLS 1.3 | `kernel/tls.c` | Client (RFC 8446) + Server (`tls_accept`); ChaCha20-Poly1305 |
| Pump | `kernel/net_pump.c` | ARP → UDP → TCP → IPv6 dispatch + `tcp_tick()` |
| L7 HTTP | `kernel/http.c` | GET request build + RFC 7230 response parse |
| L7 DHCP | `kernel/dhcp_real.c` | DISCOVER/OFFER over UDP |
| L7 DNS | `kernel/dns_real.c` | UDP/53 query + answer parse |

### TLS handshake state machine

```
client                                    server
  │                                          │
  │── ClientHello ─────────────────────────► │
  │                          (tls_parse_client_hello)
  │                                          │
  │ ◄──── ServerHello (plaintext) ────────── │   tls_build_server_hello
  │                                          │   x25519 + derive_handshake_keys
  │                                          │
  │ ◄── enc(EE | Cert | ServerFinished) ──── │   tls_build_certificate_msg +
  │  (decrypt + verify Finished MAC)         │   tls_build_server_finished
  │                                          │
  │── enc(ClientFinished) ─────────────────► │   tls_accept loops + decrypts +
  │                          (verify MAC)    │   compares HMAC(c_finished_key, th)
  │                                          │
  │ ◄═══ App traffic keys derived ═══════════►│
  │             (master_secret → c/s ap traffic)
  │                                          │
  │── enc(HTTP GET) ──────────────────────► │
  │ ◄── enc(HTTP/1.1 200) ───────────────── │
```

## Cryptographic verification matrix

| Primitive | RFC vector | Bound | Status |
|---|---|---|---|
| SHA-256 | NIST | one-shot | shipped |
| HMAC-SHA-256 | RFC 4231 §4.2 Test 1 | up to 16 KB msg | shipped |
| HKDF | RFC 5869 §A.1 | one-shot | shipped |
| ChaCha20 | RFC 8439 §2.4.2 | unbounded | shipped |
| Poly1305 | RFC 8439 §2.5.2 | up to 16 KB record | shipped |
| AEAD seal/open | RFC 8439 §2.8.2 | 16 KB max | shipped |
| X25519 | RFC 7748 §5.2 KAT 1+2 | one-shot | shipped |
| RSA-2048 verify | local KAT (modexp) | n_len ≤ 256 B | shipped |
| RSA-4096 verify | local KAT | n_len ≤ 512 B | shipped |
| RSA-PSS-SHA256 | RFC 8017 §9.1.2 | both moduli | shipped |
| ECDSA-P256 verify | RFC 6979 §A.2.5 | one-shot | shipped |

## SMP boot path

```
BSP startup (kernel.c)
  ├── lapic_init               (IA32_APIC_BASE MSR, SVR enable)
  ├── smp_start_aps
  │     ├── copy 174-byte trampoline → 0x8000
  │     ├── publish PML4 phys at 0x9008, ap_main pointer at 0x9010
  │     ├── INIT IPI (broadcast all-excluding-self)
  │     └── SIPI × 2 with vector 0x08
  └── continue boot
                                      ↓
AP startup (smp_trampoline.asm)
  ├── 16-bit real mode (CR0.PE clear)
  ├── load minimal GDT
  ├── 32-bit protected mode (CR0.PE=1, far jmp 0x08:pm32)
  ├── enable PAE, load shared PML4, set EFER.LME, enable PG
  ├── 64-bit long mode (far jmp 0x18:lm64)
  ├── read LAPIC ID, set per-CPU stack at 0x200000 + (id+1)*64K
  └── call ap_main(apic_id) → presence map + HLT loop
```

## Trust + chain validation

```
TLS Certificate handshake msg arrives
        │
        ▼
process_certificate_msg
        ├── parse CertificateList per RFC 8446 §4.4.2
        ├── x509_parse each entry → x509_cert_t[chain_n]
        ▼
walk_chain
        ├── for i = 0..n-1:  x509_verify_signature(chain[i], chain[i+1])
        │       └── dispatch on sig_alg:
        │           - SHA256_RSA + n_len ≤ 256: rsa_pkcs1_v15_sha256_verify
        │           - SHA256_RSA + n_len  > 256: rsa4k_pkcs1_v15_sha256_verify
        │           - ECDSA_SHA256: ecdsa_p256_sha256_verify
        │           - RSA_PSS_SHA256: rsa_pss_sha256_verify
        ├── trust_find_by_subject_cn(chain[n-1].issuer_cn) → anchor
        └── x509_verify_signature(chain[n-1], anchor) → final link
```

## Memory map (4 GiB identity-paged)

```
0x00000000 - 0x00007BFF   real-mode stack region (legacy)
0x00007C00 - 0x00007DFF   bootloader (loaded by BIOS)
0x00008000 - 0x00008FFF   SMP AP trampoline (planted at runtime)
0x00009000 - 0x0000FFFF   SMP shared scratch (AP flag, PML4 ptr, ap_main ptr)
0x00010000 - 0x0004FFFF   kernel.bin (~250 KB / 384 KB ceiling)
0x00100000 - 0x00101FFF   PMM bitmap
0x00200000 - 0x0023FFFF   per-CPU AP stacks (64 KB × up to 4 CPUs)
0x00500000 - ...          kernel heap
0xFEE00000                LAPIC MMIO (default)
```

## Filesystem topology

```
                 vfs_root()
                     │
   ┌─────────────────┼─────────────────────────┐
   │       VyFS (in-RAM, mode + uid + symlink) │
   │                 │                         │
   │  ┌──── / ───── home ─── user ─── notes.txt│
   │  │    bin                                 │
   │  │    readme.txt                          │
   │  └──── … ───────────────────────────────  │
   ├───────────────────────────────────────────┤
   │       FAT32 on secondary ATA disk         │
   │       fat32_mount() / fat32_list_root /   │
   │       fat32_read_file / fat32_write_file  │
   │                                           │
   │  Toggled via 'FAT32' button in Files app  │
   └───────────────────────────────────────────┘
```

## Build pipeline

```
boot/boot.asm  ─nasm-f bin─►  build/boot.bin (512 B)
kernel/*.c    ─x86_64-elf-gcc─►  build/*.o
kernel/*.asm  ─nasm-f elf64──►  build/*.o
kernel/smp_trampoline.asm ─nasm-f bin─►  build/smp_trampoline.bin
                                                │
                                       incbin into smp_trampoline_blob.o
user/init.c   ─compile + link─►  build/init.elf
              ─python embed─►   build/user_init.c → user_init.o
user/hello.c  (same pipeline) → user_hello.o
                                                │
                                                ▼
all build/*.o ─x86_64-elf-ld─► build/kernel.bin (~250 KB / 384 KB max)
boot.bin + kernel.bin → vyro.img (1.44 MB floppy image)
```

## Honest scope notes
- True IRQ-driven preemption is deferred — needs serial-console + GDB harness to validate safely
- xHCI Address Device + Configure Endpoint scaffolded; HID enumeration is a future track
- AES-GCM ciphersuite, Ed25519 sig, SHA-384, TCP-over-IPv6 not yet implemented
- Mozilla CA bundle not embedded — primitive ready (`bignum_4k` + `rsa_pss`), bundle is its own size-vs-budget call
- ACPI S3 sleep is a stub that just halts; full DSDT parsing required for real suspend
