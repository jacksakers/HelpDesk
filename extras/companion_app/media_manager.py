# Project  : HelpDesk
# File     : media_manager.py
# Purpose  : Image processing and file delivery to HelpDesk SD card over Wi-Fi
# Depends  : Pillow (optional — falls back to raw passthrough if not installed)

import io
import logging
import re
import struct
from pathlib import Path

try:
    from PIL import Image
    _HAS_PIL = True
except ImportError:
    _HAS_PIL = False
    logging.warning("Pillow not found — images will be saved without resizing.")

_UPLOAD_DIR = Path(__file__).parent / "uploads"
_IMAGES_DIR = _UPLOAD_DIR / "images"
_AUDIO_DIR  = _UPLOAD_DIR / "audio"

# Target display resolution for the HelpDesk 3.5" screen
_DISPLAY_W = 480
_DISPLAY_H = 320


def _ensure_dirs() -> None:
    _IMAGES_DIR.mkdir(parents=True, exist_ok=True)
    _AUDIO_DIR.mkdir(parents=True,  exist_ok=True)


def _sanitize_filename(name: str) -> str:
    """Strips directory components and unsafe characters from a filename."""
    name = Path(name).name                    # Drop any path prefix
    name = re.sub(r"[^\w\-. ]", "_", name)   # Replace unsafe chars with _
    return name or "unnamed"


def _to_lvgl_bin(img: "Image.Image", w: int, h: int) -> bytes:
    """
    Convert a PIL Image to an LVGL v9 binary image file (.bin).

    Format (little-endian):
      Bytes 0-1  : magic (0x19), cf (0x12 = LV_COLOR_FORMAT_RGB565)
      Bytes 2-3  : flags (0x0000)
      Bytes 4-5  : width  (uint16)
      Bytes 6-7  : height (uint16)
      Bytes 8-9  : stride (uint16) = width * 2
      Bytes 10-11: reserved (0x0000)
      Remaining  : RGB565 pixels, row-major, little-endian
    """
    # Center-fit onto a black canvas of the exact target size
    canvas = Image.new("RGB", (w, h), (0, 0, 0))
    img.thumbnail((w, h), Image.LANCZOS)
    offset = ((w - img.width) // 2, (h - img.height) // 2)
    canvas.paste(img, offset)

    # LVGL v9 binary header: 12 bytes
    header = struct.pack("<BBHHHHH",
        0x19,       # magic
        0x12,       # LV_COLOR_FORMAT_RGB565
        0x0000,     # flags
        w,          # width
        h,          # height
        w * 2,      # stride (bytes per row)
        0x0000,     # reserved
    )

    # Convert RGB pixels to RGB565 little-endian
    raw = canvas.tobytes()   # RGBRGB... bytes
    pixels = bytearray(len(raw) // 3 * 2)
    for i in range(0, len(raw), 3):
        r, g, b = raw[i], raw[i + 1], raw[i + 2]
        rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        j = (i // 3) * 2
        pixels[j]     = rgb565 & 0xFF
        pixels[j + 1] = (rgb565 >> 8) & 0xFF

    return header + bytes(pixels)


def process_image(file_bytes: bytes, filename: str) -> tuple[bytes, str]:
    """
    Resizes the image to fit _DISPLAY_W × _DISPLAY_H on a black canvas and
    converts to LVGL v9 binary format (.bin) for ZenFrame.
    Returns (processed_bytes, output_filename).
    If Pillow is unavailable, returns the original bytes unchanged.
    """
    safe_stem = Path(_sanitize_filename(filename)).stem

    if not _HAS_PIL:
        return file_bytes, _sanitize_filename(filename)

    img = Image.open(io.BytesIO(file_bytes)).convert("RGB")
    out = _to_lvgl_bin(img, _DISPLAY_W, _DISPLAY_H)
    return out, f"{safe_stem}.bin"


def save_image(file_bytes: bytes, filename: str) -> Path:
    """Saves image bytes to uploads/images/. Returns the saved Path."""
    _ensure_dirs()
    dest = _IMAGES_DIR / _sanitize_filename(filename)
    dest.write_bytes(file_bytes)
    logging.info(f"[Media] Image saved: {dest}")
    return dest


def save_audio(file_bytes: bytes, filename: str) -> Path:
    """Saves MP3 bytes to uploads/audio/. Returns the saved Path."""
    _ensure_dirs()
    dest = _AUDIO_DIR / _sanitize_filename(filename)
    dest.write_bytes(file_bytes)
    logging.info(f"[Media] Audio saved: {dest}")
    return dest


async def upload_to_device(local_path: Path, remote_path: str, device_ip: str) -> bool:
    """
    Sends a processed file to the HelpDesk SD card via HTTP multipart POST.
    Routes on the ESP32:
      POST http://{device_ip}/upload/image  ->  saved to /images/<filename>
      POST http://{device_ip}/upload/audio  ->  saved to /mp3/<filename>
    Returns True on success.
    """
    try:
        import httpx
    except ImportError:
        logging.warning("[Media] httpx not installed — device upload skipped. Run: pip install httpx")
        return False

    is_audio   = remote_path.startswith("/mp3/")
    endpoint   = "audio" if is_audio else "image"
    url        = f"http://{device_ip}/upload/{endpoint}"
    mime       = "audio/mpeg" if is_audio else "application/octet-stream"

    try:
        async with httpx.AsyncClient(timeout=30.0) as client:
            with local_path.open("rb") as fh:
                resp = await client.post(
                    url,
                    files={"file": (local_path.name, fh, mime)},
                )
        if resp.status_code == 200:
            logging.info(f"[Media] Sent {local_path.name} to device ({endpoint})")
            return True
        logging.warning(f"[Media] Device returned HTTP {resp.status_code} for {url}")
    except Exception as e:
        logging.error(f"[Media] Device upload failed: {e}")
    return False
