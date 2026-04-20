// Project  : HelpDesk
// File     : deskchat.h
// Purpose  : DeskChat — LoRa group-chat module (SX1262 via RadioLib)
// Depends  : RadioLib, sd_card.h, settings.h

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

// ── Public constants ─────────────────────────────────────────────────────────

/* Maximum length of one formatted chat line   "<USER> MSG"               */
#define CHAT_LINE_MAX   200

/* Maximum stored history entries (ring-buffer; oldest dropped when full)  */
#define CHAT_HISTORY_MAX  200

// ── Callback ─────────────────────────────────────────────────────────────────

/* Called (on the main task) when a message arrives or when a raw packet is
   captured in observe/listen mode.
     user  — sender username  (NULL for raw observe packets)
     id    — sender device ID (NULL for raw observe packets)
     msg   — message text, or raw hex string in observe mode
     rssi  — signal strength in dBm                                        */
typedef void (*deskchat_msg_cb_t)(const char *user, const char *id,
                                  const char *msg, int rssi);

// ── Lifecycle ────────────────────────────────────────────────────────────────

/* Initialise the SX1262 radio and start background receive.
   Call once in setup() after sdCardInit() and settingsInit().             */
void deskChatInit(void);

/* Poll LoRa state; dispatch pending messages via the registered callback.
   Call every loop() tick.                                                 */
void handleDeskChat(unsigned long now_ms);

// ── Messaging ────────────────────────────────────────────────────────────────

/* Transmit a group message using the configured username / device ID.
   Returns true if the packet was accepted by RadioLib.                    */
bool deskChatSend(const char *text);

/* Register the UI callback.  Pass NULL to unregister (called on destroy). */
void deskChatSetCallback(deskchat_msg_cb_t cb);

// ── Status ───────────────────────────────────────────────────────────────────

/* True if the SX1262 initialised successfully.                            */
bool deskChatReady(void);

/* Most recent received RSSI in dBm; returns 0 if nothing received yet.   */
int  deskChatLastRSSI(void);

/* Fill buf with this device's unique 8-char hex ID (null-terminated).    */
void deskChatGetDeviceID(char *buf, int buf_len);

// ── History (SD card) ────────────────────────────────────────────────────────

/* Append one message to the SD history file.  Called internally.         */
void deskChatHistoryAppend(const char *user, const char *id,
                            const char *msg, int rssi);

/* Replay saved history by firing cb once per entry (oldest → newest).    */
void deskChatHistoryLoad(deskchat_msg_cb_t cb);

/* Delete the history file from SD.  Called from the Settings screen.     */
void deskChatHistoryClear(void);

// ── Observe mode ─────────────────────────────────────────────────────────────

/* When true, every received packet — HelpDesk-format or foreign — is
   forwarded to the registered callback.  Useful for testing "what is
   in the air right now?".  Defaults to false.                             */
void deskChatSetObserveMode(bool enable);
bool deskChatGetObserveMode(void);

#ifdef __cplusplus
} /* extern "C" */
#endif
