# Project  : HelpDesk
# File     : macros.py
# Purpose  : Macro engine — load definitions from macros.json and execute actions
# Depends  : macros.json (optional; built-in stubs used when key is missing)

import json
import logging
from pathlib import Path

_MACROS_FILE = Path(__file__).parent / "macros.json"

# Built-in stubs used when no matching macros.json entry exists
_BUILT_INS: dict = {
    "mute_mic":  "Mute microphone (not yet implemented)",
    "skip_song": "Skip to next song (not yet implemented)",
}


def _load() -> dict:
    """Reads macros.json from disk. Returns an empty dict on any error."""
    if _MACROS_FILE.exists():
        try:
            with _MACROS_FILE.open() as f:
                return json.load(f)
        except (json.JSONDecodeError, OSError) as e:
            logging.warning(f"macros.json load error: {e}")
    return {}


def execute(macro_id: str) -> str:
    """
    Executes the action for macro_id. Returns a human-readable result string.

    Macro types supported in macros.json:
      - hotkey: { "type": "hotkey", "keys": ["ctrl", "shift", "m"] }
      - launch:  { "type": "launch", "path": "C:/path/to/app.exe" }
      - text:    { "type": "text",   "text": "Hello, world!" }
    """
    macros = _load()

    if macro_id in macros:
        m = macros[macro_id]
        m_type = m.get("type", "")

        if m_type == "hotkey":
            # TODO: import pyautogui; pyautogui.hotkey(*m["keys"])
            keys = m.get("keys", [])
            logging.info(f"[macro] hotkey {keys}")
            return f"Hotkey: {'+'.join(keys)}"

        if m_type == "launch":
            # TODO: import subprocess; subprocess.Popen([m["path"]])
            path = m.get("path", "")
            logging.info(f"[macro] launch {path}")
            return f"Launch: {path}"

        if m_type == "text":
            # TODO: import pyautogui; pyautogui.typewrite(m["text"])
            text = m.get("text", "")
            logging.info(f"[macro] type text ({len(text)} chars)")
            return f"Type text: {text[:40]}"

        logging.warning(f"[macro] unknown type '{m_type}' for '{macro_id}'")
        return f"Unknown macro type: {m_type}"

    if macro_id in _BUILT_INS:
        logging.info(f"[macro] built-in stub: {macro_id}")
        return _BUILT_INS[macro_id]

    logging.warning(f"[macro] no definition for '{macro_id}'")
    return f"No definition for macro: {macro_id}"
