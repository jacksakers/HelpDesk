# Project  : HelpDesk
# File     : serial_comm.py
# Purpose  : ESP32 serial port discovery, connection, send, and listener loop
# Depends  : pyserial (optional; silently disables if not installed)

import asyncio
import json
import logging
import time
from typing import Callable, Awaitable, Optional

try:
    import serial
    import serial.tools.list_ports
    _HAS_SERIAL = True
except ImportError:
    _HAS_SERIAL = False
    logging.warning("pyserial not found — serial communication disabled.")

_ESP_KEYWORDS = ["CP210", "CH34", "FT232", "USB to UART", "USB Serial", "JTAG"]

# ── Module state ──────────────────────────────────────────────────────────────
_port = None
_last_rx_time: float = 0.0        # epoch of last byte received from device
_next_reconnect_time: float = 0.0  # earliest epoch at which to attempt reconnect

_RECONNECT_INTERVAL_S = 3.0   # seconds between port-scan attempts
_HEARTBEAT_TIMEOUT_S  = 15.0  # seconds of silence before forcing disconnect


def _try_open(device: str, description: str) -> bool:
    global _port
    try:
        _port = serial.Serial(device, 115200, timeout=1)
        logging.info(f"[Serial] Connected on {device} ({description})")
        return True
    except serial.SerialException as e:
        logging.error(f"[Serial] Could not open {device}: {e}")
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


def disconnect() -> None:
    """Closes the serial port cleanly (call on app shutdown)."""
    global _port
    if _port is not None:
        try:
            _port.close()
        except Exception:
            pass
        _port = None
        logging.info("[Serial] Port closed.")


def send(payload: str) -> bool:
    """Sends a UTF-8 string to the ESP32. Returns True on success."""
    global _port
    if not is_connected():
        return False
    try:
        _port.write(payload.encode("utf-8"))
        return True
    except (serial.SerialException, OSError):
        logging.warning("[Serial] Write failed — device disconnected.")
        try:
            _port.close()
        except Exception:
            pass
        _port = None
        return False


async def timesync_loop() -> None:
    """
    Sends a Unix timestamp to the device every 5 minutes so it can maintain
    accurate time without waiting for an NTP sync.
    Format: {"event":"timesync","ts":<unix_epoch>}
    """
    while True:
        await asyncio.sleep(300)   # 5 minutes
        if is_connected():
            ts = int(time.time())
            send(f'{{"event":"timesync","ts":{ts}}}\n')
            logging.info(f"[TX] timesync ts={ts}")


async def listener(
    on_event: Callable[[dict], Awaitable[None]],
    on_connect_change: Optional[Callable[[bool], Awaitable[None]]] = None,
) -> None:
    """
    Background coroutine. Reads JSON lines from the ESP32 and dispatches them.
    Calls on_event(data) for each valid JSON message received.
    Calls on_event({"event":"raw_line","line":...}) for non-JSON device output.
    Calls on_connect_change(bool) whenever the connection state changes.

    Reconnect strategy:
      - When disconnected, scans ports at most once per _RECONNECT_INTERVAL_S.
      - When connected, if no data arrives for _HEARTBEAT_TIMEOUT_S the port is
        force-closed and the reconnect cooldown restarts.
    """
    global _port, _last_rx_time, _next_reconnect_time
    logging.info("[Serial] Listener started.")
    _was_connected = False

    while True:
        if _HAS_SERIAL:
            if not is_connected():
                if time.time() >= _next_reconnect_time:
                    auto_connect()
                    if is_connected():
                        # Seed heartbeat timer so we don't immediately timeout
                        _last_rx_time = time.time()
                    else:
                        _next_reconnect_time = time.time() + _RECONNECT_INTERVAL_S
            else:
                # Heartbeat: if device has gone silent, force a reconnect cycle
                if _last_rx_time > 0 and (time.time() - _last_rx_time) > _HEARTBEAT_TIMEOUT_S:
                    logging.warning(
                        f"[Serial] No data for {_HEARTBEAT_TIMEOUT_S:.0f}s — "
                        "forcing disconnect."
                    )
                    try:
                        _port.close()
                    except Exception:
                        pass
                    _port = None
                    _next_reconnect_time = time.time() + _RECONNECT_INTERVAL_S

            now_connected = is_connected()
            if now_connected != _was_connected:
                _was_connected = now_connected
                if on_connect_change:
                    await on_connect_change(now_connected)
                if now_connected:
                    ts = int(time.time())
                    send(f'{{"event":"host_hello","ts":{ts}}}\n')
                    logging.info("[TX] host_hello sent to device.")

            if now_connected and _port:
                try:
                    # Drain all pending bytes in one pass so we don't fall behind
                    while _port.in_waiting > 0:
                        line = _port.readline().decode("utf-8", errors="ignore").strip()
                        if line:
                            _last_rx_time = time.time()
                            logging.info(f"[RX] {line}")
                            try:
                                data = json.loads(line)
                                await on_event(data)
                            except json.JSONDecodeError:
                                await on_event({"event": "raw_line", "line": line})
                except (serial.SerialException, OSError):
                    logging.warning("[Serial] Read error — device unplugged.")
                    try:
                        _port.close()
                    except Exception:
                        pass
                    _port = None
                    _next_reconnect_time = time.time() + _RECONNECT_INTERVAL_S

        await asyncio.sleep(0.05)
