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
#include "zen_frame.h"
#include "task_master.h"
#include "calendar.h"
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
        /* Refresh ZenFrame playlist after a new image lands on the SD card. */
        if (s_upload_target_dir && strcmp(s_upload_target_dir, "/images") == 0) {
            zenFrameRescan();
        }
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

// ── Route: GET /settings ────────────────────────────────────────────────────

static void handle_settings_get()
{
    JsonDocument doc;
    doc["wifi_ssid"]     = settingsGetWifiSSID();
    doc["wifi_password"] = settingsGetWifiPassword();
    doc["owm_api_key"]   = settingsGetOwmKey();
    doc["zip_code"]      = settingsGetOwmCity();
    doc["units"]         = settingsGetOwmUnits();
    doc["companion_ip"]  = settingsGetCompanionIP();

    String out;
    serializeJson(doc, out);
    s_server.send(200, "application/json", out);
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
    if (doc["companion_ip"].is<const char*>())  settingsSetCompanionIP(doc["companion_ip"]);

    settingsSave();
    Serial.println("[HTTP] Settings updated from companion app.");
    s_server.send(200, "application/json", "{\"ok\":true}");
}

// ── Route: GET /tasks — return all tasks as JSON ──────────────────────────────

static void handle_tasks_get()
{
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    int count = taskGetCount();
    for (int i = 0; i < count; i++) {
        const task_t *t = taskGet(i);
        if (!t) continue;
        JsonObject o = arr.add<JsonObject>();
        o["id"]       = t->id;
        o["text"]     = t->text;
        o["due_date"] = t->due_date;
        o["repeat"]   = t->repeat;
        o["done"]     = t->done_today;
    }

    int daily_xp, total_xp, level, streak;
    taskGetStats(&daily_xp, &total_xp, &level, &streak);

    JsonDocument resp;
    resp["tasks"]     = arr;
    resp["daily_xp"]  = daily_xp;
    resp["total_xp"]  = total_xp;
    resp["level"]     = level;
    resp["streak"]    = streak;
    resp["daily_done"] = taskGetDailyDone();

    String out;
    serializeJson(resp, out);
    s_server.send(200, "application/json", out);
}

// ── Route: POST /tasks/add ────────────────────────────────────────────────────

static void handle_tasks_add()
{
    String body = s_server.arg("plain");
    if (body.isEmpty()) {
        s_server.send(400, "application/json", "{\"error\":\"empty body\"}");
        return;
    }
    JsonDocument doc;
    if (deserializeJson(doc, body)) {
        s_server.send(400, "application/json", "{\"error\":\"bad json\"}");
        return;
    }
    const char *text     = doc["text"]     | "";
    bool repeat           = doc["repeat"]   | false;
    const char *due_date  = doc["due_date"] | "";

    if (strlen(text) == 0) {
        s_server.send(400, "application/json", "{\"error\":\"text required\"}");
        return;
    }
    if (!taskAdd(text, repeat, due_date)) {
        s_server.send(507, "application/json", "{\"error\":\"task list full\"}");
        return;
    }
    s_server.send(200, "application/json", "{\"ok\":true}");
}

// ── Route: POST /tasks/complete ───────────────────────────────────────────────

static void handle_tasks_complete()
{
    String body = s_server.arg("plain");
    JsonDocument doc;
    if (body.isEmpty() || deserializeJson(doc, body)) {
        s_server.send(400, "application/json", "{\"error\":\"bad json\"}");
        return;
    }
    uint32_t id = doc["id"] | 0u;
    if (!taskComplete(id)) {
        s_server.send(404, "application/json", "{\"error\":\"not found or already done\"}");
        return;
    }
    s_server.send(200, "application/json", "{\"ok\":true}");
}

// ── Route: POST /tasks/delete ─────────────────────────────────────────────────

static void handle_tasks_delete()
{
    String body = s_server.arg("plain");
    JsonDocument doc;
    if (body.isEmpty() || deserializeJson(doc, body)) {
        s_server.send(400, "application/json", "{\"error\":\"bad json\"}");
        return;
    }
    uint32_t id = doc["id"] | 0u;
    if (!taskDelete(id)) {
        s_server.send(404, "application/json", "{\"error\":\"not found\"}");
        return;
    }
    s_server.send(200, "application/json", "{\"ok\":true}");
}

// ── DeskDrive routes (/api/fs/*) ──────────────────────────────────────────────

/* Validates an SD card path: must start with '/', no '..' components.
   Returns true and writes normalised path into out if safe. */
static bool safe_sd_path(const char * path, char * out, size_t out_sz)
{
    if (!path || path[0] != '/') return false;
    /* Reject any '..' component */
    for (const char * p = path; *p; p++) {
        if (p[0] == '.' && p[1] == '.' && (p[2] == '/' || p[2] == '\0')) return false;
    }
    strncpy(out, path, out_sz - 1);
    out[out_sz - 1] = '\0';
    return true;
}

/* GET /api/fs/list?dir=/path — list directory as JSON array */
static void handle_fs_list()
{
    if (!sdCardMounted()) {
        s_server.send(503, "application/json", "{\"error\":\"SD not mounted\"}");
        return;
    }
    char dir_path[128] = "/";
    String dir_arg = s_server.arg("dir");
    if (!dir_arg.isEmpty() && !safe_sd_path(dir_arg.c_str(), dir_path, sizeof(dir_path))) {
        s_server.send(400, "application/json", "{\"error\":\"invalid path\"}");
        return;
    }

    File dir = SD.open(dir_path);
    if (!dir || !dir.isDirectory()) {
        s_server.send(404, "application/json", "{\"error\":\"not a directory\"}");
        return;
    }

    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    dir.rewindDirectory();
    while (true) {
        File entry = dir.openNextFile();
        if (!entry) break;
        const char * name = entry.name();
        if (!name || name[0] == '\0' || name[0] == '.') { entry.close(); continue; }
        JsonObject o = arr.add<JsonObject>();
        o["name"]   = name;
        o["is_dir"] = entry.isDirectory();
        if (!entry.isDirectory()) o["size"] = (int)entry.size();
        entry.close();
        if (arr.size() >= 200) break;   /* safety cap */
    }
    dir.close();

    String out;
    serializeJson(doc, out);
    s_server.send(200, "application/json", out);
}

/* GET /api/fs/download?path=/file.txt — stream a file */
static void handle_fs_download()
{
    if (!sdCardMounted()) {
        s_server.send(503, "application/json", "{\"error\":\"SD not mounted\"}");
        return;
    }
    char file_path[128];
    if (!safe_sd_path(s_server.arg("path").c_str(), file_path, sizeof(file_path))) {
        s_server.send(400, "application/json", "{\"error\":\"invalid path\"}");
        return;
    }
    File f = SD.open(file_path, FILE_READ);
    if (!f || f.isDirectory()) {
        s_server.send(404, "application/json", "{\"error\":\"file not found\"}");
        return;
    }
    /* Determine MIME type from extension */
    const char * ext  = strrchr(file_path, '.');
    const char * mime = "application/octet-stream";
    if (ext) {
        if (!strcasecmp(ext,".txt") || !strcasecmp(ext,".md") || !strcasecmp(ext,".json"))
            mime = "text/plain";
        else if (!strcasecmp(ext,".jpg") || !strcasecmp(ext,".jpeg")) mime = "image/jpeg";
        else if (!strcasecmp(ext,".png"))  mime = "image/png";
        else if (!strcasecmp(ext,".bmp"))  mime = "image/bmp";
        else if (!strcasecmp(ext,".mp3"))  mime = "audio/mpeg";
    }
    s_server.streamFile(f, mime);
    f.close();
}

/* Multipart upload handlers for POST /api/fs/upload?dir=/path */
static void handle_fs_upload_file()
{
    if (!sdCardMounted()) return;
    HTTPUpload & upload = s_server.upload();

    if (upload.status == UPLOAD_FILE_START) {
        s_upload_ok = false;
        char dir_path[96] = "/";
        String dir_arg = s_server.arg("dir");
        if (!dir_arg.isEmpty()) safe_sd_path(dir_arg.c_str(), dir_path, sizeof(dir_path));
        sdEnsureDir(dir_path);

        char basename[64];
        safe_basename(upload.filename.c_str(), basename, sizeof(basename));
        snprintf(s_upload_filepath, sizeof(s_upload_filepath), "%s/%s", dir_path, basename);

        if (SD.exists(s_upload_filepath)) SD.remove(s_upload_filepath);
        s_upload_file = SD.open(s_upload_filepath, FILE_WRITE);
        if (s_upload_file) {
            s_upload_ok = true;
            Serial.printf("[HTTP] DeskDrive upload: %s\n", s_upload_filepath);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (s_upload_file) s_upload_file.write(upload.buf, upload.currentSize);
    } else if (upload.status == UPLOAD_FILE_END) {
        if (s_upload_file) s_upload_file.close();
    }
}

static void handle_fs_upload_done()
{
    if (s_upload_ok) {
        zenFrameRescan();   /* refresh ZenFrame if an image landed */
        s_server.send(200, "application/json", "{\"ok\":true}");
    } else {
        s_server.send(500, "application/json", "{\"error\":\"write_failed\"}");
    }
    s_upload_ok = false;
}

/* POST /api/fs/mkdir — body: {"path":"/newdir"} */
static void handle_fs_mkdir()
{
    if (!sdCardMounted()) {
        s_server.send(503, "application/json", "{\"error\":\"SD not mounted\"}");
        return;
    }
    JsonDocument doc;
    if (s_server.arg("plain").isEmpty() || deserializeJson(doc, s_server.arg("plain"))) {
        s_server.send(400, "application/json", "{\"error\":\"bad json\"}");
        return;
    }
    char safe_path[128];
    const char * raw = doc["path"] | "";
    if (!safe_sd_path(raw, safe_path, sizeof(safe_path)) || safe_path[0] == '\0') {
        s_server.send(400, "application/json", "{\"error\":\"invalid path\"}");
        return;
    }
    if (SD.exists(safe_path)) {
        s_server.send(200, "application/json", "{\"ok\":true,\"existed\":true}");
        return;
    }
    bool created = SD.mkdir(safe_path);
    s_server.send(created ? 200 : 500, "application/json",
                  created ? "{\"ok\":true}" : "{\"error\":\"mkdir failed\"}");
}

/* POST /api/fs/delete — body: {"path":"/file.txt"} */
static void handle_fs_delete()
{
    if (!sdCardMounted()) {
        s_server.send(503, "application/json", "{\"error\":\"SD not mounted\"}");
        return;
    }
    JsonDocument doc;
    if (s_server.arg("plain").isEmpty() || deserializeJson(doc, s_server.arg("plain"))) {
        s_server.send(400, "application/json", "{\"error\":\"bad json\"}");
        return;
    }
    char safe_path[128];
    const char * raw = doc["path"] | "";
    if (!safe_sd_path(raw, safe_path, sizeof(safe_path)) || strcmp(safe_path, "/") == 0) {
        s_server.send(400, "application/json", "{\"error\":\"invalid path\"}");
        return;
    }
    if (!SD.exists(safe_path)) {
        s_server.send(404, "application/json", "{\"error\":\"not found\"}");
        return;
    }
    File f = SD.open(safe_path);
    bool is_dir = f.isDirectory();
    f.close();
    bool ok = is_dir ? SD.rmdir(safe_path) : SD.remove(safe_path);
    s_server.send(ok ? 200 : 500, "application/json",
                  ok ? "{\"ok\":true}" : "{\"error\":\"delete failed\"}");
}

/* POST /api/fs/write — body: {"path":"/file.txt","content":"..."} — write/overwrite a text file */
static void handle_fs_write()
{
    if (!sdCardMounted()) {
        s_server.send(503, "application/json", "{\"error\":\"SD not mounted\"}");
        return;
    }
    JsonDocument doc;
    if (s_server.arg("plain").isEmpty() || deserializeJson(doc, s_server.arg("plain"))) {
        s_server.send(400, "application/json", "{\"error\":\"bad json\"}");
        return;
    }
    char safe_path[128];
    const char * raw = doc["path"] | "";
    if (!safe_sd_path(raw, safe_path, sizeof(safe_path)) || safe_path[0] == '\0') {
        s_server.send(400, "application/json", "{\"error\":\"invalid path\"}");
        return;
    }
    const char * content = doc["content"] | "";
    if (SD.exists(safe_path)) SD.remove(safe_path);
    File f = SD.open(safe_path, FILE_WRITE);
    if (!f) {
        s_server.send(500, "application/json", "{\"error\":\"write failed\"}");
        return;
    }
    f.print(content);
    f.close();
    s_server.send(200, "application/json", "{\"ok\":true}");
}

// ── Calendar routes (/calendar, /calendar/add, /calendar/delete) ──────────────

static void handle_calendar_get()
{
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    int count = calendarGetCount();
    for (int i = 0; i < count; i++) {
        const event_t *e = calendarGet(i);
        if (!e) continue;
        JsonObject o = arr.add<JsonObject>();
        o["id"]         = e->id;
        o["title"]      = e->title;
        o["date"]       = e->date;
        o["start_time"] = e->start_time;
        o["end_time"]   = e->end_time;
        o["all_day"]    = e->all_day;
    }
    String out;
    serializeJson(doc, out);
    s_server.send(200, "application/json", out);
}

static void handle_calendar_add()
{
    String body = s_server.arg("plain");
    if (body.isEmpty()) {
        s_server.send(400, "application/json", "{\"error\":\"empty body\"}");
        return;
    }
    JsonDocument doc;
    if (deserializeJson(doc, body)) {
        s_server.send(400, "application/json", "{\"error\":\"bad json\"}");
        return;
    }
    const char *title      = doc["title"]      | "";
    const char *date       = doc["date"]       | "";
    const char *start_time = doc["start_time"] | "";
    const char *end_time   = doc["end_time"]   | "";
    bool all_day           = doc["all_day"]    | false;

    if (strlen(title) == 0 || strlen(date) == 0) {
        s_server.send(400, "application/json", "{\"error\":\"title and date required\"}");
        return;
    }
    if (!calendarAddEvent(title, date, start_time, end_time, all_day)) {
        s_server.send(507, "application/json", "{\"error\":\"calendar full\"}");
        return;
    }
    s_server.send(200, "application/json", "{\"ok\":true}");
}

static void handle_calendar_delete()
{
    String body = s_server.arg("plain");
    JsonDocument doc;
    if (body.isEmpty() || deserializeJson(doc, body)) {
        s_server.send(400, "application/json", "{\"error\":\"bad json\"}");
        return;
    }
    uint32_t id = doc["id"] | 0u;
    if (!calendarDeleteEvent(id)) {
        s_server.send(404, "application/json", "{\"error\":\"not found\"}");
        return;
    }
    s_server.send(200, "application/json", "{\"ok\":true}");
}

// ── Public API ────────────────────────────────────────────────────────────────

void httpServerInit(void)
{
    s_server.on("/status",          HTTP_GET,  handle_status);
    s_server.on("/upload/image",    HTTP_POST, handle_upload_done, handle_file_upload);
    s_server.on("/upload/audio",    HTTP_POST, handle_upload_done, handle_file_upload);
    s_server.on("/settings",        HTTP_GET,  handle_settings_get);
    s_server.on("/settings",        HTTP_POST, handle_settings_post);
    s_server.on("/tasks",           HTTP_GET,  handle_tasks_get);
    s_server.on("/tasks/add",       HTTP_POST, handle_tasks_add);
    s_server.on("/tasks/complete",  HTTP_POST, handle_tasks_complete);
    s_server.on("/tasks/delete",    HTTP_POST, handle_tasks_delete);
    s_server.on("/calendar",        HTTP_GET,  handle_calendar_get);
    s_server.on("/calendar/add",    HTTP_POST, handle_calendar_add);
    s_server.on("/calendar/delete", HTTP_POST, handle_calendar_delete);
    /* DeskDrive file-system API */
    s_server.on("/api/fs/list",     HTTP_GET,  handle_fs_list);
    s_server.on("/api/fs/download", HTTP_GET,  handle_fs_download);
    s_server.on("/api/fs/upload",   HTTP_POST, handle_fs_upload_done, handle_fs_upload_file);
    s_server.on("/api/fs/mkdir",    HTTP_POST, handle_fs_mkdir);
    s_server.on("/api/fs/delete",   HTTP_POST, handle_fs_delete);
    s_server.on("/api/fs/write",    HTTP_POST, handle_fs_write);

    s_server.begin();
    Serial.printf("[HTTP] Server started. Visit http://%s/status\n",
                  WiFi.localIP().toString().c_str());
}

void httpServerLoop(void)
{
    s_server.handleClient();
}
