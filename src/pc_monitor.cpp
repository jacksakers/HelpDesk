// Project  : HelpDesk
// File     : pc_monitor.cpp
// Purpose  : PC metrics receiver — parses {"c":45,"r":60} from companion serial stream
// Depends  : pc_monitor.h, ui_Screen7.h, ArduinoJson

#include "pc_monitor.h"
#include "ui_Screen7.h"
#include <Arduino.h>
#include <ArduinoJson.h>

// ── Constants ────────────────────────────────────────────────────────────────
#define PC_MONITOR_TIMEOUT_MS  6000UL   // Show "Disconnected" after 6 s without data
#define LINE_BUF_SZ              96     // Companion payload is ~30 chars; headroom for safety

// ── Private state ────────────────────────────────────────────────────────────
static char          s_line[LINE_BUF_SZ];
static int           s_pos = 0;
static unsigned long s_last_data_ms = 0;
static bool          s_was_connected = false;

// ── Private helpers ──────────────────────────────────────────────────────────

static void set_bar(lv_obj_t * bar, lv_obj_t * label, int value, const char * colour_sym)
{
    if (!bar || !label) return;
    lv_bar_set_value(bar, value, LV_ANIM_OFF);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", value);
    lv_label_set_text(label, buf);
}

static void update_status_label(bool connected)
{
    if (!ui_PcStatusLabel) return;
    if (connected) {
        lv_label_set_text(ui_PcStatusLabel, LV_SYMBOL_USB "  Connected");
        lv_obj_set_style_text_color(ui_PcStatusLabel, lv_color_hex(0x4CAF50), 0);
    } else {
        lv_label_set_text(ui_PcStatusLabel,
                          LV_SYMBOL_USB "  Waiting for serial data...\n"
                          "Run the companion app on your PC.");
        lv_obj_set_style_text_color(ui_PcStatusLabel, lv_color_hex(0xAAAAAA), 0);
    }
}

static void process_line(const char * json)
{
    /* Only attempt to parse if it looks like JSON to avoid wasting cycles
       on the many non-JSON debug lines also emitted on the same UART. */
    if (json[0] != '{') return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) return;    /* Silently drop malformed JSON */

    /* Companion telemetry packet: {"c":<cpu>,"r":<ram>} */
    int cpu = doc["c"] | -1;
    int ram = doc["r"] | -1;

    if (cpu >= 0 && cpu <= 100) set_bar(ui_CpuBar, ui_CpuLabel, cpu, NULL);
    if (ram >= 0 && ram <= 100) set_bar(ui_RamBar, ui_RamLabel, ram, NULL);

    /* GPU bar — optional key "g"; companion app can add it later */
    int gpu = doc["g"] | -1;
    if (gpu >= 0 && gpu <= 100) set_bar(ui_GpuBar, ui_GpuLabel, gpu, NULL);
}

// ── Public API ───────────────────────────────────────────────────────────────

void initPcMonitor(void)
{
    s_pos            = 0;
    s_last_data_ms   = 0;
    s_was_connected  = false;
}

void handlePcMonitor(unsigned long now_ms)
{
    /* Drain the RX buffer a byte at a time — fast enough at 115200 baud. */
    while (Serial.available()) {
        char c = (char)Serial.read();

        if (c == '\n' || c == '\r') {
            if (s_pos > 0) {
                s_line[s_pos] = '\0';
                s_pos = 0;

                process_line(s_line);

                /* Any valid JSON line from the companion counts as "connected". */
                if (s_line[0] == '{') {
                    s_last_data_ms = now_ms;
                }
            }
        } else {
            /* Guard against buffer overrun — drop the start of the line. */
            if (s_pos < LINE_BUF_SZ - 1) {
                s_line[s_pos++] = c;
            } else {
                /* Line too long — flush and start over. */
                s_pos = 0;
            }
        }
    }

    /* Update connection indicator when status changes. */
    bool connected = (s_last_data_ms != 0) &&
                     (now_ms - s_last_data_ms < PC_MONITOR_TIMEOUT_MS);

    if (connected != s_was_connected) {
        s_was_connected = connected;
        update_status_label(connected);
    }
}
