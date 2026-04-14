"""
gen_logo_rle.py — Compress HelpDesk_Logo.bin into pixel-level RLE for flash storage.

RLE format (stored in src/logo_image.c):
  Sequence of (uint16_t count, uint16_t pixel) pairs, little-endian.
  count is capped at 32767 per entry so the high bit is always 0 (reserved for future use).
  A count of 0 is an end-of-data sentinel.

Decompressor (in display_Screen.cpp) unpacks into a PSRAM buffer and calls gfx.pushImage.
"""

import struct, os

SRC = os.path.join(os.path.dirname(__file__),
                   'companion_app', 'uploads', 'images', 'HelpDesk_Logo.bin')
DST = os.path.join(os.path.dirname(__file__), '..', 'src', 'logo_image.c')
DST = os.path.normpath(DST)

MAX_RUN = 32767  # fits in uint16_t with MSB free

data = open(SRC, 'rb').read()
# Strip the 12-byte LVGL header — we care only about raw RGB565 pixels
pixels_raw = data[12:]
pixel_count = len(pixels_raw) // 2
words = struct.unpack_from('<%dH' % pixel_count, pixels_raw)

# Build RLE pairs: (count, pixel)
rle = []
i = 0
while i < len(words):
    j = i + 1
    while j < len(words) and words[j] == words[i] and (j - i) < MAX_RUN:
        j += 1
    rle.append((j - i, words[i]))
    i = j

rle_bytes = struct.pack('<%dH' % (len(rle) * 2),
                        *[v for pair in rle for v in pair])

print(f'Original pixels : {pixel_count}  ({len(pixels_raw)} bytes)')
print(f'RLE entries     : {len(rle)}')
print(f'Compressed      : {len(rle_bytes)} bytes  ({len(rle_bytes)/len(pixels_raw)*100:.1f}% of original)')

with open(DST, 'w') as f:
    f.write('// Project  : HelpDesk\n')
    f.write('// File     : logo_image.c\n')
    f.write('// Purpose  : HelpDesk logo — pixel RLE compressed, embedded in flash\n')
    f.write('// Generated: from HelpDesk_Logo.bin via gen_logo_rle.py — do not hand-edit\n')
    f.write('//\n')
    f.write('// RLE format: sequence of (uint16_t count, uint16_t pixel) pairs LE.\n')
    f.write('// count capped at 32767. Decompress by repeating each pixel <count> times.\n\n')
    f.write('#include "logo_image.h"\n\n')
    f.write(f'const uint8_t helpdesk_logo_rle[{len(rle_bytes)}U] = {{\n')
    for i in range(0, len(rle_bytes), 16):
        chunk = rle_bytes[i:i+16]
        f.write('    ' + ', '.join('0x%02X' % b for b in chunk) + ',\n')
    f.write('};\n\n')
    f.write(f'const uint32_t helpdesk_logo_rle_size = {len(rle_bytes)}U;\n')

print(f'Written: {DST}')
