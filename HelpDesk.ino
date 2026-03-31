// Project  : HelpDesk
// File     : HelpDesk.ino
// Purpose  : Main Arduino sketch — initialise hardware and run the event loop
// Depends  : display_Screen.h, wifi_connect.h, get_time.h, weather.h, ui.h

#include <Arduino.h>
#include <lvgl.h>
#include "ui.h"

#include "display_Screen.h"
#include "wifi_connect.h"

// ─── Forward declarations for feature modules (add headers as you build them) ─
// #include "get_time.h"
// #include "weather.h"
// #include "pomo_timer.h"
// #include "juke_desk.h"
// #include "pc_monitor.h"

// ─── Update intervals ──────────────────────────────────────────────────────────
#define NTP_SYNC_INTERVAL_MS     600000UL   // 10 minutes
#define WEATHER_UPDATE_INTERVAL_MS 600000UL // 10 minutes

void setup()
{
    Serial.begin(115200);

    // 1. Hardware: display + touch + LVGL must come first
    initDisplay();

    // 2. UI: build the launcher screen
    ui_init();
    Serial.println("[HelpDesk] Display and UI ready.");

    // 3. Network: connect once; feature modules rely on this
    connectToWiFi();
    Serial.println("[HelpDesk] WiFi connected.");

    // 4. Feature module init — uncomment each as the module is implemented
    // initNTP();
    // getTimeData();
    // getWeatherData();
    // audioInit();
    // initPomoTimer();
    // pcMonitorInit();

    // Force the first frame to render immediately rather than waiting for loop()
    lv_refr_now(NULL);

    Serial.println("[HelpDesk] Init complete. Entering loop.");
}

void loop()
{
    // Let LVGL process animations, touch events, and redraws
    lv_timer_handler();

    unsigned long now = millis();

    // Periodic feature updates — uncomment each as the module is implemented
    // handleTimeUpdate(now);
    // handleNTPUpdate(now);
    // handleWeatherUpdate(now);
    // audioLoop();
    // handlePomoTimer(now);
    // handlePcMonitor(now);

    // 5 ms yield keeps the LVGL timer accurate without blocking touch input
    delay(5);
}
