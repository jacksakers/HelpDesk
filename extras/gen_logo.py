import os

src = os.path.join(os.path.dirname(__file__), '..', 'extras', 'companion_app', 'uploads', 'images', 'HelpDesk_Logo.bin')
dst = os.path.join(os.path.dirname(__file__), '..', 'src', 'logo_image.c')

src = os.path.normpath(src)
dst = os.path.normpath(dst)

with open(src, 'rb') as f:
    data = f.read()

with open(dst, 'w') as out:
    out.write('// Project  : HelpDesk\n')
    out.write('// File     : logo_image.c\n')
    out.write('// Purpose  : HelpDesk logo embedded in flash (LVGL v9 RGB565 binary, 480x320)\n')
    out.write('// Generated: from HelpDesk_Logo.bin — do not hand-edit\n\n')
    out.write('#include "logo_image.h"\n\n')
    out.write('// Stored in .rodata (flash). On ESP32-S3 const globals never consume DRAM.\n')
    out.write('const uint8_t helpdesk_logo_bin[%dU] = {\n' % len(data))
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        out.write('    ' + ', '.join('0x%02X' % b for b in chunk) + ',\n')
    out.write('};\n\n')
    out.write('const uint32_t helpdesk_logo_bin_size = %dU;\n' % len(data))

print('Done: %d bytes -> %s' % (len(data), dst))
