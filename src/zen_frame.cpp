// Project  : HelpDesk
// File     : zen_frame.cpp
// Purpose  : ZenFrame — cycle LVGL binary images from SD card on Screen6
// Depends  : zen_frame.h, sd_card.h, ui_Screen6.h, ui.h, <SD.h>
//
// Image format
// ────────────
// Store LVGL v9 binary image files (.bin) in /images on the SD card.
// Convert any picture using the online LVGL Image Converter:
//   https://lvgl.io/tools/imageconverter
// Settings: LVGL v9 · Color format: RGB565 · Output: Binary (.bin)
// Recommended resolution: 480 × 320 (fills the screen).
//
// How images are loaded
// ─────────────────────
// Each image is read entirely into a PSRAM buffer (ps_malloc).
// The LVGL binary header (12 bytes) is interpreted in place.
// The previous image buffer is freed before the next one loads.
// This approach keeps the 96 KB LVGL heap free for UI widgets.

#include "zen_frame.h"
#include "sd_card.h"
#include "ui.h"
#include <Arduino.h>
#include <SD.h>
#include <lvgl.h>

/* ── Config ──────────────────────────────────────────────────────────────── */
#define ZEN_DIR          "/images"
#define ZEN_CYCLE_MS     300000UL   /* Cycle interval: 5 minutes            */
#define MAX_IMAGES       64         /* Maximum images in the playlist        */
#define MAX_PATH_LEN     64         /* Maximum full SD path length           */

/* ── Module state ────────────────────────────────────────────────────────── */
static char     s_files[MAX_IMAGES][MAX_PATH_LEN];
static int      s_file_count  = 0;
static int      s_current_idx = 0;
static uint32_t s_last_change = 0;
static bool     s_initialized = false;

/* PSRAM buffer holding the currently displayed image's pixel data */
static uint8_t *       s_img_buf   = NULL;
static lv_image_dsc_t  s_img_dsc;

/* ── Private helpers ─────────────────────────────────────────────────────── */

static bool is_bin_file(const char * name)
{
    int len = (int)strlen(name);
    if (len < 5) return false;
    /* Case-insensitive ".bin" check */
    const char * ext = name + len - 4;
    return (tolower((unsigned char)ext[0]) == '.' &&
            tolower((unsigned char)ext[1]) == 'b' &&
            tolower((unsigned char)ext[2]) == 'i' &&
            tolower((unsigned char)ext[3]) == 'n');
}

static void scan_images(void)
{
    s_file_count = 0;

    if (!sdCardMounted()) {
        Serial.println("[ZenFrame] SD not mounted — no images loaded.");
        return;
    }

    File dir = SD.open(ZEN_DIR);
    if (!dir || !dir.isDirectory()) {
        Serial.println("[ZenFrame] /images not found on SD card.");
        return;
    }

    while (s_file_count < MAX_IMAGES) {
        File entry = dir.openNextFile();
        if (!entry) break;
        if (entry.isDirectory()) { entry.close(); continue; }

        const char * name = entry.name();
        if (is_bin_file(name)) {
            /* entry.name() on ESP32 SD returns just the filename */
            snprintf(s_files[s_file_count], MAX_PATH_LEN,
                     "%s/%s", ZEN_DIR, name);
            s_file_count++;
            Serial.printf("[ZenFrame]   + %s\n", s_files[s_file_count - 1]);
        }
        entry.close();
    }
    dir.close();

    Serial.printf("[ZenFrame] Playlist: %d image(s) in %s\n",
                  s_file_count, ZEN_DIR);
}

/* Load file at index idx into PSRAM and point s_img_dsc at the pixel data. */
static bool load_image(int idx)
{
    if (idx < 0 || idx >= s_file_count) return false;

    File f = SD.open(s_files[idx], FILE_READ);
    if (!f) {
        Serial.printf("[ZenFrame] Could not open: %s\n", s_files[idx]);
        return false;
    }

    size_t file_size = f.size();
    if (file_size <= sizeof(lv_image_header_t)) {
        Serial.printf("[ZenFrame] File too small (%u bytes): %s\n",
                      (unsigned)file_size, s_files[idx]);
        f.close();
        return false;
    }

    /* Free previous buffer before allocating a new one */
    if (s_img_buf) {
        free(s_img_buf);
        s_img_buf = NULL;
    }

    /* Allocate in PSRAM — avoids consuming the 96 KB LVGL heap */
    s_img_buf = (uint8_t *)ps_malloc(file_size);
    if (!s_img_buf) {
        Serial.printf("[ZenFrame] ps_malloc(%u) failed\n", (unsigned)file_size);
        f.close();
        return false;
    }

    size_t bytes_read = f.read(s_img_buf, file_size);
    f.close();

    if (bytes_read != file_size) {
        Serial.printf("[ZenFrame] Short read: got %u of %u bytes\n",
                      (unsigned)bytes_read, (unsigned)file_size);
        free(s_img_buf);
        s_img_buf = NULL;
        return false;
    }

    /* Interpret the first 12 bytes as lv_image_header_t in-place */
    memcpy(&s_img_dsc.header, s_img_buf, sizeof(lv_image_header_t));
    s_img_dsc.data      = s_img_buf + sizeof(lv_image_header_t);
    s_img_dsc.data_size = (uint32_t)(file_size - sizeof(lv_image_header_t));

    Serial.printf("[ZenFrame] Loaded %s — %ux%u cf=%d (%u bytes)\n",
                  s_files[idx],
                  s_img_dsc.header.w, s_img_dsc.header.h,
                  (int)s_img_dsc.header.cf,
                  (unsigned)file_size);
    return true;
}

static void show_current(void)
{
    /* ui_ZenImage is NULL if Screen6 is not mounted — safe to skip */
    if (!ui_ZenImage) return;

    if (s_file_count == 0) {
        /* No images found — leave the placeholder label visible */
        return;
    }

    if (!load_image(s_current_idx)) return;

    lv_image_set_src(ui_ZenImage, &s_img_dsc);
    lv_obj_center(ui_ZenImage);
}

/* ── Public API ──────────────────────────────────────────────────────────── */

void zenFrameInit(void)
{
    memset(&s_img_dsc, 0, sizeof(s_img_dsc));
    scan_images();
    s_current_idx = 0;
    s_last_change = millis();
    s_initialized = true;

    show_current();   /* Display first image if Screen6 is already live */
}

void zenFrameNext(void)
{
    if (s_file_count == 0) return;
    s_current_idx = (s_current_idx + 1) % s_file_count;
    s_last_change = millis();
    show_current();
}

void zenFrameRefresh(void)
{
    /* Called when Screen6 opens — re-apply current image to the new widget */
    show_current();
}

void handleZenFrame(unsigned long now_ms)
{
    if (!s_initialized || s_file_count <= 1) return;

    if ((uint32_t)(now_ms - s_last_change) >= ZEN_CYCLE_MS) {
        s_current_idx = (s_current_idx + 1) % s_file_count;
        s_last_change = now_ms;
        show_current();
    }
}
