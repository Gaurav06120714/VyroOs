#ifndef LBA_XLATE_H
#define LBA_XLATE_H

#include "../include/types.h"

// LBA-size translation adapter (vC.6.9).
//
// FAT32 and friends in this kernel assume 512-byte logical sectors. Real
// modern NVMe SSDs are increasingly 4 KiB-native: a single LBA reads or
// writes 4096 bytes. lba_xlate sits between the FS layer (which thinks
// in 512-byte LBAs) and the block layer (which speaks the device's
// native LBA size), translating addresses + slicing buffers.
//
//   FS-visible LBA  (512B)              0  1  2  3  4  5  6  7  8  9 ...
//   Device LBA      (4096B)             [ 0          ] [ 1          ]...
//
// One FS-side read of LBA N becomes:
//   device LBA = N / (4096/512) = N / 8
//   offset     = (N % 8) * 512
//   read 4096 into a bounce buffer, memcpy 512 bytes from offset.
//
// Writes are read-modify-write on partial 4KiB blocks. Aligned 4KiB-
// multiple I/Os pass through without bounce.

// Helper used by block.c when registering a device whose logical block
// size != 512. Returns 1 on success, 0 if unsupported.
int lba_xlate_register(uint32_t block_idx);

#endif
