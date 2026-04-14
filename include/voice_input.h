// Project  : HelpDesk
// File     : voice_input.h
// Purpose  : Microphone recording + companion-app transcription public interface
// Depends  : settings.h (for companion_ip), WiFi, ESP_I2S

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* ── Mic hardware (Elecrow CrowPanel Advance 3.5" — verify against schematic)
 *   These match the I2S mic pins found in the factory HMI3-5 reference firmware.
 *   If transcription returns silent audio, check these with a logic analyser.
 * ───────────────────────────────────────────────────────────────────────────── */
#define MIC_I2S_BCLK   9    /* Bit clock                    — TODO: hw verify */
#define MIC_I2S_WS     3    /* Word select / LR clock        — TODO: hw verify */
#define MIC_I2S_DATA  10    /* Data in (microphone → ESP32)  — TODO: hw verify */

#define MIC_SAMPLE_RATE    16000   /* 16 kHz — sufficient for speech              */
#define MIC_RECORD_SECS       3   /* Recording window (seconds)                  */

/* ── Lifecycle ────────────────────────────────────────────────────────────── */

/* Call once in setup().  Reads companion_ip from settings for the upload URL. */
void voiceInputInit(void);

/* ── Runtime ─────────────────────────────────────────────────────────────── */

/* Begin a MIC_RECORD_SECS recording session.  No-ops if already recording. */
void voiceInputStart(void);

/* Call every loop() iteration.  Drives the record → upload → result state
   machine without blocking LVGL. */
void handleVoiceInput(unsigned long now_ms);

/* Returns true while recording or uploading. */
bool voiceInputIsActive(void);

/* Returns the transcribed text once ready, or NULL if nothing pending.
   The pointer is valid until the next voiceInputStart(). */
const char *voiceInputGetResult(void);

/* Consume (clear) the pending result after reading it. */
void voiceInputClearResult(void);

/* ── UI result callback ───────────────────────────────────────────────────── */
/* Register a function to be called (in the main-loop context) when a
   transcription result is ready.  Keeps voice_input free of LVGL deps.
   The text pointer is valid only for the duration of the call; copy it. */
typedef void (*voice_result_cb_t)(const char *text);
void voiceInputSetResultCallback(voice_result_cb_t cb);

#ifdef __cplusplus
} /* extern "C" */
#endif
