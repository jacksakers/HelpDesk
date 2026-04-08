# Project  : HelpDesk
# File     : serial_comm.py
# Purpose  : ESP32 serial port discovery, connection, send, and listener loop
# Depends  : pyserial (optional; silently disables if not installed)

import asyncio
import json
import logging
from typing import Callable, Awaitable, Optional

try:
    import serial
    import serial.tools.list_ports
    _HAS_SERIAL = True
except ImportError:
    _HAS_SERIAL = False
    logging.warning("pyserial not found — serial communication disabled.")

_ESP_KEYWORDS = ["CP210", "CH34", "FT232", "USB to UART", "USB Serial", "JTAG"]
_port = None


def _try_open(device: str, description: str) -> bool:
    global _port
    try:
        _port = serial.Serial(device, 115200, timeout=1)
        logging.info(f"Connected to ESP32 on {device} ({description})")
        return True
    except serial.SerialException as e:
        logging.error(f"Could not open {device}: {e}")
        return False


def auto_connect() -> None:
    """Scans COM ports and connects to the first recognised ESP32."""
    global _port
    if not _HAS_SERIAL:
        return
    if _port is not None and _port.is_open:
        return

    for port in serial.tools.list_ports.comports():
        if any(kw in port.description or kw in port.hwid for kw in _ESP_KEYWORDS):
            if _try_open(port.device, port.description):
                return

    # Fallback: first USB serial device
    for port in serial.tools.list_ports.comports():
        if "USB" in port.hwid:
            if _try_open(port.device, port.description):
                return


def is_connected() -> bool:
    """Returns True when an ESP32 serial port is open."""
    return _HAS_SERIAL and _port is not None and _port.is_open


def send(payload: str) -> bool:
    """Sends a UTF-8 string to the ESP32. Returns True on success."""
    global _port
    if not is_connected():
        return False
    try:
        _port.write(payload.encode("utf-8"))
        return True
    except serial.SerialException:
        logging.warning("Serial write failed — device disconnected.")
        _port.close()
        _port = None
        return False


async def listener(
    on_event: Callable[[dict], Awaitable[None]],
    on_connect_change: Optional[Callable[[bool], Awaitable[None]]] = None,
) -> None:
    """
    Background coroutine. Reads JSON lines from the ESP32 and dispatches them.
    Calls on_event(data) for each valid JSON message received.
    Calls on_connect_change(bool) whenever the connection state changes.
    """
    global _port
    logging.info("Serial listener started.")
    _was_connected = False

    while True:
        if _HAS_SERIAL:
            if not is_connected():
                auto_connect()

            now_connected = is_connected()
            if now_connected != _was_connected:
                _was_connected = now_connected
                if on_connect_change:
                    await on_connect_change(now_connected)

            if now_connected:
                try:
                    if _port.in_waiting > 0:
                        line = _port.readline().decode("utf-8", errors="ignore").strip()
                        if line:
                            try:
                                data = json.loads(line)
                                await on_event(data)
                            except json.JSONDecodeError:
                                logging.debug(f"Non-JSON from ESP32: {line}")
                except serial.SerialException:
                    logging.warning("Serial read error — device unplugged.")
                    _port.close()
                    _port = None

        await asyncio.sleep(0.05)
