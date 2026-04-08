# Project  : HelpDesk
# File     : main.py
# Purpose  : FastAPI app — routes, WebSocket hub, startup, and entry point
# Depends  : serial_comm, telemetry, macros, static/index.html

import asyncio
import logging
from pathlib import Path

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles
import uvicorn

import serial_comm
import telemetry
import macros

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
