# Project  : HelpDesk
# File     : routes/drive.py
# Purpose  : DeskDrive proxy routes (/api/drive/*) — SD card browser and editor
# Depends  : device_client, serial_transport, media_manager, settings_store

import logging
import posixpath
from pathlib import Path as _Path

from fastapi import APIRouter, File, HTTPException, Request, UploadFile
from fastapi.responses import Response
from pydantic import BaseModel

import device_client
import media_manager
import serial_transport
import settings_store

router = APIRouter(prefix="/api/drive")

_ERR_UNREACHABLE = "Device unreachable (WiFi and serial both failed)."


class _DriveWriteBody(BaseModel):
    path: str
    content: str


@router.get("/list")
async def drive_list(dir: str = "/"):
    path = device_client.validate_sd_path(dir)
    base = device_client.device_base_url()
    if base:
        result = await device_client.wifi_get(f"{base}/api/fs/list", params={"dir": path}, timeout=8.0)
        if result is not None:
            return result
    result = await serial_transport.fs_list(path)
    if result is not None:
        return result
    raise HTTPException(503, _ERR_UNREACHABLE)


@router.get("/download")
async def drive_download(path: str):
    clean = device_client.validate_sd_path(path)
    base = device_client.device_base_url()
    content: bytes | None = None
    mime = "application/octet-stream"

    if base:
        result = await device_client.wifi_get_bytes(
            f"{base}/api/fs/download", params={"path": clean}, timeout=30.0
        )
        if result is not None:
            content, mime = result

    if content is None:
        content = await serial_transport.fs_download(clean)
        if content is None:
            raise HTTPException(503, _ERR_UNREACHABLE)
        ext = clean.rsplit(".", 1)[-1].lower() if "." in clean else ""
        mime_map = {
            "txt": "text/plain", "md": "text/plain", "json": "text/plain",
            "jpg": "image/jpeg", "jpeg": "image/jpeg", "png": "image/png",
            "bmp": "image/bmp", "mp3": "audio/mpeg",
        }
        mime = mime_map.get(ext, "application/octet-stream")

    filename = clean.rsplit("/", 1)[-1] or "download"
    return Response(
        content=content,
        media_type=mime,
        headers={"Content-Disposition": f'attachment; filename="{filename}"'},
    )


@router.post("/upload")
async def drive_upload(dir: str = "/", file: UploadFile = File(...)):
    dest_dir = device_client.validate_sd_path(dir)
    base = device_client.device_base_url()
    data = await file.read()
    if len(data) > 50 * 1024 * 1024:
        raise HTTPException(413, "File too large (50 MB max)")
    if base:
        result = await device_client.wifi_post(
            f"{base}/api/fs/upload",
            files={"file": (file.filename, data, "application/octet-stream")},
            params={"dir": dest_dir},
            timeout=60.0,
        )
        if result is not None:
            return {"ok": True, "filename": file.filename}
    dest_path = posixpath.join(dest_dir, file.filename or "upload.bin")
    ok = await serial_transport.fs_upload(dest_path, data)
    if ok:
        return {"ok": True, "filename": file.filename}
    raise HTTPException(503, _ERR_UNREACHABLE)


@router.post("/mkdir")
async def drive_mkdir(body: dict):
    path = device_client.validate_sd_path(body.get("path", ""))
    base = device_client.device_base_url()
    if base:
        result = await device_client.wifi_post(f"{base}/api/fs/mkdir", json={"path": path}, timeout=8.0)
        if result is not None:
            return result
    ok = await serial_transport.fs_mkdir(path)
    if ok:
        return {"ok": True}
    raise HTTPException(503, _ERR_UNREACHABLE)


@router.post("/delete")
async def drive_delete(body: dict):
    path = device_client.validate_sd_path(body.get("path", ""))
    if path == "/":
        raise HTTPException(400, "Cannot delete root")
    base = device_client.device_base_url()
    if base:
        result = await device_client.wifi_post(f"{base}/api/fs/delete", json={"path": path}, timeout=8.0)
        if result is not None:
            return result
    ok = await serial_transport.fs_delete(path)
    if ok:
        return {"ok": True}
    raise HTTPException(503, _ERR_UNREACHABLE)


@router.post("/write")
async def drive_write(body: _DriveWriteBody):
    path = device_client.validate_sd_path(body.path)
    if len(body.content) > 65536:
        raise HTTPException(413, "Content too large (64 KB max)")
    base = device_client.device_base_url()
    if base:
        result = await device_client.wifi_post(
            f"{base}/api/fs/write",
            json={"path": path, "content": body.content},
            timeout=15.0,
        )
        if result is not None:
            return {"ok": True}
    ok = await serial_transport.fs_write(path, body.content)
    if ok:
        return {"ok": True}
    raise HTTPException(503, _ERR_UNREACHABLE)


@router.post("/convert")
async def drive_convert(request: Request):
    """Download a JPG/PNG from device, convert to LVGL .bin, upload back."""
    try:
        data = await request.json()
    except Exception:
        raise HTTPException(400, "Invalid JSON")

    path = device_client.validate_sd_path(data.get("path", ""))
    device_ip = data.get("device_ip") or settings_store.get("device_ip")

    try:
        from PIL import Image
        import io as _io
    except ImportError:
        raise HTTPException(503, "Pillow not installed — run: pip install pillow")

    stem     = _Path(path).stem
    dir_path = str(_Path(path).parent)
    if dir_path == ".":
        dir_path = "/"
    bin_name = f"{stem}.bin"

    # Step 1: Download source image
    img_bytes: bytes | None = None
    if device_ip:
        result = await device_client.wifi_get_bytes(
            f"http://{device_ip}/api/fs/download", params={"path": path}, timeout=60.0
        )
        if result is not None:
            img_bytes = result[0]

    if img_bytes is None:
        img_bytes = await serial_transport.fs_download(path)
        if img_bytes is None:
            raise HTTPException(502, "Could not download file from device (WiFi and serial failed).")

    # Step 2: Convert to LVGL .bin
    try:
        img = Image.open(_io.BytesIO(img_bytes)).convert("RGB")
        bin_data = media_manager._to_lvgl_bin(img, media_manager._DISPLAY_W, media_manager._DISPLAY_H)
    except Exception as e:
        raise HTTPException(500, f"Image conversion failed: {e}")

    # Step 3: Upload .bin back to device
    upload_ok = False
    if device_ip:
        result = await device_client.wifi_post(
            f"http://{device_ip}/api/fs/upload",
            files={"file": (bin_name, bin_data, "application/octet-stream")},
            params={"dir": dir_path},
            timeout=60.0,
        )
        upload_ok = result is not None

    if not upload_ok:
        dest = f"{dir_path}/{bin_name}" if dir_path != "/" else f"/{bin_name}"
        upload_ok = await serial_transport.fs_upload(dest, bin_data)

    if not upload_ok:
        raise HTTPException(502, "Could not upload .bin to device (WiFi and serial failed).")

    logging.info(f"[Convert] {path} → {dir_path}/{bin_name}")
    return {"ok": True, "bin_path": f"{dir_path}/{bin_name}"}
