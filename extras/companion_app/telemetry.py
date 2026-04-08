# Project  : HelpDesk
# File     : telemetry.py
# Purpose  : PC hardware telemetry — CPU and RAM usage gathering
# Depends  : psutil (optional; falls back to mock data if not installed)

import random
import logging

try:
    import psutil
    _HAS_PSUTIL = True
except ImportError:
    _HAS_PSUTIL = False
    logging.warning("psutil not found — sending mocked telemetry data.")


def get() -> dict:
    """Returns current CPU and RAM usage as a dict with keys 'cpu' and 'ram'."""
    if _HAS_PSUTIL:
        return {
            "cpu": psutil.cpu_percent(interval=None),
            "ram": psutil.virtual_memory().percent,
        }
    return {
        "cpu": round(random.uniform(10.0, 40.0), 1),
        "ram": round(random.uniform(40.0, 60.0), 1),
    }
