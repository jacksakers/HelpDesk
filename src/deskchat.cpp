// Project  : HelpDesk
// File     : deskchat.cpp
// Purpose  : DeskChat — LoRa group-chat backend (SX1262 via RadioLib)
// Depends  : deskchat.h, sd_card.h, settings.h, RadioLib, ArduinoJson

#include "deskchat.h"
#include "sd_card.h"
#include "settings.h"
#include <Arduino.h>
#include <RadioLib.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <SD.h>

// ── LoRa Pin Configuration ────────────────────────────────────────────────────
#define LORA_USE_SD_SPI   0   /* 0 = own SPI on the Elecrow Header */

#if LORA_USE_SD_SPI
  // (Ignoring this section since we are using dedicated pins)
#else
  // Dedicated SPI — Mapped to the "Upside Down" Custom Layout
  #define LORA_SCK    16    /* SPI clock (CLK) -> IO16 */
  #define LORA_MOSI   3    /* SPI MOSI -> IO3 */
  #define LORA_MISO   9    /* SPI MISO -> IO9 */
  #define LORA_NSS    2    /* Chip-select (CS) -> IO2 */
  
  #define LORA_DIO1   15   /* Interrupt / data-ready -> IO15 */
  #define LORA_BUSY   1    /* BUSY signal -> IO1 */
  #define LORA_RST    10   /* Active-low reset -> IO10 */

  // Antenna Switch Pins
  #define LORA_TX_EN  46   /* TX Enable -> IO46 */
  #define LORA_RX_EN  0    /* RX Enable -> IO0 */
#endif

// ── LoRa Radio Parameters ─────────────────────────────────────────────────────
// These MUST match on every HelpDesk in the same group-chat network.
#define LORA_FREQ_MHZ     915.0f   // 915 = US/AU; use 868.0 for EU
#define LORA_BW_KHZ       125.0f
#define LORA_SF               9
#define LORA_CR               7    // 4/7 — matches design doc
#define LORA_SYNC_WORD    0x12    // Private network (ignores public LoRaWAN)
#define LORA_POWER_DBM       10
#define LORA_PREAMBLE_LEN     8
#define LORA_TCXO_V         0.0f  // 0 = crystal osc  |  1.8 for Heltec-style TCXO

// ── History file ──────────────────────────────────────────────────────────────
#define CHAT_HISTORY_FILE  "/chat/history.txt"
#define CHAT_MAX_FILE_KB    64     /* Trim history file when it exceeds this */

// ── Private state ─────────────────────────────────────────────────────────────
static SX1262 * s_radio      = nullptr;
static bool     s_ready      = false;
static int      s_last_rssi  = 0;
static bool     s_observe    = false;
static char     s_device_id[9] = "";   /* 8-char hex + NUL */

static deskchat_msg_cb_t s_cb = nullptr;

/* ISR flag: set inside the interrupt, cleared in handleDeskChat */
static volatile bool s_rx_flag = false;

/* Pending send buffer — set from UI task, sent in handleDeskChat */
static char     s_tx_buf[CHAT_LINE_MAX] = "";
static volatile bool s_tx_pending = false;

// ── ISR ───────────────────────────────────────────────────────────────────────
IRAM_ATTR static void rx_isr(void)
{
    s_rx_flag = true;
}

// ── Private helpers ───────────────────────────────────────────────────────────

static void build_device_id(void)
{
    /* Use lower 32 bits of the factory-fused chip ID for an 8-char hex ID. */
    uint64_t mac = ESP.getEfuseMac();
    uint32_t low = (uint32_t)(mac & 0xFFFFFFFF);
    snprintf(s_device_id, sizeof(s_device_id), "%08X", low);
}

static void emit_to_serial(const char *user, const char *id,
                            const char *msg, int rssi, bool raw)
{
    /* Forward every receive to the companion app over Serial. */
    char buf[256];
    if (raw) {
        snprintf(buf, sizeof(buf),
                 "{\"event\":\"lora_raw\",\"raw\":\"%s\",\"rssi\":%d}", msg, rssi);
    } else {
        /* Escape single-quote chars in user/msg to keep JSON valid */
        snprintf(buf, sizeof(buf),
                 "{\"event\":\"lora_msg\",\"id\":\"%s\",\"user\":\"%s\","
                 "\"msg\":\"%s\",\"rssi\":%d}", id, user, msg, rssi);
    }
    Serial.println(buf);
}

static void dispatch(const char *user, const char *id,
                     const char *msg, int rssi)
{
    if (s_cb) s_cb(user, id, msg, rssi);
    emit_to_serial(user, id, msg, rssi, false);
    deskChatHistoryAppend(user, id, msg, rssi);
}

static void handle_raw_packet(const String &data, int rssi)
{
    /* In observe mode, try to decode any HelpDesk packet first;
       if it doesn't parse, forward the raw bytes as a hex string. */
    if (data.length() == 0) return;

    if (data[0] == '{') {
        // Attempt HelpDesk JSON parse
        JsonDocument doc;
        if (deserializeJson(doc, data) == DeserializationError::Ok) {
            const char *id   = doc["id"]   | "";
            const char *user = doc["user"] | "";
            const char *msg  = doc["msg"]  | "";
            if (msg[0] != '\0') {
                dispatch(user[0] ? user : "?", id[0] ? id : "?", msg, rssi);
                return;
            }
        }
    }

    if (s_observe) {
        /* Unknown packet — relay raw bytes to companion + show in chat */
        if (s_cb) s_cb(nullptr, nullptr, data.c_str(), rssi);
        emit_to_serial(nullptr, nullptr, data.c_str(), rssi, true);
    }
}

static void process_rx(void)
{
    if (!s_radio) return;

    String received;
    int state = s_radio->readData(received);

    if (state == RADIOLIB_ERR_NONE) {
        s_last_rssi = (int)s_radio->getRSSI();

        if (received.length() == 0) {
            s_radio->startReceive();
            return;
        }

        if (received[0] == '{') {
            JsonDocument doc;
            if (deserializeJson(doc, received) == DeserializationError::Ok) {
                const char *id   = doc["id"]   | "";
                const char *user = doc["user"] | "";
                const char *msg  = doc["msg"]  | "";
                if (msg[0] != '\0') {
                    dispatch(user[0] ? user : "Anon",
                             id[0]   ? id   : "?",
                             msg, s_last_rssi);
                    s_radio->startReceive();
                    return;
                }
            }
        }
        handle_raw_packet(received, s_last_rssi);

    } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
        Serial.println("[LoRa] CRC error on received packet");
    } else {
        Serial.printf("[LoRa] readData error: %d\n", state);
    }

    s_radio->startReceive();
}

static void do_transmit(void)
{
    if (!s_radio) return;

    /* Build JSON packet */
    const char *username = settingsGetChatUsername();
    char packet[CHAT_LINE_MAX + 64];
    /* Simple manual JSON to avoid ArduinoJson overhead during TX */
    snprintf(packet, sizeof(packet),
             "{\"id\":\"%s\",\"user\":\"%s\",\"msg\":\"%s\"}",
             s_device_id,
             username[0] ? username : "Anon",
             s_tx_buf);

    int state = s_radio->transmit(packet);
    if (state == RADIOLIB_ERR_NONE) {
        /* Echo own message back to UI as a local entry */
        dispatch(username[0] ? username : "Anon", s_device_id,
                 s_tx_buf, 0 /* own TX: no RSSI */);
        Serial.printf("[LoRa] Sent: %s\n", packet);
    } else {
        Serial.printf("[LoRa] TX error: %d\n", state);
    }

    /* Return to receive mode */
    s_radio->startReceive();
}

// ── Public API ────────────────────────────────────────────────────────────────

void deskChatInit(void)
{
    build_device_id();
    Serial.printf("[DeskChat] Device ID: %s\n", s_device_id);

    /* Create the SD /chat directory if absent */
    if (sdCardMounted() && !SD.exists("/chat")) {
        SD.mkdir("/chat");
    }

#if LORA_USE_SD_SPI
    SPIClass *bus = sdCardGetSPI();   /* Share HSPI bus with SD card */
#else
    static SPIClass lora_spi(FSPI);
    lora_spi.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
    SPIClass *bus = &lora_spi;
#endif

    static Module mod(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY, *bus);
    static SX1262 radio(&mod);
    s_radio = &radio;

    int state = s_radio->begin(LORA_FREQ_MHZ, LORA_BW_KHZ, LORA_SF, LORA_CR,
                                LORA_SYNC_WORD, LORA_POWER_DBM,
                                LORA_PREAMBLE_LEN, LORA_TCXO_V);
    if (state == RADIOLIB_ERR_NONE) {
        /* Tell RadioLib to drive the antenna switch automatically.            */
        /* RADIOLIB_NC means "not connected" — safe to pass for shared-bus    */
        /* mode where we didn't wire TXEn/RXEn.                               */
        s_radio->setRfSwitchPins(LORA_RX_EN, LORA_TX_EN);

        attachInterrupt(digitalPinToInterrupt(LORA_DIO1), rx_isr, RISING);
        s_radio->startReceive();
        s_ready = true;
        Serial.printf("[LoRa] Radio ready. %.1f MHz  BW=%.0f  SF=%d  CR=4/%d\n",
                      LORA_FREQ_MHZ, LORA_BW_KHZ, LORA_SF, LORA_CR - 4);
    } else {
        s_ready = false;
        Serial.printf("[LoRa] Radio init FAILED (state %d). "
                      "Check pin definitions in deskchat.cpp.\n", state);
    }
}

void handleDeskChat(unsigned long /*now_ms*/)
{
    /* Process any pending transmit first (stops receive until done) */
    if (s_tx_pending) {
        s_tx_pending = false;
        do_transmit();
    }

    /* Process any received packet flagged by ISR */
    if (s_rx_flag) {
        s_rx_flag = false;
        process_rx();
    }
}

bool deskChatSend(const char *text)
{
    if (!s_ready || !text || text[0] == '\0') return false;
    if (s_tx_pending) return false;   /* Busy with previous send */

    strncpy(s_tx_buf, text, sizeof(s_tx_buf) - 1);
    s_tx_buf[sizeof(s_tx_buf) - 1] = '\0';
    s_tx_pending = true;
    return true;
}

void deskChatSetCallback(deskchat_msg_cb_t cb)
{
    s_cb = cb;
}

bool deskChatReady(void)
{
    return s_ready;
}

int deskChatLastRSSI(void)
{
    return s_last_rssi;
}

void deskChatGetDeviceID(char *buf, int buf_len)
{
    strncpy(buf, s_device_id, buf_len - 1);
    buf[buf_len - 1] = '\0';
}

// ── History (SD) ─────────────────────────────────────────────────────────────

void deskChatHistoryAppend(const char *user, const char *id,
                            const char *msg, int rssi)
{
    if (!sdCardMounted()) return;
    if (!user || !msg) return;

    File f = SD.open(CHAT_HISTORY_FILE, FILE_APPEND);
    if (!f) return;

    /* One JSON object per line (JSON Lines format) */
    char line[CHAT_LINE_MAX + 80];
    snprintf(line, sizeof(line),
             "{\"u\":\"%s\",\"id\":\"%s\",\"m\":\"%s\",\"r\":%d}\n",
             user ? user : "", id ? id : "", msg, rssi);
    f.print(line);
    f.close();
}

void deskChatHistoryLoad(deskchat_msg_cb_t cb)
{
    if (!sdCardMounted() || !cb) return;

    File f = SD.open(CHAT_HISTORY_FILE, FILE_READ);
    if (!f) return;

    char line[CHAT_LINE_MAX + 80];
    while (f.available()) {
        int n = f.readBytesUntil('\n', line, (int)sizeof(line) - 1);
        if (n <= 0) continue;
        line[n] = '\0';

        JsonDocument doc;
        if (deserializeJson(doc, line) != DeserializationError::Ok) continue;

        const char *u = doc["u"] | "";
        const char *i = doc["id"] | "";
        const char *m = doc["m"] | "";
        int r = doc["r"] | 0;
        if (m[0] != '\0') cb(u, i, m, r);
    }
    f.close();
}

void deskChatHistoryClear(void)
{
    if (!sdCardMounted()) return;
    if (SD.exists(CHAT_HISTORY_FILE)) {
        SD.remove(CHAT_HISTORY_FILE);
        Serial.println("[DeskChat] Chat history cleared.");
    }
}

// ── Observe mode ──────────────────────────────────────────────────────────────

void deskChatSetObserveMode(bool enable)
{
    s_observe = enable;
    Serial.printf("[DeskChat] Observe mode: %s\n", enable ? "ON" : "OFF");
}

bool deskChatGetObserveMode(void)
{
    return s_observe;
}
