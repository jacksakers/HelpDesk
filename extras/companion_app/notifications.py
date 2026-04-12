# Project  : HelpDesk
# File     : notifications.py
# Purpose  : Windows notification listener — filters and forwards to ESP32 via serial
# Depends  : winsdk (pip install winsdk), serial_comm

import asyncio
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

# Body text is truncated to this length before sending to avoid ESP32 buffer overruns.
_BODY_MAX_LEN = 50

# Callback type: async fn(app, title, body) → None
_NotifCallback = Callable[[str, str, str], Awaitable[None]]

# Module-level broadcast callback — set by main.py so the dashboard also sees notifs.
_broadcast_cb: _NotifCallback | None = None


def set_broadcast_callback(cb: _NotifCallback) -> None:
    """Register a coroutine that is called whenever a notification is dispatched."""
    global _broadcast_cb
    _broadcast_cb = cb


# ── winsdk imports (optional — silently disables on non-Windows) ──────────────
try:
    from winsdk.windows.ui.notifications.management import UserNotificationListener
    from winsdk.windows.ui.notifications import (
        UserNotificationChangedKind,
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
_loop: asyncio.AbstractEventLoop | None = None


def _extract_notification(notif_id: int) -> tuple[str, str, str] | None:
    """
    Pulls app name, title, and body out of a UserNotification.
    Returns None if anything is missing or the app is not in the allow-list.
    """
    if _listener is None:
        return None

    try:
        notif = _listener.get_notification(notif_id)
    except Exception:
        return None

    if notif is None:
        return None

    # ── App name ──────────────────────────────────────────────────────────
    try:
        app_name: str = notif.app_info.display_info.display_name
    except Exception:
        return None

    if app_name not in ALLOWED_APPS:
        return None

    # ── Text content ──────────────────────────────────────────────────────
    try:
        binding = notif.notification.visual.get_binding(
            KnownNotificationBindings.get_toast_generic()
        )
        text_elements = list(binding.get_text_elements())
    except Exception:
        return None

    title = text_elements[0].text if len(text_elements) > 0 else "Notification"
    body  = text_elements[1].text if len(text_elements) > 1 else ""

    # Truncate body before sending over serial
    if len(body) > _BODY_MAX_LEN:
        body = body[:_BODY_MAX_LEN - 1] + "…"

    return app_name, title, body


def _on_notification_changed(listener: "UserNotificationListener", args) -> None:
    """
    Fired by Windows whenever the Action Center changes.
    Runs synchronously on the WinRT thread — schedules async dispatch back
    onto the FastAPI event loop so serial_comm.send is always called from
    the correct thread.
    """
    # Only act on Added events; ignore Removed/Cleared to prevent duplicates.
    if args.change_kind != UserNotificationChangedKind.ADDED:
        return

    result = _extract_notification(args.user_notification_id)
    if result is None:
        return

    app_name, title, body = result

    if _loop is not None and not _loop.is_closed():
        asyncio.run_coroutine_threadsafe(_dispatch(app_name, title, body), _loop)


async def _dispatch(app_name: str, title: str, body: str) -> None:
    """Sends the notification over serial and optionally to the dashboard."""
    import json

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


async def start() -> None:
    """
    Requests Windows notification access and registers the event handler.
    Safe to call even if winsdk is unavailable — becomes a no-op.
    """
    global _listener, _loop

    if not _HAS_WINSDK:
        return

    _loop = asyncio.get_running_loop()

    try:
        _listener = UserNotificationListener.get_current()

        # Request OS permission to read notifications
        access_status = await _listener.request_access_async()
        logging.info(f"[Notifications] Access status: {access_status}")

        _listener.add_notification_changed(_on_notification_changed)
        logging.info(
            "[Notifications] Listener active. "
            f"Watching: {', '.join(sorted(ALLOWED_APPS))}"
        )
    except Exception as exc:
        logging.error(f"[Notifications] Failed to start listener: {exc}")


def stop() -> None:
    """Removes the event handler on shutdown."""
    global _listener
    if _listener is not None and _HAS_WINSDK:
        try:
            _listener.remove_notification_changed(_on_notification_changed)
        except Exception:
            pass
        _listener = None
    logging.info("[Notifications] Listener stopped.")
