# HelpDesk Companion App

A lightweight Python background app that bridges your PC to the HelpDesk device over USB serial.

---

## What it does

- Streams CPU / RAM usage to the **PC Monitor** screen in real time
- Forwards Windows notifications (Chrome, Slack, Discord, Teams, etc.) to the **Notifications** screen
- Hosts a local web dashboard at **http://127.0.0.1:8000** for managing settings, uploading media, and triggering macros

---

## Requirements

- Python 3.10 or newer
- The HelpDesk device connected via USB
- Windows 10/11 *(for notifications — the rest works on any OS)*

---

## Setup

**1. Install dependencies**

```
pip install -r requirements.txt
```

**2. Allow notification access (Windows only)**

Go to **Settings → Privacy & Security → Notifications** and turn on *"Allow apps to access your notifications"*.

**3. Run the app**

```
python main.py
```

The dashboard opens at **http://127.0.0.1:8000** in your browser.

---

## Dashboard tabs

| Tab | What you can do |
|-----|----------------|
| Device Status | See connection state, firmware version, SD card usage, and serial log |
| Settings | Set your Wi-Fi credentials, OpenWeatherMap API key, zip code, and device IP |
| Media | Upload images (auto-resized to 480×320 BMP) and MP3s to the device SD card |
| Macros | Trigger button macros mapped to keyboard shortcuts or scripts |

---

## Notifications

The app watches the Windows Action Center for new notifications from:

> Google Chrome, Microsoft Edge, Slack, Discord, Microsoft Teams, Telegram, WhatsApp

When one arrives it is forwarded over serial to the HelpDesk. The **Notifs** tile on the launcher flashes and shows a red unread count badge. Open the screen to see the grouped sender list, then tap **Clear** to dismiss them.

To add or remove apps from the watch list, edit `ALLOWED_APPS` near the top of `notifications.py`.

---

## Troubleshooting

**Device not detected** — The app scans for CP210x / CH34x / FT232 USB-serial chips automatically. If it misses your port, check Device Manager and verify the driver is installed.

**Notifications not arriving** — Confirm the privacy setting above is enabled and that `winsdk` installed without errors (`pip show winsdk`).

**Dashboard shows "Disconnected"** — The serial link uses 115200 baud. Make sure no other program (e.g. Arduino Serial Monitor) has the COM port open at the same time.
