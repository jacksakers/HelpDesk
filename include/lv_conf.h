/*
 * lv_conf.h — LVGL 9.x configuration for HelpDesk
 *
 * Based on lv_conf_template.h from LVGL 9.2.x.
 * Only the settings that differ from defaults are listed here.
 * Everything else inherits the LVGL built-in defaults.
 *
 * If you get compile errors after an LVGL update, grab a fresh
 * lv_conf_template.h from the lvgl repo, copy it here, set
 * the #if 0 → #if 1, and re-apply the settings below.
 */

#if 1  /* Set to 1 to enable this config file */

#ifndef LV_CONF_H
#define LV_CONF_H

/* ── Colour depth (must match SquareLine Studio export) ───── */
#define LV_COLOR_DEPTH 16

/* ── Memory ───────────────────────────────────────────────── */
/* Use stdlib malloc — simpler than the built-in pool on ESP32 */
#define LV_USE_STDLIB_MALLOC    LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING    LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_CLIB
#define LV_MEM_SIZE             (48 * 1024)

/* ── Display ──────────────────────────────────────────────── */
#define LV_USE_OS               LV_OS_NONE
#define LV_DEF_REFR_PERIOD      33      /* ~30 fps */

/* ── Logging (enable during development, disable for release) */
#define LV_USE_LOG              0

/* ── Fonts ─────────────────────────────────────────────────── */
#define LV_FONT_MONTSERRAT_14   1       /* default font */
/* Uncomment to enable larger fonts for the clock display:     */
/* #define LV_FONT_MONTSERRAT_28  1 */
/* #define LV_FONT_MONTSERRAT_48  1 */

/* ── Widgets ───────────────────────────────────────────────── */
/* All core widgets are enabled by default in LVGL 9.          */
/* Disable any you don't need to save flash:                   */
/* #define LV_USE_CALENDAR     0 */
/* #define LV_USE_CHART        0 */

/* ── Themes ────────────────────────────────────────────────── */
#define LV_USE_THEME_DEFAULT    1

#endif /* LV_CONF_H */
#endif /* #if 1 */
