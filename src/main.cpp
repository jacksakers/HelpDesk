// Project  : HelpDesk
// File     : HelpDesk.ino
// Purpose  : Main Arduino sketch — initialise hardware and run the event loop
// Depends  : display_Screen.h, wifi_connect.h, get_time.h, weather.h, pomo_timer.h, ui.h

#include <Arduino.h>
#include <lvgl.h>
#include "ui.h"

#include "display_Screen.h"
#include "wifi_connect.h"
#include "wifi_status.h"
#include "buzzer.h"
#include "sd_card.h"
#include "settings.h"
#include "zen_frame.h"
#include "get_time.h"
#include "weather.h"
#include "pomo_timer.h"
#include "http_server.h"
#include "pc_monitor.h"
#include "handshake.h"
#include "notifications.h"
#include "task_master.h"
#include "voice_input.h"
#include "calendar.h"

// ─── Update intervals ──────────────────────────────────────────────────────────
#define NTP_SYNC_INTERVAL_MS     600000UL   // 10 minutes
#define WEATHER_UPDATE_INTERVAL_MS 600000UL // 10 minutes

void setup()
{
    Serial.begin(115200);
    delay(500);   // brief settle time for UART

    Serial.println();
    Serial.println("===================================");
    Serial.println("===== HelpDesk starting =====");
    Serial.println("===================================");
    Serial.printf("Free heap : %u bytes\n", ESP.getFreeHeap());
    Serial.printf("Free PSRAM: %u bytes\n", ESP.getFreePsram());
    Serial.printf("SDK       : %s\n", ESP.getSdkVersion());
    Serial.flush();

    // 1. Hardware: display + touch + LVGL must come first
    Serial.println("[setup] Calling initDisplay...");
    Serial.flush();
    initDisplay();

    // 1b. Buzzer: init after display so LEDC is ready
    buzzerInit();
    Serial.println("[setup] Buzzer ready (GPIO 8)");
    Serial.println("[setup] initDisplay done");
    Serial.flush();

    // 1c. SD card: mount before settings and zen_frame need it
    sdCardInit();

    // 1d. Settings: load persisted values and apply to subsystems (buzzer etc.)
    settingsInit();

    // 2. UI: build the launcher screen
    ui_init();
    Serial.println("[HelpDesk] Display and UI ready.");

    // 2b. ZenFrame: scan /images playlist (ui_ZenImage may be NULL until Screen6 opens)
    zenFrameInit();

    // 3. Network: start connection in background; UI is already visible
    connectToWiFi();

    // 4. Feature module init
    initNTP();
    initPomoTimer();
    initPcMonitor();
    notifInit();
    taskMasterInit();
    calendarInit();
    voiceInputInit();

    // 5. Handshake — fires immediately (sends IP 0.0.0.0 if offline; companion ignores it)
    handshakeInit();
    // HTTP server is deferred to loop() — started there once WiFi comes up

    Serial.println("[HelpDesk] Init complete. Entering loop.");
}

void loop()
{
    // Let LVGL process animations, touch events, and redraws
    lv_timer_handler();

    // Advance non-blocking multi-note buzzer sequences
    buzzerLoop();

    unsigned long now = millis();

    // // ── Tick watchdog: print every 5 s so we know lv_timer_handler is running
    // static unsigned long last_tick_log = 0;
    // static uint32_t handler_calls = 0;
    // handler_calls++;
    // if(now - last_tick_log >= 5000UL) {
    //     Serial.printf("[loop] calls=%lu  lv_tick=%lu ms  heap=%u  psram=%u\n",
    //                   handler_calls, lv_tick_get(),
    //                   ESP.getFreeHeap(), ESP.getFreePsram());
    //     last_tick_log = now;
    // }

    // Periodic feature updates
    handleTimeUpdate(now);
    handleWeatherUpdate(now);
    handlePomoTimer(now);
    handleZenFrame(now);
    handlePcMonitor(now);
    handleHandshake(now);
    handleNotifications(now);
    handleVoiceInput(now);
    wifiHandleConnection(now);

    // Start HTTP server once (the first loop tick after WiFi connects)
    static bool s_http_started = false;
    if (!s_http_started && wifiJustConnected()) {
        httpServerInit();
        s_http_started = true;
    }

    httpServerLoop();

    // 5 ms yield keeps the LVGL timer accurate without blocking touch input
    delay(5);
}
