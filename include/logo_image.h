// Project  : HelpDesk
// File     : logo_image.h
// Purpose  : Exposes the flash-embedded HelpDesk logo (480x320 RGB565, RLE compressed)
// Generated: from HelpDesk_Logo.bin via gen_logo_rle.py — do not hand-edit
//
// RLE format: sequence of (uint16_t count, uint16_t pixel) pairs, little-endian.
// Decompress: for each pair, write <pixel> to output <count> times.
// Total uncompressed output: LOGO_WIDTH * LOGO_HEIGHT * 2 bytes (RGB565).

#pragma once
#include <stdint.h>

// RLE-compressed pixel data stored in .rodata (flash).
extern const uint8_t  helpdesk_logo_rle[];
extern const uint32_t helpdesk_logo_rle_size;

#define LOGO_WIDTH   480
#define LOGO_HEIGHT  320
