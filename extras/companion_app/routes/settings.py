# Project  : HelpDesk
# File     : routes/settings.py
# Purpose  : Settings routes (/api/settings) — read/write with WiFi + serial fallback
# Depends  : device_client, serial_transport, serial_comm, settings_store

import json
import logging
from typing import Optional

from fastapi import APIRouter
from pydantic import BaseModel

import device_client
import serial_comm
import serial_transport
import settings_store

router = APIRouter(prefix="/api")


class _SettingsBody(BaseModel):
    wifi_ssid:     Optional[str] = None
    wifi_password: Optional[str] = None
    owm_api_key:   Optional[str] = None
    zip_code:      Optional[str] = None
    units:         Optional[str] = None
    device_ip:     Optional[str] = None
    companion_ip:  Optional[str] = None


@router.get("/settings")
async def get_settings():
    """Returns current settings. Priority: WiFi → serial → local cache."""
    local = settings_store.load()
    device_ip = local.get("device_ip", "")

    if device_ip:
        result = await device_client.wifi_get(f"http://{device_ip}/settings", timeout=3.0)
        if result is not None:
            result["device_ip"] = device_ip
            return result

    serial_settings = await serial_transport.get_settings()
    if serial_settings:
        serial_settings["device_ip"] = device_ip
        logging.info("[Settings] Fetched from device via serial.")
        return serial_settings

    return local


@router.post("/settings")
async def update_settings(body: _SettingsBody):
    """Saves settings locally and forwards to the device over WiFi and serial."""
    updates = body.model_dump(exclude_none=True)
    settings_store.save(updates)

    device_ip = settings_store.get("device_ip", "")
    forwarded = False
    if device_ip:
        result = await device_client.wifi_post(f"http://{device_ip}/settings", json=updates, timeout=5.0)
        forwarded = result is not None

    # Also push via serial — works even before WiFi is configured
    serial_comm.send(json.dumps({"event": "settings", **updates}) + "\n")
    return {"status": "saved", "forwarded_to_device": forwarded}
