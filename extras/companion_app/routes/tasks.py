# Project  : HelpDesk
# File     : routes/tasks.py
# Purpose  : Task-list proxy routes (/api/tasks/*) — WiFi then serial fallback
# Depends  : device_client, serial_transport

from typing import Optional

from fastapi import APIRouter, HTTPException
from pydantic import BaseModel

import device_client
import serial_transport

router = APIRouter(prefix="/api/tasks")

_ERR_UNREACHABLE = "Device unreachable (WiFi and serial both failed)."


class _TaskAddBody(BaseModel):
    text:     str
    repeat:   bool = False
    due_date: Optional[str] = None


@router.get("")
async def get_tasks():
    base = device_client.device_base_url()
    if base:
        result = await device_client.wifi_get(f"{base}/tasks", timeout=5.0)
        if result is not None:
            return result
    result = await serial_transport.task_list()
    if result is not None:
        return result
    raise HTTPException(503, _ERR_UNREACHABLE)


@router.post("/add")
async def add_task(body: _TaskAddBody):
    if not body.text.strip():
        raise HTTPException(400, "text must not be empty")
    base = device_client.device_base_url()
    if base:
        payload: dict = {"text": body.text.strip(), "repeat": body.repeat}
        if body.due_date:
            payload["due_date"] = body.due_date
        result = await device_client.wifi_post(f"{base}/tasks/add", json=payload, timeout=5.0)
        if result is not None:
            return result
    ok = await serial_transport.task_add(body.text.strip(), body.repeat, body.due_date)
    if ok:
        return {"ok": True}
    raise HTTPException(503, _ERR_UNREACHABLE)


@router.post("/complete")
async def complete_task(task_id: int):
    base = device_client.device_base_url()
    if base:
        result = await device_client.wifi_post(f"{base}/tasks/complete", json={"id": task_id})
        if result is not None:
            return result
    ok = await serial_transport.task_complete(task_id)
    if ok:
        return {"ok": True}
    raise HTTPException(503, _ERR_UNREACHABLE)


@router.post("/delete")
async def delete_task(task_id: int):
    base = device_client.device_base_url()
    if base:
        result = await device_client.wifi_post(f"{base}/tasks/delete", json={"id": task_id})
        if result is not None:
            return result
    ok = await serial_transport.task_delete(task_id)
    if ok:
        return {"ok": True}
    raise HTTPException(503, _ERR_UNREACHABLE)
