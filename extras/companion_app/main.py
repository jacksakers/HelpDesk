# Project  : HelpDesk
# File     : main.py
# Purpose  : FastAPI app — routes, WebSocket hub, startup, and entry point
# Depends  : serial_comm, telemetry, macros, media_manager, settings_store, static/index.html

import asyncio
import json
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

# Last known device_info payload — sent to new dashboard clients on connect
# so they see device state even if they opened after the hello was received.
_last_device_info: dict | None = None

# Track the active screen reported by the device so telemetry is only sent
# when the PC Monitor screen is visible (avoids flooding the serial link).
_current_screen: str = ""


# ── Routes ───────────────────────────────────────────────────────────────────

@app.get("/")
async def serve_dashboard():
    return FileResponse(_STATIC_DIR / "index.html")


@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    """Streams live telemetry to the dashboard client."""
    await _manager.connect(websocket)
    # Send cached device info immediately so a newly-opened dashboard
    # sees the device card without waiting for the next hello/status event.
    if _last_device_info:
        try:
            await websocket.send_json(_last_device_info)
        except Exception:
            pass
    try:
        while True:
            data = telemetry.get()
            await websocket.send_json({"type": "telemetry", **data})
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

    # Also push via serial — works even before WiFi is configured (e.g. first-time WiFi credentials)
    serial_payload = json.dumps({"event": "settings", **updates})
    serial_comm.send(serial_payload + "\n")

    return {"status": "saved", "forwarded_to_device": forwarded}


# ── Background telemetry task ────────────────────────────────────────────────

async def _telemetry_loop() -> None:
    """Pushes PC metrics to the HelpDesk every 5 seconds over serial.
    Only sends when the device is showing the PC Monitor screen to avoid
    flooding the serial link when the data isn't being displayed.
    """
    while True:
        await asyncio.sleep(5)
        if serial_comm.is_connected() and _current_screen == "pc_monitor":
            data = telemetry.get()
            serial_comm.send(f'{{"c":{data["cpu"]},"r":{data["ram"]}}}\n')


# ── Serial event routing ─────────────────────────────────────────────────────

async def _on_serial_event(data: dict) -> None:
    """Routes JSON events received from the ESP32 to the right handler."""
    global _last_device_info, _current_screen
    event = data.get("event")

    if event == "raw_line":
        # Non-JSON device output (boot messages, debug prints, etc.).
        # Already logged to terminal by serial_comm; also show in dashboard log.
        line = data.get("line", "")
        await _manager.broadcast({"type": "serial_log", "label": line})
        return

    if event == "hello":
        ip   = data.get("ip", "")
        ssid = data.get("ssid", "")
        fw   = data.get("fw", "")
        sd   = "ok" if data.get("sd_ok") else "missing"
        # If the device hasn't got a WiFi address yet, fall back to the
        # manually-saved IP so the dashboard card shows something useful.
        if not ip or ip == "0.0.0.0":
            ip = settings_store.get("device_ip", "") or ip
        logging.info(f"[RX] device hello — ip={ip}  ssid={ssid}  fw={fw}  sd={sd}")
        if ip and ip != "0.0.0.0" and not settings_store.get("device_ip"):
            settings_store.save({"device_ip": ip})
            logging.info(f"[Handshake] Auto-saved device IP: {ip}")
        _last_device_info = {
            "type":         "device_info",
            "ip":           ip,
            "ssid":         ssid,
            "fw":           fw,
            "sd_ok":        data.get("sd_ok", False),
            "sd_total_mb":  data.get("sd_total_mb", 0),
            "sd_used_mb":   data.get("sd_used_mb", 0),
        }
        await _manager.broadcast(_last_device_info)
        await _manager.broadcast({"type": "serial_log", "label": f"hello — ip:{ip}  fw:{fw}  sd:{sd}"})
        return

    if event == "status":
        screen  = data.get("screen", "")
        sd_used = data.get("sd_used_mb", 0)
        _current_screen = screen
        logging.info(f"[RX] device status — screen={screen}  sd_used={sd_used} MB")
        await _manager.broadcast({
            "type":       "device_status",
            "screen":     screen,
            "sd_used_mb": sd_used,
        })
        await _manager.broadcast({"type": "serial_log", "label": f"status — screen:{screen}  sd:{sd_used} MB"})
        return

    if event == "btn_press":
        macro_id = data.get("id", "")
        logging.info(f"[RX] device btn_press — id={macro_id}")
        action = macros.execute(macro_id)
        serial_comm.send(f'{{"event":"macro_ok","id":"{macro_id}"}}\n')
        logging.info(f"[TX] macro_ok  id={macro_id}")
        await _manager.broadcast({
            "type": "macro_event",
            "macro_id": macro_id,
            "action": action,
            "source": "device",
        })


async def _on_connect_change(connected: bool) -> None:
    """Notifies the dashboard when the HelpDesk serial connection changes."""
    status = "connected" if connected else "disconnected"
    logging.info(f"[Serial] HelpDesk {status}.")
    await _manager.broadcast({"type": "serial_status", "connected": connected})
    await _manager.broadcast({"type": "serial_log", "label": f"HelpDesk {status}"})


# ── Startup ──────────────────────────────────────────────────────────────────

@app.on_event("startup")
async def startup_event():
    asyncio.create_task(serial_comm.listener(_on_serial_event, _on_connect_change))
    asyncio.create_task(serial_comm.timesync_loop())
    asyncio.create_task(_telemetry_loop())


@app.on_event("shutdown")
async def shutdown_event():
    serial_comm.disconnect()


# ── Entry point ──────────────────────────────────────────────────────────────

if __name__ == "__main__":
    print("=======================================================")
    print(" Starting HelpDesk Companion App")
    print(" Dashboard will be available at: http://127.0.0.1:8000")
    print("=======================================================")
    uvicorn.run("main:app", host="127.0.0.1", port=8000, reload=True)
