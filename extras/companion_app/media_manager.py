# Project  : HelpDesk
# File     : media_manager.py
# Purpose  : Image processing and file delivery to HelpDesk SD card over Wi-Fi
# Depends  : Pillow (optional — falls back to raw passthrough if not installed)

import io
import logging
import re
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


def process_image(file_bytes: bytes, filename: str) -> tuple[bytes, str]:
    """
    Resizes the image to fit _DISPLAY_W × _DISPLAY_H and converts to BMP.
    Returns (processed_bytes, output_filename).
    If Pillow is unavailable, returns the original bytes unchanged.
    """
    safe_stem = Path(_sanitize_filename(filename)).stem

    if not _HAS_PIL:
        return file_bytes, _sanitize_filename(filename)

    img = Image.open(io.BytesIO(file_bytes)).convert("RGB")
    img.thumbnail((_DISPLAY_W, _DISPLAY_H), Image.LANCZOS)

    out = io.BytesIO()
    img.save(out, format="BMP")
    return out.getvalue(), f"{safe_stem}.bmp"


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
    mime       = "audio/mpeg" if is_audio else "image/bmp"

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
