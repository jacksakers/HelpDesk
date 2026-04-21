# Project  : HelpDesk
# File     : main.py
# Purpose  : FastAPI app — app setup, WebSocket hub, serial event routing, startup
# Depends  : serial_comm, telemetry, macros, notifications, state, routes/*

import asyncio
import logging
from pathlib import Path

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles
import uvicorn

import macros
import notifications
import serial_comm
import serial_transport
import settings_store
import state
import telemetry
from routes import calendar, drive, media, settings, tasks, voice

app = FastAPI(title="HelpDesk Companion App")
logging.basicConfig(level=logging.INFO)

_STATIC_DIR = Path(__file__).parent / "static"
app.mount("/static", StaticFiles(directory=str(_STATIC_DIR)), name="static")

# ── Register route modules ────────────────────────────────────────────────────
app.include_router(tasks.router)
app.include_router(calendar.router)
app.include_router(drive.router)
app.include_router(media.router)
app.include_router(settings.router)
app.include_router(voice.router)


# ── Dashboard ─────────────────────────────────────────────────────────────────

@app.get("/")
async def serve_dashboard():
    return FileResponse(_STATIC_DIR / "index.html")


@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    """Streams live telemetry to the dashboard client."""
    await state.manager.connect(websocket)
    # Send cached device info immediately so a newly-opened dashboard
    # sees the device card without waiting for the next hello/status event.
    if state.last_device_info:
        try:
            await websocket.send_json(state.last_device_info)
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
        state.manager.disconnect(websocket)


@app.post("/api/macros/{macro_id}/trigger")
async def trigger_macro(macro_id: str):
    """Triggered by the dashboard UI. Executes a macro and broadcasts the result."""
    logging.info(f"Dashboard triggered macro: {macro_id}")
    action = macros.execute(macro_id)
    serial_comm.send(f'{{"event":"macro_ok","id":"{macro_id}"}}\n')
    await state.manager.broadcast({
        "type": "macro_event",
        "macro_id": macro_id,
        "action": action,
        "source": "dashboard",
    })
    return {"status": "success", "macro_id": macro_id, "message": action}


# ── Background telemetry task ─────────────────────────────────────────────────

async def _telemetry_loop() -> None:
    """Pushes PC metrics to the HelpDesk every 5 seconds over serial.
    Only sends when the device is showing the PC Monitor screen to avoid
    flooding the serial link when the data isn't being displayed.
    """
    while True:
        await asyncio.sleep(5)
        if serial_comm.is_connected() and state.current_screen == "pc_monitor":
            data = telemetry.get()
            serial_comm.send(f'{{"c":{data["cpu"]},"r":{data["ram"]}}}\n')


# ── Serial event routing ──────────────────────────────────────────────────────

async def _on_serial_event(data: dict) -> None:
    """Routes JSON events received from the ESP32 to the right handler."""
    event = data.get("event")

    if event == "resp":
        serial_transport.dispatch_response(data)
        return
    if event == "chunk":
        serial_transport.dispatch_chunk(data)
        return

    if event == "raw_line":
        await state.manager.broadcast({"type": "serial_log", "label": data.get("line", "")})
        return

    if event == "hello":
        ip   = data.get("ip", "")
        ssid = data.get("ssid", "")
        fw   = data.get("fw", "")
        sd   = "ok" if data.get("sd_ok") else "missing"
        # Fall back to manually-saved IP if device hasn't got a WiFi address yet.
        if not ip or ip == "0.0.0.0":
            ip = settings_store.get("device_ip", "") or ip
        logging.info(f"[RX] device hello — ip={ip}  ssid={ssid}  fw={fw}  sd={sd}")
        if ip and ip != "0.0.0.0" and not settings_store.get("device_ip"):
            settings_store.save({"device_ip": ip})
            logging.info(f"[Handshake] Auto-saved device IP: {ip}")
        state.last_device_info = {
            "type":        "device_info",
            "ip":          ip,
            "ssid":        ssid,
            "fw":          fw,
            "sd_ok":       data.get("sd_ok", False),
            "sd_total_mb": data.get("sd_total_mb", 0),
            "sd_used_mb":  data.get("sd_used_mb", 0),
        }
        await state.manager.broadcast(state.last_device_info)
        await state.manager.broadcast({"type": "serial_log", "label": f"hello — ip:{ip}  fw:{fw}  sd:{sd}"})
        return

    if event == "status":
        screen  = data.get("screen", "")
        sd_used = data.get("sd_used_mb", 0)
        state.current_screen = screen
        await state.manager.broadcast({"type": "device_status", "screen": screen, "sd_used_mb": sd_used})
        await state.manager.broadcast({"type": "serial_log", "label": f"status — screen:{screen}  sd:{sd_used} MB"})
        return

    if event == "btn_press":
        macro_id = data.get("id", "")
        logging.info(f"[RX] device btn_press — id={macro_id}")
        action = macros.execute(macro_id)
        serial_comm.send(f'{{"event":"macro_ok","id":"{macro_id}"}}\n')
        logging.info(f"[TX] macro_ok  id={macro_id}")
        await state.manager.broadcast({
            "type":     "macro_event",
            "macro_id": macro_id,
            "action":   action,
            "source":   "device",
        })


async def _on_notification_received(app_name: str, title: str, body: str) -> None:
    await state.manager.broadcast({"type": "notification", "app": app_name, "title": title, "body": body})


async def _on_connect_change(connected: bool) -> None:
    status = "connected" if connected else "disconnected"
    logging.info(f"[Serial] HelpDesk {status}.")
    await state.manager.broadcast({"type": "serial_status", "connected": connected})
    await state.manager.broadcast({"type": "serial_log", "label": f"HelpDesk {status}"})


# ── Startup / shutdown ────────────────────────────────────────────────────────

@app.on_event("startup")
async def startup_event():
    asyncio.create_task(serial_comm.listener(_on_serial_event, _on_connect_change))
    asyncio.create_task(serial_comm.timesync_loop())
    asyncio.create_task(_telemetry_loop())
    notifications.set_broadcast_callback(_on_notification_received)
    await notifications.start()


@app.on_event("shutdown")
async def shutdown_event():
    notifications.stop()
    serial_comm.disconnect()


# ── Entry point ───────────────────────────────────────────────────────────────

if __name__ == "__main__":
    print("========================================================")
    print(" Starting HelpDesk Companion App")
    print(" Dashboard will be available at: http://127.0.0.1:8000")
    print(" LAN access: http://<your-pc-ip>:8000")
    print("========================================================")
    uvicorn.run("main:app", host="0.0.0.0", port=8000, reload=True)
