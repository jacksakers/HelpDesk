// Project  : HelpDesk
// File     : serial_fs.cpp
// Purpose  : Serial-FS command dispatcher — file system, settings, tasks, calendar over UART
// Depends  : serial_fs.h, sd_card.h, sd_utils.h, settings.h, task_master.h, calendar.h, zen_frame.h
//
// Protocol (all messages are newline-terminated JSON):
//   Companion → Device : {"cmd":"<op>","rid":<int>,...params}
//   Device → Companion : {"event":"resp","rid":<int>,"ok":<bool>[,"items"|"data"|"settings":...]}
//   Device → Companion : {"event":"chunk","rid":<int>,"seq":<int>,"total":<int>,"data":"<b64>"}
//
// Supported commands:
//   fs_list       {"cmd":"fs_list","rid":N,"dir":"/"}
//   fs_write      {"cmd":"fs_write","rid":N,"path":"...","content":"..."}
//   fs_mkdir      {"cmd":"fs_mkdir","rid":N,"path":"..."}
//   fs_delete     {"cmd":"fs_delete","rid":N,"path":"..."}
//   fs_download   {"cmd":"fs_download","rid":N,"path":"..."} → N chunk events
//   upload_chunk  {"cmd":"upload_chunk","rid":N,"path":"...","seq":N,"total":N,"data":"<b64>"}
//   get_settings  {"cmd":"get_settings","rid":N}
//   task_list     {"cmd":"task_list","rid":N}
//   task_add      {"cmd":"task_add","rid":N,"text":"...","repeat":bool,"due_date":"..."}
//   task_complete {"cmd":"task_complete","rid":N,"id":N}
//   task_delete   {"cmd":"task_delete","rid":N,"id":N}
//   cal_list      {"cmd":"cal_list","rid":N}
//   cal_add       {"cmd":"cal_add","rid":N,"title":"...","date":"...","start_time":"...","end_time":"...","all_day":bool}
//   cal_delete    {"cmd":"cal_delete","rid":N,"id":N}

#include "serial_fs.h"
#include "sd_card.h"
#include "sd_utils.h"
#include "settings.h"
#include "task_master.h"
#include "calendar.h"
#include "zen_frame.h"
#include <Arduino.h>
#include <SD.h>
#include <ArduinoJson.h>
#include <mbedtls/base64.h>

// ── Constants ──────────────────────────────────────────────────────────────────
#define DOWNLOAD_CHUNK_RAW  192   // raw bytes streamed per download chunk (→ 256 B base64)
#define UPLOAD_CHUNK_MAX    256   // max decoded bytes per incoming upload chunk
#define B64_ENCODE_BUF_SZ   352  // ceil(192/3)*4 + safety margin

// ── Upload state machine ───────────────────────────────────────────────────────
// Chunks arrive sequentially; state is carried across process_line() calls.
static char   s_up_path[128] = "";
static File   s_up_file;
static int    s_up_rid   = -1;
static int    s_up_total = 0;
static int    s_up_rcvd  = 0;
static bool   s_up_ok    = false;

// ── Private helpers ────────────────────────────────────────────────────────────

/* Validate an SD card path: must start with '/', no '..' component. */
static bool valid_sd_path(const char * path)
{
    if (!path || path[0] != '/') return false;
    for (const char * p = path; *p; p++) {
        if (p[0] == '.' && p[1] == '.' && (p[2] == '/' || p[2] == '\0')) return false;
    }
    return true;
}

/* Emit a minimal success/failure response. */
static void send_resp(int rid, bool ok, const char * error = nullptr)
{
    if (error && error[0]) {
        Serial.printf("{\"event\":\"resp\",\"rid\":%d,\"ok\":false,\"error\":\"%s\"}\n", rid, error);
    } else {
        Serial.printf("{\"event\":\"resp\",\"rid\":%d,\"ok\":%s}\n", rid, ok ? "true" : "false");
    }
    Serial.flush();
}

// ── Command handlers ───────────────────────────────────────────────────────────

static void handle_fs_list(const JsonDocument & doc, int rid)
{
    if (!sdCardMounted()) { send_resp(rid, false, "sd_not_mounted"); return; }

    const char * d = doc["dir"] | "/";
    if (!valid_sd_path(d)) { send_resp(rid, false, "invalid_path"); return; }

    File root = SD.open(d);
    if (!root || !root.isDirectory()) { send_resp(rid, false, "not_a_directory"); return; }

    /* Stream the array directly to Serial — avoids a large heap allocation. */
    Serial.printf("{\"event\":\"resp\",\"rid\":%d,\"ok\":true,\"items\":[", rid);
    bool first = true;
    int  count = 0;
    root.rewindDirectory();
    while (count < 200) {
        File entry = root.openNextFile();
        if (!entry) break;
        const char * name = entry.name();
        if (!name || name[0] == '\0' || name[0] == '.') { entry.close(); continue; }

        if (!first) Serial.print(',');
        first = false;

        /* Use ArduinoJson to handle any special characters in filenames. */
        JsonDocument j;
        j["name"]   = name;
        j["is_dir"] = entry.isDirectory();
        if (!entry.isDirectory()) j["size"] = (int)entry.size();
        serializeJson(j, Serial);

        entry.close();
        count++;
    }
    root.close();
    Serial.print("]}\n");
    Serial.flush();
}

static void handle_fs_write(const JsonDocument & doc, int rid)
{
    if (!sdCardMounted()) { send_resp(rid, false, "sd_not_mounted"); return; }

    const char * path    = doc["path"]    | "";
    const char * content = doc["content"] | "";
    if (!valid_sd_path(path)) { send_resp(rid, false, "invalid_path"); return; }

    if (SD.exists(path)) SD.remove(path);
    File f = SD.open(path, FILE_WRITE);
    if (!f) { send_resp(rid, false, "open_failed"); return; }

    f.print(content);
    f.close();
    send_resp(rid, true);
}

static void handle_fs_mkdir(const JsonDocument & doc, int rid)
{
    if (!sdCardMounted()) { send_resp(rid, false, "sd_not_mounted"); return; }

    const char * path = doc["path"] | "";
    if (!valid_sd_path(path)) { send_resp(rid, false, "invalid_path"); return; }

    if (SD.exists(path)) { send_resp(rid, true); return; }   /* already exists — not an error */

    bool ok = SD.mkdir(path);
    send_resp(rid, ok, ok ? nullptr : "mkdir_failed");
}

static void handle_fs_delete(const JsonDocument & doc, int rid)
{
    if (!sdCardMounted()) { send_resp(rid, false, "sd_not_mounted"); return; }

    const char * path = doc["path"] | "";
    if (!valid_sd_path(path) || strcmp(path, "/") == 0) { send_resp(rid, false, "invalid_path"); return; }
    if (!SD.exists(path)) { send_resp(rid, false, "not_found"); return; }

    File f = SD.open(path);
    bool is_dir = f.isDirectory();
    f.close();
    bool ok = is_dir ? SD.rmdir(path) : SD.remove(path);
    send_resp(rid, ok, ok ? nullptr : "delete_failed");
}

static void handle_fs_download(const JsonDocument & doc, int rid)
{
    if (!sdCardMounted()) { send_resp(rid, false, "sd_not_mounted"); return; }

    const char * path = doc["path"] | "";
    if (!valid_sd_path(path)) { send_resp(rid, false, "invalid_path"); return; }

    File f = SD.open(path, FILE_READ);
    if (!f || f.isDirectory()) { send_resp(rid, false, "not_found"); return; }

    size_t file_sz     = f.size();
    /* At least 1 chunk even for an empty file so the companion resolves its future. */
    int total_chunks = (file_sz == 0) ? 1
                     : (int)((file_sz + DOWNLOAD_CHUNK_RAW - 1) / DOWNLOAD_CHUNK_RAW);

    uint8_t raw_buf[DOWNLOAD_CHUNK_RAW];
    uint8_t b64_buf[B64_ENCODE_BUF_SZ];
    int seq = 0;

    if (file_sz == 0) {
        /* Empty file — one chunk with empty data. */
        Serial.printf("{\"event\":\"chunk\",\"rid\":%d,\"seq\":0,\"total\":1,\"data\":\"\"}\n", rid);
        Serial.flush();
    } else {
        while (f.available()) {
            int n = f.read(raw_buf, DOWNLOAD_CHUNK_RAW);
            if (n <= 0) break;

            size_t enc_len = 0;
            mbedtls_base64_encode(b64_buf, B64_ENCODE_BUF_SZ, &enc_len, raw_buf, (size_t)n);
            b64_buf[enc_len] = '\0';

            Serial.printf("{\"event\":\"chunk\",\"rid\":%d,\"seq\":%d,\"total\":%d,\"data\":\"%s\"}\n",
                          rid, seq, total_chunks, (char *)b64_buf);
            Serial.flush();
            seq++;
            /* Yield for a couple of ms so LVGL is not completely starved. */
            delay(2);
        }
    }
    f.close();
}

static void handle_upload_chunk(const JsonDocument & doc, int rid)
{
    if (!sdCardMounted()) { send_resp(rid, false, "sd_not_mounted"); return; }

    const char * path = doc["path"] | "";
    int seq           = doc["seq"]   | -1;
    int total         = doc["total"] | 0;
    const char * b64  = doc["data"]  | "";

    if (!valid_sd_path(path) || seq < 0 || total <= 0) {
        send_resp(rid, false, "invalid_params");
        return;
    }

    /* Decode base64 payload. */
    uint8_t raw[UPLOAD_CHUNK_MAX + 8];
    size_t  decoded_len = 0;
    int ret = mbedtls_base64_decode(raw, sizeof(raw), &decoded_len,
                                    (const uint8_t *)b64, strlen(b64));
    if (ret != 0) {
        send_resp(rid, false, "b64_decode_error");
        return;
    }

    /* seq == 0 → first chunk: open file and initialise state. */
    if (seq == 0) {
        if (s_up_file) s_up_file.close();
        s_up_ok    = false;
        s_up_rid   = rid;
        s_up_total = total;
        s_up_rcvd  = 0;
        strncpy(s_up_path, path, sizeof(s_up_path) - 1);
        s_up_path[sizeof(s_up_path) - 1] = '\0';

        /* Ensure parent directory exists. */
        char dir_buf[128];
        strncpy(dir_buf, path, sizeof(dir_buf) - 1);
        char * slash = strrchr(dir_buf, '/');
        if (slash && slash != dir_buf) { *slash = '\0'; sdEnsureDir(dir_buf); }

        if (SD.exists(path)) SD.remove(path);
        s_up_file = SD.open(path, FILE_WRITE);
        if (!s_up_file) {
            Serial.printf("[SerialFS] Upload: could not open %s\n", path);
            s_up_ok = false;
        } else {
            s_up_ok = true;
        }
    } else if (rid != s_up_rid || strcmp(path, s_up_path) != 0) {
        /* Stale or mismatched chunk — drop silently. */
        return;
    }

    if (s_up_ok && s_up_file) {
        s_up_file.write(raw, decoded_len);
        s_up_rcvd++;
    }

    /* Last chunk → close file and send final response. */
    if (seq == total - 1) {
        if (s_up_file) s_up_file.close();

        bool ok = s_up_ok;
        if (ok) {
            /* Notify ZenFrame if a new image landed. */
            if (strncmp(s_up_path, "/images", 7) == 0) zenFrameRescan();
            Serial.printf("{\"event\":\"resp\",\"rid\":%d,\"ok\":true,\"path\":\"%s\"}\n", rid, s_up_path);
            Serial.flush();
        } else {
            send_resp(rid, false, "write_failed");
        }

        /* Reset upload state. */
        s_up_path[0] = '\0';
        s_up_rid     = -1;
        s_up_total   = 0;
        s_up_rcvd    = 0;
        s_up_ok      = false;
    }
}

static void handle_get_settings(int rid)
{
    Serial.printf("{\"event\":\"resp\",\"rid\":%d,\"ok\":true,\"settings\":", rid);

    JsonDocument doc;
    doc["wifi_ssid"]    = settingsGetWifiSSID();
    doc["owm_api_key"]  = settingsGetOwmKey();
    doc["zip_code"]     = settingsGetOwmCity();
    doc["units"]        = settingsGetOwmUnits();
    doc["companion_ip"] = settingsGetCompanionIP();
    serializeJson(doc, Serial);

    Serial.print("}\n");
    Serial.flush();
}

static void handle_task_list(int rid)
{
    Serial.printf("{\"event\":\"resp\",\"rid\":%d,\"ok\":true,\"data\":{\"tasks\":[", rid);

    int count = taskGetCount();
    for (int i = 0; i < count; i++) {
        const task_t * t = taskGet(i);
        if (!t) continue;
        if (i > 0) Serial.print(',');

        JsonDocument j;
        j["id"]       = (uint32_t)t->id;
        j["text"]     = t->text;
        j["due_date"] = t->due_date;
        j["repeat"]   = t->repeat;
        j["done"]     = t->done_today;
        serializeJson(j, Serial);
    }

    int daily_xp, total_xp, level, streak;
    taskGetStats(&daily_xp, &total_xp, &level, &streak);
    Serial.printf("],\"daily_xp\":%d,\"total_xp\":%d,\"level\":%d,\"streak\":%d,\"daily_done\":%d}}\n",
                  daily_xp, total_xp, level, streak, taskGetDailyDone());
    Serial.flush();
}

static void handle_task_add(const JsonDocument & doc, int rid)
{
    const char * text     = doc["text"]     | "";
    bool         repeat   = doc["repeat"]   | false;
    const char * due_date = doc["due_date"] | "";

    if (strlen(text) == 0) { send_resp(rid, false, "text_required"); return; }

    bool ok = taskAdd(text, repeat, due_date);
    send_resp(rid, ok, ok ? nullptr : "task_list_full");
}

static void handle_task_complete(const JsonDocument & doc, int rid)
{
    uint32_t id = doc["id"] | 0u;
    bool ok = taskComplete(id);
    send_resp(rid, ok, ok ? nullptr : "not_found");
}

static void handle_task_delete(const JsonDocument & doc, int rid)
{
    uint32_t id = doc["id"] | 0u;
    bool ok = taskDelete(id);
    send_resp(rid, ok, ok ? nullptr : "not_found");
}

static void handle_cal_list(int rid)
{
    Serial.printf("{\"event\":\"resp\",\"rid\":%d,\"ok\":true,\"items\":[", rid);

    int count = calendarGetCount();
    for (int i = 0; i < count; i++) {
        const event_t * e = calendarGet(i);
        if (!e) continue;
        if (i > 0) Serial.print(',');

        JsonDocument j;
        j["id"]         = (uint32_t)e->id;
        j["title"]      = e->title;
        j["date"]       = e->date;
        j["start_time"] = e->start_time;
        j["end_time"]   = e->end_time;
        j["all_day"]    = e->all_day;
        serializeJson(j, Serial);
    }

    Serial.print("]}\n");
    Serial.flush();
}

static void handle_cal_add(const JsonDocument & doc, int rid)
{
    const char * title      = doc["title"]      | "";
    const char * date       = doc["date"]       | "";
    const char * start_time = doc["start_time"] | "";
    const char * end_time   = doc["end_time"]   | "";
    bool all_day            = doc["all_day"]    | false;

    if (strlen(title) == 0 || strlen(date) == 0) {
        send_resp(rid, false, "title_date_required");
        return;
    }

    bool ok = calendarAddEvent(title, date, start_time, end_time, all_day);
    send_resp(rid, ok, ok ? nullptr : "calendar_full");
}

static void handle_cal_delete(const JsonDocument & doc, int rid)
{
    uint32_t id = doc["id"] | 0u;
    bool ok = calendarDeleteEvent(id);
    send_resp(rid, ok, ok ? nullptr : "not_found");
}

// ── Public API ─────────────────────────────────────────────────────────────────

bool serialFsDispatch(const JsonDocument & doc)
{
    const char * cmd = doc["cmd"] | "";
    int rid = doc["rid"] | -1;

    if      (strcmp(cmd, "fs_list")       == 0) { handle_fs_list(doc, rid);       return true; }
    else if (strcmp(cmd, "fs_write")      == 0) { handle_fs_write(doc, rid);      return true; }
    else if (strcmp(cmd, "fs_mkdir")      == 0) { handle_fs_mkdir(doc, rid);      return true; }
    else if (strcmp(cmd, "fs_delete")     == 0) { handle_fs_delete(doc, rid);     return true; }
    else if (strcmp(cmd, "fs_download")   == 0) { handle_fs_download(doc, rid);   return true; }
    else if (strcmp(cmd, "upload_chunk")  == 0) { handle_upload_chunk(doc, rid);  return true; }
    else if (strcmp(cmd, "get_settings")  == 0) { handle_get_settings(rid);       return true; }
    else if (strcmp(cmd, "task_list")     == 0) { handle_task_list(rid);          return true; }
    else if (strcmp(cmd, "task_add")      == 0) { handle_task_add(doc, rid);      return true; }
    else if (strcmp(cmd, "task_complete") == 0) { handle_task_complete(doc, rid); return true; }
    else if (strcmp(cmd, "task_delete")   == 0) { handle_task_delete(doc, rid);   return true; }
    else if (strcmp(cmd, "cal_list")      == 0) { handle_cal_list(rid);           return true; }
    else if (strcmp(cmd, "cal_add")       == 0) { handle_cal_add(doc, rid);       return true; }
    else if (strcmp(cmd, "cal_delete")    == 0) { handle_cal_delete(doc, rid);    return true; }

    return false;
}
