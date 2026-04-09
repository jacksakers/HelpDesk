// Project  : HelpDesk
// File     : http_server.cpp
// Purpose  : Lightweight HTTP server — companion-app file uploads and settings sync
// Depends  : http_server.h, sd_utils.h, sd_card.h, settings.h, <WebServer.h>, ArduinoJson
//
// Routes:
//   GET  /status           — JSON: SD stats and firmware info
//   POST /upload/image     — multipart file → /images/<filename> on SD
//   POST /upload/audio     — multipart file → /mp3/<filename>   on SD
//   POST /settings         — JSON body → update and save device settings

#include "http_server.h"
#include "sd_utils.h"
#include "sd_card.h"
#include "settings.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SD.h>
#include <ArduinoJson.h>

// ── Module state ─────────────────────────────────────────────────────────────
static WebServer s_server(80);

/* Tracks the target directory for the active multipart upload. */
static const char * s_upload_target_dir = nullptr;
static File         s_upload_file;
static char         s_upload_filepath[96];
static bool         s_upload_ok = false;   // true only when the SD file opened successfully

// ── Path validation ───────────────────────────────────────────────────────────

/* Returns filename basename with directory components stripped.
   Writes into a caller-supplied buffer so there is no dangling pointer. */
static void safe_basename(const char * raw, char * out, size_t out_sz)
{
    /* Find the last slash */
    const char * base = raw;
    for (const char * p = raw; *p; p++) {
        if (*p == '/' || *p == '\\') base = p + 1;
    }
    strncpy(out, base, out_sz - 1);
    out[out_sz - 1] = '\0';
    /* Replace any remaining non-alphanumeric chars except . - _ */
    for (char * c = out; *c; c++) {
        if (!isalnum((unsigned char)*c) && *c != '.' && *c != '-' && *c != '_') {
            *c = '_';
        }
    }
    if (out[0] == '\0') strncpy(out, "upload.bin", out_sz - 1);
}

// ── Route: GET /status ────────────────────────────────────────────────────────

static void handle_status()
{
    JsonDocument doc;
    doc["fw_version"]  = "1.0";
    doc["sd_mounted"]  = sdCardMounted();
    if (sdCardMounted()) {
        doc["sd_total_mb"] = (int)(SD.totalBytes() / (1024ULL * 1024ULL));
        doc["sd_used_mb"]  = (int)(SD.usedBytes()  / (1024ULL * 1024ULL));
    }
    doc["ip"] = WiFi.localIP().toString();

    String out;
    serializeJson(doc, out);
    s_server.send(200, "application/json", out);
}

// ── Route: POST /upload/image  and  POST /upload/audio ───────────────────────

/* Called ONCE after the multipart upload finishes — send the response. */
static void handle_upload_done()
{
    /* Use s_upload_ok rather than s_upload_file: after close() the File object
       evaluates to false, which would always trigger the 500 path. */
    if (s_upload_ok) {
        Serial.printf("[HTTP] Upload complete: %s\n", s_upload_filepath);
        s_server.send(200, "application/json", "{\"ok\":true}");
    } else {
        s_server.send(500, "application/json", "{\"error\":\"write_failed\"}");
    }
    s_upload_ok         = false;
    s_upload_target_dir = nullptr;
}

/* Called repeatedly during the multipart stream (START → WRITE... → END). */
static void handle_file_upload()
{
    if (!sdCardMounted()) return;

    HTTPUpload & upload = s_server.upload();

    if (upload.status == UPLOAD_FILE_START) {
        s_upload_ok = false;   /* Reset for each new upload */
        /* Determine destination directory from the URI that matched. */
        s_upload_target_dir = s_server.uri().indexOf("image") >= 0 ? "/images" : "/mp3";
        sdEnsureDir(s_upload_target_dir);

        char basename[64];
        safe_basename(upload.filename.c_str(), basename, sizeof(basename));
        snprintf(s_upload_filepath, sizeof(s_upload_filepath),
                 "%s/%s", s_upload_target_dir, basename);

        if (SD.exists(s_upload_filepath)) SD.remove(s_upload_filepath);
        s_upload_file = SD.open(s_upload_filepath, FILE_WRITE);

        if (!s_upload_file) {
            Serial.printf("[HTTP] Upload: could not open %s\n", s_upload_filepath);
        } else {
            s_upload_ok = true;
            Serial.printf("[HTTP] Upload start: %s\n", s_upload_filepath);
        }

    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (s_upload_file) {
            s_upload_file.write(upload.buf, upload.currentSize);
        }

    } else if (upload.status == UPLOAD_FILE_END) {
        if (s_upload_file) {
            s_upload_file.close();
        }
    }
}

// ── Route: POST /settings ─────────────────────────────────────────────────────

static void handle_settings_post()
{
    String body = s_server.arg("plain");
    if (body.isEmpty()) {
        s_server.send(400, "application/json", "{\"error\":\"empty body\"}");
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        s_server.send(400, "application/json", "{\"error\":\"bad json\"}");
        return;
    }

    /* Apply only known keys — unknown fields are silently ignored. */
    if (doc["wifi_ssid"].is<const char*>())     settingsSetWifiSSID(doc["wifi_ssid"]);
    if (doc["wifi_password"].is<const char*>()) settingsSetWifiPassword(doc["wifi_password"]);
    if (doc["owm_api_key"].is<const char*>())   settingsSetOwmKey(doc["owm_api_key"]);
    if (doc["zip_code"].is<const char*>())      settingsSetOwmCity(doc["zip_code"]);
    if (doc["units"].is<const char*>())         settingsSetOwmUnits(doc["units"]);

    settingsSave();
    Serial.println("[HTTP] Settings updated from companion app.");
    s_server.send(200, "application/json", "{\"ok\":true}");
}

// ── Public API ────────────────────────────────────────────────────────────────

void httpServerInit(void)
{
    s_server.on("/status",        HTTP_GET,  handle_status);
    s_server.on("/upload/image",  HTTP_POST, handle_upload_done, handle_file_upload);
    s_server.on("/upload/audio",  HTTP_POST, handle_upload_done, handle_file_upload);
    s_server.on("/settings",      HTTP_POST, handle_settings_post);

    s_server.begin();
    Serial.printf("[HTTP] Server started. Visit http://%s/status\n",
                  WiFi.localIP().toString().c_str());
}

void httpServerLoop(void)
{
    s_server.handleClient();
}
