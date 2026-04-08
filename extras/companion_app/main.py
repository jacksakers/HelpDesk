# Project  : HelpDesk
# File     : main.py
# Purpose  : FastAPI app — routes, WebSocket hub, startup, and entry point
# Depends  : serial_comm, telemetry, macros, media_manager, settings_store, static/index.html

import asyncio
import logging
from pathlib import Path
from typing import Optional

from fastapi import FastAPI, WebSocket, WebSocketDisconnect, UploadFile, File, HTTPException
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel
import uvicorn

import serial_comm
import telemetry
import macros
import media_manager
import settings_store

# ── Input validation constants ───────────────────────────────────────────────
_VALID_IMAGE_EXTS = {".jpg", ".jpeg", ".png", ".bmp", ".gif"}
_VALID_AUDIO_EXTS = {".mp3"}
_MAX_IMAGE_BYTES  = 10 * 1024 * 1024   # 10 MB
_MAX_AUDIO_BYTES  = 50 * 1024 * 1024   # 50 MB


class _SettingsBody(BaseModel):
    wifi_ssid:     Optional[str] = None
    wifi_password: Optional[str] = None
    owm_api_key:   Optional[str] = None
    zip_code:      Optional[str] = None
    units:         Optional[str] = None
    device_ip:     Optional[str] = None

app = FastAPI(title="HelpDesk Companion App")
logging.basicConfig(level=logging.INFO)

_STATIC_DIR = Path(__file__).parent / "static"
app.mount("/static", StaticFiles(directory=str(_STATIC_DIR)), name="static")


# ── WebSocket connection manager ─────────────────────────────────────────────

class _ConnectionManager:
    """Tracks all active dashboard WebSocket clients and broadcasts to them."""

    def __init__(self):
        self._clients: set[WebSocket] = set()

    async def connect(self, ws: WebSocket) -> None:
        await ws.accept()
        self._clients.add(ws)

    def disconnect(self, ws: WebSocket) -> None:
        self._clients.discard(ws)

    async def broadcast(self, message: dict) -> None:
        for ws in list(self._clients):
            try:
                await ws.send_json(message)
            except Exception:
                self._clients.discard(ws)


_manager = _ConnectionManager()


# ── Routes ───────────────────────────────────────────────────────────────────

@app.get("/")
async def serve_dashboard():
    return FileResponse(_STATIC_DIR / "index.html")


@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    """Streams live telemetry and relays device events to the dashboard."""
    await _manager.connect(websocket)
    try:
        while True:
            data = telemetry.get()
            serial_comm.send(f'{{"c":{data["cpu"]},"r":{data["ram"]}}}\n')
            await _manager.broadcast({"type": "telemetry", **data})
            await asyncio.sleep(1)
    except WebSocketDisconnect:
        logging.info("Dashboard client disconnected.")
    finally:
        _manager.disconnect(websocket)


@app.post("/api/macros/{macro_id}/trigger")
async def trigger_macro(macro_id: str):
    """Triggered by the dashboard UI. Executes a macro and broadcasts the result."""
    logging.info(f"Dashboard triggered macro: {macro_id}")
    action = macros.execute(macro_id)
    serial_comm.send(f'{{"event":"macro_ok","id":"{macro_id}"}}\n')
    await _manager.broadcast({
        "type": "macro_event",
        "macro_id": macro_id,
        "action": action,
        "source": "dashboard",
    })
    return {"status": "success", "macro_id": macro_id, "message": action}


# ── Media routes ─────────────────────────────────────────────────────────────

@app.post("/api/media/image")
async def upload_image(file: UploadFile = File(...)):
    """Receives an image, resizes it to 480×320 BMP, saves locally, and attempts WiFi upload."""
    ext = Path(file.filename or "").suffix.lower()
    if ext not in _VALID_IMAGE_EXTS:
        raise HTTPException(400, f"Invalid image type. Allowed: {sorted(_VALID_IMAGE_EXTS)}")
    data = await file.read()
    if len(data) > _MAX_IMAGE_BYTES:
        raise HTTPException(413, "Image exceeds 10 MB limit")
    processed, out_name = media_manager.process_image(data, file.filename or "image")
    saved = media_manager.save_image(processed, out_name)
    device_ip = settings_store.get("device_ip", "")
    sent = await media_manager.upload_to_device(saved, f"/images/{out_name}", device_ip) if device_ip else False
    return {"status": "ok", "filename": out_name, "sent_to_device": sent}


@app.post("/api/media/audio")
async def upload_audio(file: UploadFile = File(...)):
    """Receives an MP3, saves locally, and attempts WiFi upload."""
    ext = Path(file.filename or "").suffix.lower()
    if ext not in _VALID_AUDIO_EXTS:
        raise HTTPException(400, "Only .mp3 files are supported")
    data = await file.read()
    if len(data) > _MAX_AUDIO_BYTES:
        raise HTTPException(413, "Audio exceeds 50 MB limit")
    safe_name = file.filename or "audio.mp3"
    saved = media_manager.save_audio(data, safe_name)
    device_ip = settings_store.get("device_ip", "")
    sent = await media_manager.upload_to_device(saved, f"/mp3/{saved.name}", device_ip) if device_ip else False
    return {"status": "ok", "filename": saved.name, "sent_to_device": sent}


# ── Settings routes ───────────────────────────────────────────────────────────

@app.get("/api/settings")
async def get_settings():
    """Returns the current device settings (read from device_settings.json)."""
    return settings_store.load()


@app.post("/api/settings")
async def update_settings(body: _SettingsBody):
    """Saves updated device settings locally and forwards them to the device if reachable."""
    updates = body.model_dump(exclude_none=True)
    settings_store.save(updates)

    # Forward to the ESP32 over Wi-Fi so it can update its own SD-card settings file.
    device_ip = settings_store.get("device_ip", "")
    forwarded = False
    if device_ip:
        try:
            import httpx
            async with httpx.AsyncClient(timeout=5.0) as client:
                resp = await client.post(
                    f"http://{device_ip}/settings",
                    json=updates,
                )
            forwarded = resp.status_code == 200
        except Exception as e:
            logging.warning(f"[Settings] Forward to device failed: {e}")

    return {"status": "saved", "forwarded_to_device": forwarded}


# ── Serial event routing ─────────────────────────────────────────────────────

async def _on_serial_event(data: dict) -> None:
    """Routes JSON events received from the ESP32 to the right handler."""
    event = data.get("event")
    if event == "btn_press":
        macro_id = data.get("id", "")
        logging.info(f"ESP32 macro trigger: {macro_id}")
        action = macros.execute(macro_id)
        serial_comm.send(f'{{"event":"macro_ok","id":"{macro_id}"}}\n')
        await _manager.broadcast({
            "type": "macro_event",
            "macro_id": macro_id,
            "action": action,
            "source": "device",
        })


async def _on_connect_change(connected: bool) -> None:
    """Notifies the dashboard when the HelpDesk serial connection changes."""
    await _manager.broadcast({"type": "serial_status", "connected": connected})


# ── Startup ──────────────────────────────────────────────────────────────────

@app.on_event("startup")
async def startup_event():
    asyncio.create_task(serial_comm.listener(_on_serial_event, _on_connect_change))


# ── Entry point ──────────────────────────────────────────────────────────────

if __name__ == "__main__":
    print("=======================================================")
    print(" Starting HelpDesk Companion App")
    print(" Dashboard will be available at: http://127.0.0.1:8000")
    print("=======================================================")
    uvicorn.run("main:app", host="127.0.0.1", port=8000, reload=True)
