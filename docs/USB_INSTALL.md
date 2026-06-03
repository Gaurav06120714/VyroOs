# Installing Vyro OS on a USB stick

## What works

| Hardware | Boots Vyro? |
|---|---|
| Legacy-BIOS x86_64 PC (most desktops < 2015, Intel NUC, ThinkPad with CSM enabled) | ✅ |
| UEFI PC with CSM (Compatibility Support Module) enabled in firmware setup | ✅ |
| Pure UEFI x86_64 PC (CSM disabled) | ❌ (UEFI loader incomplete) |
| Intel Mac (pre-2018, no T2) | ⚠️ Holding ⌥ Option at boot may show the USB. Vyro's BIOS bootloader runs through Apple's CSM-equivalent. |
| Intel Mac with T2 chip (2018+) | ❌ unless you disable Secure Boot in Recovery + allow boot from external media |
| Apple Silicon Mac (M1, M2, M3, M4) | ❌ **completely impossible** — these are ARM64, run signed Apple firmware only, and Vyro OS is x86_64 |

## Build the image

```bash
cd /Users/gaurav/Desktop/MyProjects/VyroEcosystem/VyroOs
make clean
make
make usb
```

This produces `build/vyro-usb.img` — a 32 MB raw bootable image.

## Write to a USB stick (macOS host)

```bash
# 1. Plug in a USB stick (≥ 32 MB; anything will do — even an old one).

# 2. Find its disk number:
diskutil list
# Look for /dev/diskN where N corresponds to your USB stick.
# DO NOT pick disk0 — that's your Mac's internal drive!

# 3. Unmount it:
diskutil unmountDisk /dev/diskN

# 4. Write the image (the 'r' prefix = raw device, ~10x faster):
sudo dd if=build/vyro-usb.img of=/dev/rdiskN bs=1m
# Enter your password when prompted.

# 5. Eject:
diskutil eject /dev/diskN
```

## Boot from it

### On a legacy-BIOS PC
1. Insert the USB stick.
2. Power on while pressing **F12 / F11 / Esc / Del** (depends on motherboard) to enter the boot menu.
3. Pick the USB device.
4. You should see the boot chime + boot banner + shell prompt.

### On an Intel Mac (pre-T2)
1. Insert the USB stick.
2. Power on while holding **⌥ Option** (Startup Manager).
3. If the USB shows up as a boot device — pick it.
4. **It likely won't show up.** Apple's EFI firmware filters for blessed APFS/HFS volumes by default; raw MBR images are usually invisible to the Startup Manager. There's no clean workaround for this without rewriting our bootloader as a proper EFI binary.

### On a T2 Mac or Apple Silicon Mac
**You can't.** Apple Secure Boot blocks unsigned operating systems. T2 Macs need Recovery → Startup Security → "No Security" + "Allow booting from external media", and even then they expect a signed bootloader. Apple Silicon Macs have a completely different boot chain (iBoot → m1n1-class trampoline → kernel) and Vyro OS doesn't have an ARM64 port.

## What you'll see if it boots

```
[OK] Bootloader (BIOS, 512 B)
[OK] Long mode (64-bit), GDT, IDT, PIC, syscalls
[OK] PMM + heap + 4 GiB paging
[OK] PCI bus scan
...
[OK] Boot chime (5-note arpeggio)
                                       *** boot chime plays ***
   Vyro OS v5.x  —  shell ready
   Type `help` for 60+ commands.

vyro$
```

You can type `crypto`, `tls`, `widgets`, `gui`, `cpuinfo`, `acpi`, `ahci`, `e1000`, `nvme`, etc. **Do not expect Chrome, real WiFi, or real battery** — see `AUDIT.md` for why.

## If you want a real OS that does all of this

Use **Ubuntu** or another mainstream Linux distro. Their USB installers handle every modern Mac (with caveats per generation), drive real WiFi, run Chrome, and show actual battery percentage. Vyro OS is a hobby learning project — not a daily driver.
