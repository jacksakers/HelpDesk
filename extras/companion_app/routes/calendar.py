# Project  : HelpDesk
# File     : routes/calendar.py
# Purpose  : Calendar proxy routes (/api/calendar/*) — WiFi then serial fallback
# Depends  : device_client, serial_transport

from fastapi import APIRouter, HTTPException
from pydantic import BaseModel

import device_client
import serial_transport

router = APIRouter(prefix="/api/calendar")

_ERR_UNREACHABLE = "Device unreachable (WiFi and serial both failed)."


class _CalendarAddBody(BaseModel):
    title:      str
    date:       str
    start_time: str  = ""
    end_time:   str  = ""
    all_day:    bool = False


class _CalendarDeleteBody(BaseModel):
    id: int


@router.get("")
async def get_calendar():
    base = device_client.device_base_url()
    if base:
        result = await device_client.wifi_get(f"{base}/calendar", timeout=5.0)
        if result is not None:
            return result
    result = await serial_transport.cal_list()
    if result is not None:
        return result
    raise HTTPException(503, _ERR_UNREACHABLE)


@router.post("/add")
async def add_calendar_event(body: _CalendarAddBody):
    if not body.title.strip():
        raise HTTPException(400, "title must not be empty")
    if not body.date:
        raise HTTPException(400, "date is required")
    payload = {
        "title":      body.title.strip(),
        "date":       body.date,
        "start_time": body.start_time,
        "end_time":   body.end_time,
        "all_day":    body.all_day,
    }
    base = device_client.device_base_url()
    if base:
        result = await device_client.wifi_post(f"{base}/calendar/add", json=payload, timeout=5.0)
        if result is not None:
            return result
    ok = await serial_transport.cal_add(
        payload["title"], payload["date"],
        payload["start_time"], payload["end_time"], payload["all_day"],
    )
    if ok:
        return {"ok": True}
    raise HTTPException(503, _ERR_UNREACHABLE)


@router.post("/delete")
async def delete_calendar_event(body: _CalendarDeleteBody):
    base = device_client.device_base_url()
    if base:
        result = await device_client.wifi_post(f"{base}/calendar/delete", json={"id": body.id})
        if result is not None:
            return result
    ok = await serial_transport.cal_delete(body.id)
    if ok:
        return {"ok": True}
    raise HTTPException(503, _ERR_UNREACHABLE)
