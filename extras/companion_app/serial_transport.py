# Project  : HelpDesk
# File     : serial_transport.py
# Purpose  : Async request/response layer over serial — correlates cmds to device responses
# Depends  : serial_comm
#
# Protocol (all messages are newline-terminated JSON):
#   Companion → Device : {"cmd":"<op>","rid":<int>,...params}
#   Device → Companion : {"event":"resp","rid":<int>,"ok":<bool>[,"items"|"data"|"settings":...]}
#   Device → Companion : {"event":"chunk","rid":<int>,"seq":<int>,"total":<int>,"data":"<b64>"}
#
# Each request is assigned a unique "rid" (request ID). dispatch_response() and
# dispatch_chunk() are called by main.py's _on_serial_event to resolve the right Future.

import asyncio
import base64
import json
import logging
from typing import Any

import serial_comm

# ── Constants ─────────────────────────────────────────────────────────────────
_DEFAULT_TIMEOUT_S = 10.0    # default response timeout
_DOWNLOAD_TIMEOUT  = 90.0    # longer timeout for file downloads
_UPLOAD_TIMEOUT    = 180.0   # longer timeout for file uploads
_CHUNK_RAW_SZ      = 192     # raw bytes per upload chunk (→ 256 bytes base64)

# ── Module state ──────────────────────────────────────────────────────────────
_pending:     dict[int, asyncio.Future] = {}   # rid → Future waiting for resp
_chunk_bufs:  dict[int, dict]           = {}   # rid → {"total": N, "chunks": {seq: bytes}}
_bulk_lock:   asyncio.Lock | None = None        # serialises multi-message ops
_rid_counter: int = 0


def _get_bulk_lock() -> asyncio.Lock:
    """Lazy-init the bulk lock inside the running event loop."""
    global _bulk_lock
    if _bulk_lock is None:
        _bulk_lock = asyncio.Lock()
    return _bulk_lock


def _next_rid() -> int:
    global _rid_counter
    _rid_counter = (_rid_counter + 1) % 10000
    return _rid_counter


# ── Incoming event dispatch (called from main.py _on_serial_event) ─────────────

def dispatch_response(data: dict) -> bool:
    """Called when event=="resp" arrives from device. Returns True if handled."""
    rid = data.get("rid")
    if rid is None:
        return False
    fut = _pending.get(rid)
    if fut and not fut.done():
        fut.set_result(data)
    return True


def dispatch_chunk(data: dict) -> bool:
    """Called when event=="chunk" arrives from device (file download chunk)."""
    rid   = data.get("rid")
    seq   = data.get("seq", 0)
    total = data.get("total", 1)
    b64   = data.get("data", "")
    if rid is None:
        return False

    try:
        raw = base64.b64decode(b64) if b64 else b""
    except Exception:
        logging.warning(f"[SerialTransport] Bad base64 in chunk rid={rid} seq={seq}")
        return False

    buf = _chunk_bufs.setdefault(rid, {"total": total, "chunks": {}})
    buf["chunks"][seq] = raw

    # Resolve future once all chunks have arrived.
    if len(buf["chunks"]) >= buf["total"]:
        try:
            combined = b"".join(buf["chunks"][i] for i in range(buf["total"]))
        except KeyError:
            return True   # a seq is still missing — wait for more
        del _chunk_bufs[rid]
        fut = _pending.get(rid)
        if fut and not fut.done():
            fut.set_result({"ok": True, "data": combined})
    return True


# ── Core request helper ────────────────────────────────────────────────────────

async def _request(cmd: dict, timeout: float = _DEFAULT_TIMEOUT_S) -> dict | None:
    """Sends one serial command and awaits the matching response (by rid)."""
    if not serial_comm.is_connected():
        return None

    rid = _next_rid()
    payload = dict(cmd)
    payload["rid"] = rid

    loop = asyncio.get_running_loop()
    fut: asyncio.Future = loop.create_future()
    _pending[rid] = fut
    try:
        serial_comm.send(json.dumps(payload, separators=(",", ":")) + "\n")
        return await asyncio.wait_for(fut, timeout=timeout)
    except asyncio.TimeoutError:
        logging.warning(f"[SerialTransport] Timeout rid={rid} cmd={cmd.get('cmd')}")
        return None
    finally:
        _pending.pop(rid, None)


# ── FS operations ──────────────────────────────────────────────────────────────

async def fs_list(dir_path: str) -> list | None:
    """Returns a list of file/folder dicts, or None on failure."""
    resp = await _request({"cmd": "fs_list", "dir": dir_path})
    if resp and resp.get("ok"):
        return resp.get("items", [])
    return None


async def fs_write(path: str, content: str) -> bool:
    resp = await _request({"cmd": "fs_write", "path": path, "content": content}, timeout=20.0)
    return bool(resp and resp.get("ok"))


async def fs_mkdir(path: str) -> bool:
    resp = await _request({"cmd": "fs_mkdir", "path": path})
    return bool(resp and resp.get("ok"))


async def fs_delete(path: str) -> bool:
    resp = await _request({"cmd": "fs_delete", "path": path})
    return bool(resp and resp.get("ok"))


async def fs_download(path: str) -> bytes | None:
    """Downloads a file from the device over serial (chunked base64). Returns bytes or None."""
    if not serial_comm.is_connected():
        return None

    async with _get_bulk_lock():
        rid = _next_rid()
        loop = asyncio.get_running_loop()
        fut: asyncio.Future = loop.create_future()
        _pending[rid] = fut
        try:
            serial_comm.send(
                json.dumps({"cmd": "fs_download", "rid": rid, "path": path}, separators=(",", ":")) + "\n"
            )
            result = await asyncio.wait_for(fut, timeout=_DOWNLOAD_TIMEOUT)
            if result and result.get("ok"):
                return result.get("data", b"")
            return None
        except asyncio.TimeoutError:
            logging.warning(f"[SerialTransport] Download timeout for {path}")
            return None
        finally:
            _pending.pop(rid, None)
            _chunk_bufs.pop(rid, None)


async def fs_upload(path: str, data: bytes) -> bool:
    """Uploads a file to the device over serial (chunked base64). Returns True on success."""
    if not serial_comm.is_connected():
        return False

    async with _get_bulk_lock():
        chunks = [data[i : i + _CHUNK_RAW_SZ] for i in range(0, len(data), _CHUNK_RAW_SZ)]
        if not chunks:
            chunks = [b""]
        total = len(chunks)
        rid = _next_rid()

        loop = asyncio.get_running_loop()
        fut: asyncio.Future = loop.create_future()
        _pending[rid] = fut
        try:
            for seq, chunk in enumerate(chunks):
                encoded = base64.b64encode(chunk).decode("ascii")
                serial_comm.send(
                    json.dumps(
                        {
                            "cmd":   "upload_chunk",
                            "rid":   rid,
                            "path":  path,
                            "seq":   seq,
                            "total": total,
                            "data":  encoded,
                        },
                        separators=(",", ":"),
                    )
                    + "\n"
                )
                # Yield so the event loop can process incoming ack/chunk events.
                await asyncio.sleep(0.02)

            result = await asyncio.wait_for(fut, timeout=_UPLOAD_TIMEOUT)
            return bool(result and result.get("ok"))
        except asyncio.TimeoutError:
            logging.warning(f"[SerialTransport] Upload timeout for {path}")
            return False
        finally:
            _pending.pop(rid, None)


# ── Settings ───────────────────────────────────────────────────────────────────

async def get_settings() -> dict | None:
    """Fetches current device settings over serial. Returns a dict or None."""
    resp = await _request({"cmd": "get_settings"})
    if resp and resp.get("ok"):
        return resp.get("settings")
    return None


# ── Tasks ──────────────────────────────────────────────────────────────────────

async def task_list() -> dict | None:
    """Returns the full tasks payload (tasks + XP stats) or None."""
    resp = await _request({"cmd": "task_list"}, timeout=8.0)
    if resp and resp.get("ok"):
        return resp.get("data")
    return None


async def task_add(text: str, repeat: bool, due_date: str | None) -> bool:
    resp = await _request(
        {"cmd": "task_add", "text": text, "repeat": repeat, "due_date": due_date or ""}
    )
    return bool(resp and resp.get("ok"))


async def task_complete(task_id: int) -> bool:
    resp = await _request({"cmd": "task_complete", "id": task_id})
    return bool(resp and resp.get("ok"))


async def task_delete(task_id: int) -> bool:
    resp = await _request({"cmd": "task_delete", "id": task_id})
    return bool(resp and resp.get("ok"))


# ── Calendar ───────────────────────────────────────────────────────────────────

async def cal_list() -> list | None:
    """Returns a list of calendar event dicts, or None."""
    resp = await _request({"cmd": "cal_list"}, timeout=8.0)
    if resp and resp.get("ok"):
        return resp.get("items")
    return None


async def cal_add(
    title: str, date: str, start_time: str, end_time: str, all_day: bool
) -> bool:
    resp = await _request(
        {
            "cmd":        "cal_add",
            "title":      title,
            "date":       date,
            "start_time": start_time,
            "end_time":   end_time,
            "all_day":    all_day,
        }
    )
    return bool(resp and resp.get("ok"))


async def cal_delete(event_id: int) -> bool:
    resp = await _request({"cmd": "cal_delete", "id": event_id})
    return bool(resp and resp.get("ok"))
