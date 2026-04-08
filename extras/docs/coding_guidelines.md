# HelpDesk — Coding Guidelines

These rules exist to keep the codebase approachable for a solo developer returning after months away.
Prioritise readability and maintainability over cleverness.

---

## 1. One Responsibility Per File

Every `.c/.cpp` source file has one job.  Name the file after that job.

| File | Responsibility |
|------|---------------|
| `ui_Screen1.c` | Build and destroy the Launcher screen |
| `ui_Screen3.c` | Build and destroy the Tomatimer screen |
| `weather.cpp` | Fetch and parse weather data from the API |
| `get_Time.cpp` | NTP synchronisation and time formatting |
| `timer.cpp` | Stopwatch state machine only |

Do **not** mix unrelated logic in one file (e.g. no Wi-Fi code inside a screen file).


### 1.a. Keep Things DRY!

If there is some logic that is going to be reused and potentially repeated in many modules, add it to a utils file. All logic should only exist once in the codebase and should not be repeated. 

---

## 2. One Responsibility Per Function

A function does one thing.  If you need a comment to explain *what* it does (not *why*),
the function should be split.

**Good:**
```c
static void build_header_bar(lv_obj_t * parent, const char * title);
static void build_tile_grid(lv_obj_t * parent);
```

**Bad:**
```c
void ui_Screen1_screen_init(void) {
    // ... 200 lines of mixed header, grid, and navigation logic ...
}
```

---

## 3. Keep Functions Short

Target ≤ 40 lines per function body.  Factor logical sub-steps into clearly named
`static` helper functions within the same translation unit.

---

## 4. No Magic Numbers

Use `#define` or `const` for every numeric literal that carries meaning.

**Good:**
```c
#define TILE_W    71
#define TILE_H    94
#define TILE_GAP   6
```

**Bad:**
```c
lv_obj_set_size(tile, 71, 94);    /* what does 71×94 mean? */
```

---

## 5. Separate Interface from Implementation

Every module has a `.h` header and a `.c/.cpp` source.

- The header exposes the **minimum** public API.  Keep internal helpers `static`.
- Never `#include` a `.c` / `.cpp` file.
- Do not put executable code in a header.

---

## 6. Include Only What You Use

Only include headers your file actually needs.  Rely on transitive includes deliberately
where documented — e.g. every UI screen file includes `"ui.h"`, which in turn brings in
LVGL and all shared helpers.

---

## 7. Limit Global State

Globals make code hard to reason about.

- Each screen file owns **one** global: its root `lv_obj_t *` (e.g. `lv_obj_t * ui_Screen1`).
- Key widget handles that other modules need to update (e.g. `ui_TimeLabel`) are allowed
  as additional globals in the owning screen file, declared `extern` in its header.
- All other state is `static` within the translation unit.
- Shared mutable state (e.g. "current track index") lives in the module that owns it
  and is accessed via getter/setter functions — never as a raw global.

---

## 8. Screen / Module Lifecycle Pattern

Every UI screen follows the same init / destroy contract:

```c
void ui_ScreenX_screen_init(void);    /* creates all widgets; sets global pointer */
void ui_ScreenX_screen_destroy(void); /* deletes root object; sets global to NULL  */
```

Register the destroy callback at creation time so memory is reclaimed automatically when
LVGL unloads the screen:

```c
lv_obj_add_event_cb(ui_ScreenX, scr_unloaded_delete_cb,
                    LV_EVENT_SCREEN_UNLOADED, ui_ScreenX_screen_destroy);
```

---

## 9. Naming Conventions

| Category | Convention | Example |
|----------|-----------|---------|
| LVGL screen root object | `ui_ScreenN` | `ui_Screen1` |
| Screen init / destroy | `ui_ScreenN_screen_init/destroy` | `ui_Screen3_screen_init` |
| Exposed widget handles | `ui_<DescriptiveName>` | `ui_TimeLabel`, `ui_PomoArc` |
| App-module init / loop | `camelCase` verbs | `initNTP()`, `handleTimer()` |
| Constants / macros | `UPPER_SNAKE_CASE` | `TILE_W`, `POMO_WORK_SECS` |
| Private helpers | `static` + `snake_case` | `static void build_header(...)` |
| Event callbacks | suffix `_ev` | `static void tile_clicked_ev(...)` |

---

## 10. Comment the *Why*, Not the *What*

Write comments that explain non-obvious decisions, not what the code obviously does.

**Good:**
```c
/* RTC is queried first; NTP sync can take several seconds on cold boot. */
display_time(rtc_get_time());
```

**Bad:**
```c
/* Call rtc_get_time to get the time. */
display_time(rtc_get_time());
```

---

## 11. Prefer Explicit Over Clever

- Avoid macros that generate function bodies.
- Prefer a clear `switch` statement over chained ternary operators.
- Prefer explicit repetition that can be read linearly over clever code that must be
  mentally expanded.

---

## 12. Validate Only at System Boundaries

Add input validation where untrusted data enters the system:
- JSON responses from the weather / NTP API.
- Data arriving over UART from the PC companion script.
- File contents read from the SD card.

Do **not** add null-checks to internal functions where `NULL` is not a legal value;
that masks real bugs instead of surfacing them.

---

## 13. File Header Block

Every source file begins with a four-line header comment:

```c
// Project  : HelpDesk
// File     : ui_Screen3.c
// Purpose  : Tomatimer screen — Pomodoro timer placeholder layout
// Depends  : ui.h (LVGL 8.3.11)
```

---

## 14. App Module Structure (Arduino `.cpp` files)

Each feature module exposes:
- `void featureInit(void)`  — called once in `setup()`
- `void featureLoop(void)`  — called every `loop()` iteration (must be non-blocking)
- `void handleFeature(unsigned long ms)` — time-based update (debounced by interval check)

Never call `delay()` inside a module function; it blocks the LVGL timer handler and makes
the UI unresponsive.
