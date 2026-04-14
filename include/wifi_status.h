// Project  : HelpDesk
// File     : wifi_status.h
// Purpose  : C-compatible WiFi status API (no C++ dependencies)
// Depends  : (none — safe to include from .c files)

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Returns true while WiFi.status() == WL_CONNECTED.                         */
bool wifiIsConnected(void);

/* Call once per loop() iteration. Logs connect/disconnect transitions and
   tracks whether the HTTP server has been started.  The caller is responsible
   for calling httpServerInit() when this function returns true for the first
   time; check wifiJustConnected() for that purpose.                          */
void wifiHandleConnection(unsigned long now);

/* Returns true exactly once — the first loop tick after WiFi connects.
   Use this to trigger one-time post-connection initialisation (e.g. httpServerInit). */
bool wifiJustConnected(void);

#ifdef __cplusplus
}
#endif
