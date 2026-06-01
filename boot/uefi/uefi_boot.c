// ─────────────────────────────────────────────────
// Vyro OS — UEFI Bootloader (Phase 26)
//
// Alternative boot path to the BIOS bootloader (boot/boot.asm).
// A UEFI application that:
//   1. Locates the Graphics Output Protocol (GOP) for a framebuffer
//   2. Gets the memory map
//   3. Exits Boot Services
//   4. Jumps to the Vyro OS kernel
//
// Build (requires gnu-efi or the EDK2 toolchain + OVMF firmware):
//   x86_64-w64-mingw32-gcc -e efi_main -nostdlib -Wl,--subsystem,10 \
//       -o BOOTX64.EFI boot/uefi/uefi_boot.c
//
// This is the modern firmware path. Secure Boot and GPT notes are in
// docs/DESIGN.md. The BIOS path remains the default for QEMU testing.
// ─────────────────────────────────────────────────

#include <stdint.h>

// Minimal EFI type definitions (subset — a real build uses efi.h)
typedef uint64_t EFI_STATUS;
typedef void*    EFI_HANDLE;
typedef uint16_t CHAR16;

#define EFI_SUCCESS 0
#define EFIAPI

typedef struct {
    uint32_t Width;
    uint32_t Height;
    uint32_t PixelFormat;
    uint32_t PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef struct {
    uint32_t MaxMode;
    uint32_t Mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* Info;
    uint64_t SizeOfInfo;
    uint64_t FrameBufferBase;     // The linear framebuffer address
    uint64_t FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

typedef struct _EFI_GRAPHICS_OUTPUT_PROTOCOL {
    void* QueryMode;
    void* SetMode;
    void* Blt;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE* Mode;
} EFI_GRAPHICS_OUTPUT_PROTOCOL;

typedef struct {
    char                       _pad[60];
    EFI_STATUS (EFIAPI *ConOutOutputString)(void* This, CHAR16* String);
} SIMPLE_TEXT_OUTPUT;

typedef struct {
    char    _hdr[60];
    void*   _pad1[6];
    void*   ConOut;             // SIMPLE_TEXT_OUTPUT*
    void*   _pad2[3];
    void*   BootServices;       // EFI_BOOT_SERVICES*
} EFI_SYSTEM_TABLE;

// Boot services subset: LocateProtocol, GetMemoryMap, ExitBootServices
typedef struct {
    char    _hdr[24];
    void*   _raise_tpl;
    void*   _restore_tpl;
    void*   _allocate_pages;
    void*   _free_pages;
    EFI_STATUS (EFIAPI *GetMemoryMap)(uint64_t*, void*, uint64_t*, uint64_t*, uint32_t*);
    void*   _allocate_pool;
    void*   _free_pool;
    char    _pad[10 * 8];
    EFI_STATUS (EFIAPI *ExitBootServices)(EFI_HANDLE, uint64_t);
    char    _pad2[8 * 8];
    EFI_STATUS (EFIAPI *LocateProtocol)(void* Protocol, void* Registration, void** Interface);
} EFI_BOOT_SERVICES;

// GOP GUID: {9042A9DE-23DC-4A38-96FB-7ADED080516A}
static uint8_t GOP_GUID[16] = {
    0xde,0xa9,0x42,0x90,0xdc,0x23,0x38,0x4a,
    0x96,0xfb,0x7a,0xde,0xd0,0x80,0x51,0x6a
};

// The kernel is loaded at this physical address by the BIOS path;
// the UEFI path would load the kernel from the EFI System Partition.
#define KERNEL_ENTRY 0x10000

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE* st) {
    EFI_BOOT_SERVICES* bs = (EFI_BOOT_SERVICES*) st->BootServices;

    // 1. Locate the Graphics Output Protocol → get framebuffer
    EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = 0;
    bs->LocateProtocol(GOP_GUID, 0, (void**)&gop);

    uint64_t fb_base = 0;
    if (gop && gop->Mode) {
        fb_base = gop->Mode->FrameBufferBase;
        // Hand the framebuffer address to the kernel at 0x0500
        *(volatile uint32_t*)0x0500 = (uint32_t)fb_base;
    }

    // 2. Get memory map (size first), 3. Exit Boot Services
    uint64_t map_size = 0, map_key = 0, desc_size = 0;
    uint32_t desc_ver = 0;
    static uint8_t map_buf[16384];
    map_size = sizeof(map_buf);
    bs->GetMemoryMap(&map_size, map_buf, &map_key, &desc_size, &desc_ver);
    bs->ExitBootServices(image, map_key);

    // 4. Jump to the kernel entry point (already loaded by firmware/loader)
    void (*kernel_entry)(void) = (void(*)(void)) KERNEL_ENTRY;
    kernel_entry();

    for (;;) {}
    return EFI_SUCCESS;
}
