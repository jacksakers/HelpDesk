# Project  : HelpDesk
# File     : state.py
# Purpose  : Shared runtime state — WebSocket manager and device state cache
# Depends  : fastapi

from fastapi import WebSocket


class ConnectionManager:
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


manager = ConnectionManager()

# Last known device_info payload — sent to new dashboard clients on connect
# so they see device state even if they opened after the hello was received.
last_device_info: dict | None = None

# Track the active screen reported by the device so telemetry is only sent
# when the PC Monitor screen is visible (avoids flooding the serial link).
current_screen: str = ""
