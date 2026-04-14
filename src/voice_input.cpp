// Project  : HelpDesk
// File     : voice_input.cpp
// Purpose  : Microphone capture (I2S) → WAV → HTTP POST to companion for Whisper transcription
// Depends  : voice_input.h, settings.h, ui_Screen5.h, driver/i2s.h (IDF), <HTTPClient.h>
//
// Flow:
//   1. voiceInputStart()  — allocates PSRAM buffer, installs IDF I2S driver
//   2. handleVoiceInput() — called every loop(); drains i2s_read() until MIC_RECORD_SECS
//   3. After time elapsed  — builds WAV header, launches upload FreeRTOS task (core 0)
//   4. Upload task POSTs to http://{companion_ip}:8000/api/voice/transcribe
//   5. On success          — stores text; ui_Screen5 picks it up via voiceInputGetResult()

#include "voice_input.h"
#include "settings.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

/* IDF legacy I2S driver — available in every arduino-esp32 3.x build. */
#include <driver/i2s.h>

// ── WAV header layout (44 bytes) ──────────────────────────────────────────────
#pragma pack(push, 1)
struct WavHeader {
    char     chunk_id[4];       /* "RIFF"                          */
    uint32_t chunk_size;        /* total file size - 8             */
    char     format[4];         /* "WAVE"                          */
    char     sub1_id[4];        /* "fmt "                          */
    uint32_t sub1_size;         /* 16 for PCM                      */
    uint16_t audio_format;      /* 1 = PCM                         */
    uint16_t num_channels;      /* 1 = mono                        */
    uint32_t sample_rate;       /* MIC_SAMPLE_RATE                 */
    uint32_t byte_rate;         /* sample_rate * channels * bits/8 */
    uint16_t block_align;       /* channels * bits/8               */
    uint16_t bits_per_sample;   /* 16                              */
    char     sub2_id[4];        /* "data"                          */
    uint32_t sub2_size;         /* number of data bytes            */
};
#pragma pack(pop)

#define WAV_HEADER_SIZE       sizeof(WavHeader)
#define AUDIO_BYTES_PER_SEC   (MIC_SAMPLE_RATE * 1 * 2)  /* mono, 16-bit */
#define AUDIO_CAPTURE_BYTES   (AUDIO_BYTES_PER_SEC * MIC_RECORD_SECS)
#define BUF_TOTAL             (WAV_HEADER_SIZE + AUDIO_CAPTURE_BYTES)  /* ~100 KB in PSRAM */

// ── State machine ─────────────────────────────────────────────────────────────
typedef enum {
    VOICE_IDLE,
    VOICE_RECORDING,
    VOICE_UPLOADING,
    VOICE_DONE,
} voice_state_t;

static voice_state_t  s_state          = VOICE_IDLE;
static uint8_t       *s_buf            = nullptr;  /* PSRAM alloc — WAV header + audio */
static size_t         s_captured_bytes = 0;
static unsigned long  s_record_start   = 0;
static char           s_result[256]    = "";        /* transcription output */
static bool             s_result_ready   = false;
static voice_result_cb_t s_result_cb     = nullptr;

#define MIC_I2S_PORT  I2S_NUM_0   /* PDM mic only works on I2S0 on ESP32-S3 */

// ── IDF I2S helpers ───────────────────────────────────────────────────────────

static bool i2s_mic_install(void)
{
    i2s_config_t cfg = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
        .sample_rate          = MIC_SAMPLE_RATE,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count        = 4,
        .dma_buf_len          = 512,
        .use_apll             = false,
        .tx_desc_auto_clear   = false,
        .fixed_mclk           = 0,
    };

    esp_err_t err = i2s_driver_install(MIC_I2S_PORT, &cfg, 0, nullptr);
    if (err != ESP_OK) {
        Serial.printf("[Voice] i2s_driver_install failed: %d\n", err);
        return false;
    }

    /* PDM mic only needs CLK (WS pin) and DATA (SD pin).
       BCLK is generated internally for PDM; passing -1 disables it. */
    i2s_pin_config_t pins = {
        .mck_io_num   = I2S_PIN_NO_CHANGE,
        .bck_io_num   = I2S_PIN_NO_CHANGE,   /* PDM: no BCLK */
        .ws_io_num    = MIC_I2S_WS,          /* PDM CLK */
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num  = MIC_I2S_DATA,        /* PDM DATA */
    };
    err = i2s_set_pin(MIC_I2S_PORT, &pins);
    if (err != ESP_OK) {
        Serial.printf("[Voice] i2s_set_pin failed: %d\n", err);
        i2s_driver_uninstall(MIC_I2S_PORT);
        return false;
    }

    return true;
}

static void i2s_mic_uninstall(void)
{
    i2s_driver_uninstall(MIC_I2S_PORT);
}

// ── Upload task (runs on core 0 to free core 1 for LVGL) ─────────────────────

struct UploadArgs {
    uint8_t *buf;
    size_t   len;
    char     url[128];
};

static void upload_task(void *pvParams)
{
    UploadArgs *args = (UploadArgs *)pvParams;

    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(args->url);
        http.addHeader("Content-Type", "audio/wav");
        int code = http.POST(args->buf, (int)args->len);
        if (code == 200) {
            String resp = http.getString();
            /* Expected JSON: {"text": "buy groceries"} */
            /* Simple parse without ArduinoJson to keep stack small */
            int idx = resp.indexOf("\"text\"");
            if (idx >= 0) {
                int start = resp.indexOf('"', idx + 7) + 1;
                int end   = resp.indexOf('"', start);
                if (start > 0 && end > start) {
                    String text = resp.substring(start, end);
                    strncpy(s_result, text.c_str(), sizeof(s_result) - 1);
                    s_result[sizeof(s_result) - 1] = '\0';
                    s_result_ready = true;
                }
            }
        } else {
            Serial.printf("[Voice] Upload failed: HTTP %d\n", code);
        }
        http.end();
    } else {
        Serial.println("[Voice] Upload skipped: WiFi not connected.");
    }

    s_state = VOICE_DONE;
    free(args);
    vTaskDelete(nullptr);
}

// ── WAV header builder ────────────────────────────────────────────────────────
static void write_wav_header(uint8_t *buf, uint32_t audio_bytes)
{
    WavHeader *h = reinterpret_cast<WavHeader *>(buf);
    memcpy(h->chunk_id,  "RIFF", 4);
    h->chunk_size      = 36 + audio_bytes;
    memcpy(h->format,   "WAVE", 4);
    memcpy(h->sub1_id,  "fmt ", 4);
    h->sub1_size       = 16;
    h->audio_format    = 1;
    h->num_channels    = 1;
    h->sample_rate     = MIC_SAMPLE_RATE;
    h->byte_rate       = AUDIO_BYTES_PER_SEC;
    h->block_align     = 2;
    h->bits_per_sample = 16;
    memcpy(h->sub2_id, "data", 4);
    h->sub2_size       = audio_bytes;
}

// ── Public API ────────────────────────────────────────────────────────────────
void voiceInputInit(void)
{
    s_state        = VOICE_IDLE;
    s_result_ready = false;
    s_result[0]    = '\0';
    Serial.println("[Voice] Module ready.  Companion IP read at record time.");
}

void voiceInputStart(void)
{
    if (s_state != VOICE_IDLE) return;

    /* Skip if no companion IP configured */
    const char *ip = settingsGetCompanionIP();
    if (!ip || strlen(ip) == 0) {
        Serial.println("[Voice] Cannot start: companion_ip not set in settings.");
        return;
    }

    /* Allocate capture buffer in PSRAM */
    if (s_buf) { heap_caps_free(s_buf); s_buf = nullptr; }
    s_buf = (uint8_t *)heap_caps_malloc(BUF_TOTAL, MALLOC_CAP_SPIRAM);
    if (!s_buf) {
        Serial.println("[Voice] PSRAM alloc failed.");
        return;
    }

    /* Leave the first WAV_HEADER_SIZE bytes blank; audio starts after */
    s_captured_bytes = 0;

    /* Install IDF I2S driver in PDM-RX mode */
    if (!i2s_mic_install()) {
        heap_caps_free(s_buf);
        s_buf = nullptr;
        return;
    }

    s_record_start = millis();
    s_state        = VOICE_RECORDING;
    Serial.printf("[Voice] Recording started (%d s, %d Hz, WS=%d DIN=%d).\n",
                  MIC_RECORD_SECS, MIC_SAMPLE_RATE, MIC_I2S_WS, MIC_I2S_DATA);
}

void handleVoiceInput(unsigned long now_ms)
{
    if (s_state == VOICE_RECORDING) {
        /* Drain available I2S bytes via IDF i2s_read() */
        size_t remaining = AUDIO_CAPTURE_BYTES - s_captured_bytes;
        if (remaining > 0) {
            size_t bytes_read = 0;
            /* Non-blocking: 0 ms timeout — return whatever is in the DMA buffer now */
            i2s_read(MIC_I2S_PORT,
                     s_buf + WAV_HEADER_SIZE + s_captured_bytes,
                     remaining,
                     &bytes_read,
                     0);
            s_captured_bytes += bytes_read;
        }

        /* Stop after MIC_RECORD_SECS */
        if ((now_ms - s_record_start) >= (unsigned long)(MIC_RECORD_SECS * 1000UL)) {
            i2s_mic_uninstall();
            write_wav_header(s_buf, (uint32_t)s_captured_bytes);

            /* Build upload URL from companion IP */
            const char *ip = settingsGetCompanionIP();
            UploadArgs *args = (UploadArgs *)malloc(sizeof(UploadArgs));
            if (!args) { s_state = VOICE_IDLE; return; }
            args->buf = s_buf;
            args->len = WAV_HEADER_SIZE + s_captured_bytes;
            snprintf(args->url, sizeof(args->url),
                     "http://%s:8000/api/voice/transcribe", ip);

            s_state = VOICE_UPLOADING;
            Serial.printf("[Voice] Recording done (%u bytes). Uploading to %s\n",
                          (unsigned)s_captured_bytes, args->url);

            /* Fire upload on core 0 (ESP32-S3 dual-core) */
            xTaskCreatePinnedToCore(upload_task, "voice_upload",
                                    8192, args, 1, nullptr, 0);
        }
    }

    if (s_state == VOICE_DONE) {
        /* Free buffer now that upload task is finished */
        if (s_buf) { heap_caps_free(s_buf); s_buf = nullptr; }

        if (s_result_ready && s_result_cb) {
            s_result_cb(s_result);   /* UI code fills the textarea */
        }
        s_state = VOICE_IDLE;
    }
}

bool voiceInputIsActive(void)
{
    return s_state == VOICE_RECORDING || s_state == VOICE_UPLOADING;
}

const char *voiceInputGetResult(void)
{
    return s_result_ready ? s_result : nullptr;
}

void voiceInputClearResult(void)
{
    s_result_ready = false;
    s_result[0]    = '\0';
}

void voiceInputSetResultCallback(voice_result_cb_t cb)
{
    s_result_cb = cb;
}
