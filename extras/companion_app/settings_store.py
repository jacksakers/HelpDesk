# Project  : HelpDesk
# File     : settings_store.py
# Purpose  : Persist device configuration to a local JSON file
# Depends  : device_settings.json (auto-created on first save)

import json
import logging
from pathlib import Path
from typing import Any

_SETTINGS_FILE = Path(__file__).parent / "device_settings.json"

# Canonical schema — unknown keys from file are silently dropped on load
_DEFAULTS: dict = {
    "wifi_ssid":     "",
    "wifi_password": "",
    "owm_api_key":   "",
    "zip_code":      "",
    "units":         "metric",
    "device_ip":     "",
}


def load() -> dict:
    """Returns current settings merged over defaults. Read-only callers should use this."""
    if _SETTINGS_FILE.exists():
        try:
            with _SETTINGS_FILE.open() as f:
                stored = json.load(f)
            # Only keep keys in the known schema; drop anything extra
            return {k: stored.get(k, v) for k, v in _DEFAULTS.items()}
        except (json.JSONDecodeError, OSError) as e:
            logging.warning(f"[Settings] Load error: {e} — using defaults.")
    return dict(_DEFAULTS)


def save(updates: dict) -> None:
    """Merges updates into stored settings and writes to disk. Unknown keys are ignored."""
    current = load()
    for key in _DEFAULTS:
        if key in updates:
            current[key] = str(updates[key]).strip()   # Validate at boundary: coerce to str, strip whitespace
    try:
        with _SETTINGS_FILE.open("w") as f:
            json.dump(current, f, indent=2)
        logging.info("[Settings] Saved to device_settings.json")
    except OSError as e:
        logging.error(f"[Settings] Save failed: {e}")


def get(key: str, default: Any = None) -> Any:
    """Returns a single setting by key, or default if not found."""
    return load().get(key, default)
