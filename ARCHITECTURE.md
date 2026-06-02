# Vyro OS — Architecture

## Top-level layout
```
boot/        bootloaders (BIOS asm + UEFI C)
kernel/      kernel + subsystems
kernel/apps/ native desktop applications
drivers/     hardware drivers (rtl8139, ata, ps2, framebuffer, ...)
user/        sample ELF app + libvyro framework
include/     shared headers
docs/        design documents
```

## Kernel modules (Mn = module, arrows = "depends on")

```
boot.asm → kernel_entry.asm → kernel.c → {
    gdt, idt, pic, isr           # CPU / interrupts
    pmm, heap, paging            # memory
    timer, rtc                   # clocks
    task, switch, syscall        # multitasking
    elf, usermode                # ring 3
    pci → ata, rtl8139, usb      # bus + device drivers
    vfs                          # filesystem
    net → arp → udp → net_pump   # network stack
    compositor → gui → widgets → app → apps/*
}
```

## Networking subsystem (v3.1)

| Layer | File | Responsibility |
|-------|------|----------------|
| L1/L2 driver | `drivers/rtl8139.c` | DMA TX/RX, IRQ ack, RX ring drain |
| L2 queue | `kernel/net_io.c` | RX queue (16 slots), raw L2 send |
| L2.5 ARP | `kernel/arp.c` | 8-entry IP→MAC cache, request/reply |
| L3 IPv4 | `kernel/net.c` | identity (MAC/IP), RFC 1071 checksum |
| L4 UDP | `kernel/udp.c` | 16-port dispatch, RFC 768 checksum |
| Pump | `kernel/net_pump.c` | main-loop RX dispatcher (ARP → UDP) |
| L7 DHCP | `kernel/dhcp_real.c` | `udp_listen(68)`, OFFER parsing, applies IP |
| L7 DNS | `kernel/dns_real.c` | ephemeral-port query/response |

### UDP RX dispatch

```
rtl8139 IRQ → net_io rx_handler → enqueue
                                     │
                  net_pump_run() ────┘
                       │
                       ├── arp_input()   (consumes ARP frames)
                       └── udp_input()
                              │
                              └── port lookup → listener.cb(src_ip, src_port, data, len)
```

### UDP TX

```
caller → udp_send_to(dst_ip, ...)
            │
            ├── arp_resolve(dst_ip, 500ms)  (cache hit or broadcast fallback)
            ├── build_udp_frame(...)
            ├── compute IPv4 checksum, UDP pseudo-header checksum
            └── rtl8139_send(frame, len)
```

## Memory map
Unchanged from v2.0. See `docs/DESIGN.md`.

## Build system
Single top-level `Makefile`. Each `.c` has an explicit rule with no header dependencies (header changes need `make clean`). Targets: `build`, `all` (build + qemu), `debug` (build + qemu -s -S), `clean`.
