// Project  : HelpDesk
// File     : http_server.h
// Purpose  : Lightweight HTTP server for companion-app file uploads and settings sync
// Depends  : <WebServer.h> (ESP32 Arduino core)

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Start the HTTP server.  Call once after connectToWiFi() succeeds.
// Registers all route handlers and begins listening on port 80.
void httpServerInit(void);

// Process pending HTTP requests.  Must be called in loop() on every iteration.
// Each call is non-blocking unless a client is actively transferring data.
void httpServerLoop(void);

#ifdef __cplusplus
} /* extern "C" */
#endif
