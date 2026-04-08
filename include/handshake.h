// Project  : HelpDesk
// File     : handshake.h
// Purpose  : Serial handshake with companion app — device info broadcast and timesync
// Depends  : WiFi.h, sd_card.h, settings.h

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Call once in setup() after WiFi init.  Immediately broadcasts the hello packet.
void handshakeInit(void);

// Call every loop() iteration.  Sends a lightweight status packet every 30 s.
void handleHandshake(unsigned long now_ms);

// Broadcasts the full device hello packet now.
// Also called by pc_monitor when a host_hello event is received.
void handshakeSendHello(void);

// Update the "current screen" string embedded in status packets.
// Screen init functions may call this so the companion knows which app is active.
// name must be a string literal or a buffer that remains valid (≤ 24 chars).
void handshakeSetScreen(const char * name);

#ifdef __cplusplus
} /* extern "C" */
#endif
