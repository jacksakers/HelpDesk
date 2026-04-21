# Project  : HelpDesk
# File     : routes/media.py
# Purpose  : Media upload routes (/api/media/*) — image and audio uploads
# Depends  : device_client, serial_transport, media_manager, settings_store

from pathlib import Path

from fastapi import APIRouter, File, HTTPException, UploadFile

import device_client
import media_manager
import serial_transport
import settings_store

router = APIRouter(prefix="/api/media")

_VALID_IMAGE_EXTS = {".jpg", ".jpeg", ".png", ".bmp", ".gif"}
_VALID_AUDIO_EXTS = {".mp3"}
_MAX_IMAGE_BYTES  = 10 * 1024 * 1024   # 10 MB
_MAX_AUDIO_BYTES  = 50 * 1024 * 1024   # 50 MB


@router.post("/image")
async def upload_image(file: UploadFile = File(...)):
    """Converts image to LVGL .bin, saves locally, then uploads to device."""
    ext = Path(file.filename or "").suffix.lower()
    if ext not in _VALID_IMAGE_EXTS:
        raise HTTPException(400, f"Invalid image type. Allowed: {sorted(_VALID_IMAGE_EXTS)}")
    data = await file.read()
    if len(data) > _MAX_IMAGE_BYTES:
        raise HTTPException(413, "Image exceeds 10 MB limit")
    processed, out_name = media_manager.process_image(data, file.filename or "image")
    saved = media_manager.save_image(processed, out_name)
    device_ip = settings_store.get("device_ip", "")
    remote_path = f"/images/{out_name}"
    sent = await media_manager.upload_to_device(saved, remote_path, device_ip) if device_ip else False
    if not sent:
        sent = await serial_transport.fs_upload(remote_path, saved.read_bytes())
    return {"status": "ok", "filename": out_name, "sent_to_device": sent}


@router.post("/audio")
async def upload_audio(file: UploadFile = File(...)):
    """Saves an MP3 locally then uploads to device."""
    ext = Path(file.filename or "").suffix.lower()
    if ext not in _VALID_AUDIO_EXTS:
        raise HTTPException(400, "Only .mp3 files are supported")
    data = await file.read()
    if len(data) > _MAX_AUDIO_BYTES:
        raise HTTPException(413, "Audio exceeds 50 MB limit")
    safe_name = file.filename or "audio.mp3"
    saved = media_manager.save_audio(data, safe_name)
    device_ip = settings_store.get("device_ip", "")
    remote_path = f"/mp3/{saved.name}"
    sent = await media_manager.upload_to_device(saved, remote_path, device_ip) if device_ip else False
    if not sent:
        sent = await serial_transport.fs_upload(remote_path, saved.read_bytes())
    return {"status": "ok", "filename": saved.name, "sent_to_device": sent}
