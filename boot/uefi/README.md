# Vyro OS — UEFI Boot Path (Phase 26)

Vyro OS supports **two boot paths**:

| Path | File | Default | Firmware |
|------|------|---------|----------|
| **BIOS / Legacy** | `boot/boot.asm` | ✅ used by `make` | SeaBIOS (built into QEMU) |
| **UEFI** | `boot/uefi/uefi_boot.c` | optional | OVMF (UEFI firmware) |

## Why UEFI?

Modern hardware ships with UEFI firmware instead of legacy BIOS. UEFI gives:
- A clean **Boot Services** API (no real-mode/`int 0x10` juggling)
- The **Graphics Output Protocol (GOP)** for the framebuffer (no VBE)
- **GPT** partitioning (vs MBR's 4-primary-partition / 2TB limits)
- **Secure Boot** — cryptographically signed bootloaders

## Boot sequence (UEFI)

```
Power on
   │
UEFI firmware (OVMF)
   │  reads the EFI System Partition (FAT32, GPT)
   ▼
/EFI/BOOT/BOOTX64.EFI   ← our uefi_boot.c
   │  1. LocateProtocol(GOP)      → framebuffer base
   │  2. GetMemoryMap()
   │  3. ExitBootServices()       → firmware hands over the machine
   ▼
Vyro OS kernel (0x10000)
```

## GPT (GUID Partition Table)

UEFI boots from a **GPT** disk with a FAT32 **EFI System Partition (ESP)**
containing `/EFI/BOOT/BOOTX64.EFI`. GPT replaces the legacy MBR:
- 128 partitions (vs 4 primary)
- 64-bit LBAs (vs 32-bit → 2TB limit)
- CRC32-protected headers + backup table at end of disk

## Secure Boot (preparation)

Secure Boot verifies the bootloader's signature against keys in firmware:
- **PK** (Platform Key) → **KEK** (Key Exchange Key) → **db** (allowed) / **dbx** (revoked)
- `BOOTX64.EFI` must be signed (`sbsign`) with a key whose cert is enrolled in `db`
- Vyro OS ships the boot stub unsigned for development; production would sign it.

## Building (requires a UEFI toolchain + OVMF)

```bash
# Compile the EFI application (PE/COFF, subsystem 10 = EFI app)
x86_64-w64-mingw32-gcc -e efi_main -nostdlib \
    -Wl,--subsystem,10 -o BOOTX64.EFI boot/uefi/uefi_boot.c

# Create an ESP and run under OVMF
mkdir -p esp/EFI/BOOT && cp BOOTX64.EFI esp/EFI/BOOT/
qemu-system-x86_64 -bios OVMF.fd -drive format=raw,file=fat:rw:esp
```

> The BIOS path (`make`) remains the default for QEMU because it needs no extra
> firmware. The UEFI stub above is the real, modern alternative and is wired to
> pass the GOP framebuffer to the kernel exactly like the BIOS path passes the
> VBE framebuffer via `0x0500`.
