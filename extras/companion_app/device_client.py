# Project  : HelpDesk
# File     : device_client.py
# Purpose  : Async HTTP helpers for WiFi communication with the device + path validation
# Depends  : httpx, settings_store

import logging
import posixpath

import httpx
from fastapi import HTTPException

import settings_store


def device_base_url() -> str | None:
    """Returns 'http://<device_ip>' if a device IP is configured, otherwise None."""
    ip = settings_store.get("device_ip", "")
    return f"http://{ip}" if ip else None


def validate_sd_path(path: str) -> str:
    """Normalise and validate an SD card path. Rejects path traversal attempts."""
    clean = posixpath.normpath("/" + str(path).replace("\\", "/").lstrip("/"))
    if any(part == ".." for part in clean.split("/")):
        raise HTTPException(400, "Invalid path")
    return clean


async def wifi_get(url: str, *, params=None, timeout: float = 5.0) -> dict | None:
    """GET url and return parsed JSON on HTTP 200, otherwise None."""
    try:
        async with httpx.AsyncClient(timeout=timeout) as client:
            resp = await client.get(url, params=params)
        if resp.status_code == 200:
            try:
                return resp.json()
            except Exception:
                return {}
    except Exception as e:
        logging.debug(f"[WiFi] GET {url} failed: {e}")
    return None


async def wifi_get_bytes(url: str, *, params=None, timeout: float = 30.0) -> tuple[bytes, str] | None:
    """GET url and return (content_bytes, content_type) on HTTP 200, otherwise None."""
    try:
        async with httpx.AsyncClient(timeout=timeout) as client:
            resp = await client.get(url, params=params)
        if resp.status_code == 200:
            return resp.content, resp.headers.get("Content-Type", "application/octet-stream")
    except Exception as e:
        logging.debug(f"[WiFi] GET (bytes) {url} failed: {e}")
    return None


async def wifi_post(
    url: str,
    *,
    json=None,
    files=None,
    params=None,
    timeout: float = 5.0,
) -> dict | None:
    """POST to url and return parsed JSON on HTTP 200, otherwise None."""
    try:
        async with httpx.AsyncClient(timeout=timeout) as client:
            resp = await client.post(url, json=json, files=files, params=params)
        if resp.status_code == 200:
            try:
                return resp.json()
            except Exception:
                return {}  # success but non-JSON body (e.g. plain 200 OK)
    except Exception as e:
        logging.debug(f"[WiFi] POST {url} failed: {e}")
    return None
