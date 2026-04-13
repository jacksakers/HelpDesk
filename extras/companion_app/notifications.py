# Project  : HelpDesk
# File     : notifications.py
# Purpose  : Windows notification listener — filters and forwards to ESP32 via serial
# Depends  : winsdk (pip install winsdk), serial_comm
#
# Implementation note: winsdk's add_notification_changed event subscription fails
# with "Element not found" for plain Python desktop processes. We use a polling
# approach instead — get_notifications_async() is called every POLL_INTERVAL_S
# seconds and new IDs are detected by diffing against a seen-ID set.

import asyncio
import json
import logging
from typing import Callable, Awaitable

import serial_comm

# ── Configure which apps to forward ──────────────────────────────────────────
# App display names as they appear in Windows Action Center.
# Chrome / Edge host web-app notifications under the browser name.
ALLOWED_APPS: set[str] = {
    "Google Chrome",
    "Microsoft Edge",
    "Slack",
    "Discord",
    "Microsoft Teams",
    "Teams",
    "Telegram",
    "WhatsApp",
}

# Body text truncated to this length to avoid ESP32 serial buffer overruns.
_BODY_MAX_LEN   = 50
# How often (seconds) to check the Action Center for new notifications.
_POLL_INTERVAL_S = 2

# Callback type: async fn(app, title, body) → None
_NotifCallback = Callable[[str, str, str], Awaitable[None]]

_broadcast_cb: _NotifCallback | None = None


def set_broadcast_callback(cb: _NotifCallback) -> None:
    """Register a coroutine called whenever a notification is dispatched."""
    global _broadcast_cb
    _broadcast_cb = cb


# ── winsdk imports (optional — silently disables on non-Windows) ──────────────
try:
    from winsdk.windows.ui.notifications.management import UserNotificationListener
    from winsdk.windows.ui.notifications import (
        KnownNotificationBindings,
    )
    _HAS_WINSDK = True
except ImportError:
    _HAS_WINSDK = False
    logging.warning(
        "[Notifications] winsdk not found or not on Windows — "
        "notification listener disabled. Install with: pip install winsdk"
    )

# ── Private state ─────────────────────────────────────────────────────────────
_listener: "UserNotificationListener | None" = None
_seen_ids: set[int] = set()
_running: bool = False


def _extract_from_notif(notif) -> tuple[str, str, str] | None:
    """
    Pulls app name, title, and body from an already-fetched UserNotification object.
    Returns None if the app is not in the allow-list or content can't be read.
    """
    try:
        app_name: str = notif.app_info.display_info.display_name
    except Exception:
        return None

    if app_name not in ALLOWED_APPS:
        return None

    try:
        binding = notif.notification.visual.get_binding(
            KnownNotificationBindings.toast_generic
        )
        text_elements = list(binding.get_text_elements())
    except Exception:
        return None

    title = text_elements[0].text if len(text_elements) > 0 else "Notification"
    body  = text_elements[1].text if len(text_elements) > 1 else ""

    if len(body) > _BODY_MAX_LEN:
        body = body[:_BODY_MAX_LEN - 1] + "…"

    return app_name, title, body


async def _dispatch(app_name: str, title: str, body: str) -> None:
    """Sends the notification over serial and optionally to the dashboard."""
    payload = json.dumps({
        "cmd":   "notif",
        "app":   app_name,
        "title": title,
        "body":  body,
    })
    serial_comm.send(payload + "\n")
    logging.info(f"[Notifications] Forwarded: {app_name} | {title}")

    if _broadcast_cb is not None:
        try:
            await _broadcast_cb(app_name, title, body)
        except Exception as exc:
            logging.error(f"[Notifications] broadcast callback error: {exc}")


async def _poll_loop() -> None:
    """
    Polls the Action Center every POLL_INTERVAL_S seconds.
    Detects new notifications by comparing current IDs against _seen_ids.
    Dismissed notifications are pruned from the set so memory stays bounded.
    """
    global _seen_ids

    while _running and _listener is not None:
        await asyncio.sleep(_POLL_INTERVAL_S)
        try:
            notifs = await _listener.get_notifications_async(1)  # 1 = NotificationKinds.Toast
            current_ids: set[int] = set()

            for notif in notifs:
                nid = notif.id
                current_ids.add(nid)

                if nid not in _seen_ids:
                    _seen_ids.add(nid)
                    result = _extract_from_notif(notif)
                    if result is not None:
                        await _dispatch(*result)

            # Prune IDs for notifications that have since been dismissed
            _seen_ids &= current_ids

        except Exception as exc:
            logging.error(f"[Notifications] Poll error: {exc}")


async def start() -> None:
    """
    Requests Windows notification access and starts the polling loop.
    Safe to call even if winsdk is unavailable — becomes a no-op.
    """
    global _listener, _running, _seen_ids

    if not _HAS_WINSDK:
        return

    try:
        _listener = UserNotificationListener.current

        # Request OS permission to read notifications (status 1 = allowed)
        access_status = await _listener.request_access_async()
        logging.info(f"[Notifications] Access status: {access_status}")

        if int(access_status) != 1:
            logging.warning(
                "[Notifications] Access not granted — check Windows Settings "
                "→ Privacy & Security → Notifications."
            )
            return

        _seen_ids.clear()
        _running = True

        # Seed seen_ids with whatever is already in the Action Center so we
        # don't re-fire old notifications on every app restart.
        try:
            existing = await _listener.get_notifications_async(1)  # 1 = NotificationKinds.Toast
            _seen_ids = {n.id for n in existing}
        except Exception:
            pass

        asyncio.create_task(_poll_loop())
        logging.info(
            "[Notifications] Polling active (every %ds). "
            "Watching: %s", _POLL_INTERVAL_S, ", ".join(sorted(ALLOWED_APPS))
        )
    except Exception as exc:
        logging.error(f"[Notifications] Failed to start listener: {exc}")


def stop() -> None:
    """Signals the polling loop to exit on its next iteration."""
    global _running, _listener
    _running  = False
    _listener = None
    logging.info("[Notifications] Listener stopped.")

