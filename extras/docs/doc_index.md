# Documentation Index — Context Shard Map

**Quick reference: which doc answers which question?**

---

## Start Here

| Question | Doc | Section |
|----------|-----|---------|
| How do I work with this project's docs? | `README_ON_EVERY_PROMPT.md` | All |
| What's the current project status? | `project_status.md` | §Feature Modules |
| What hardware is this? | `display_notes.md` | §1 Hardware Summary |
| Where's the working example code? | `README_ON_EVERY_PROMPT.md` | §Reference |

---

## By Topic

### Display & Graphics

| Question | Doc | Section |
|----------|-----|---------|
| What display IC and pins? | `display_notes.md` | §1–2 |
| How is LovyanGFX configured? | `display_notes.md` | §3 |
| Why no `setColorDepth(16)`? | `lessons_learned.md` | §1 |
| How does LVGL integrate? | `display_notes.md` | §5 |
| Display is garbled — help! | `troubleshooting_log.md` | Issue #3 |

**Key file:** `include/LovyanGFX_Driver.h`

### Touch Input

| Question | Doc | Section |
|----------|-----|---------|
| What touch controller? | `display_notes.md` | §1–2 |
| Touch coordinates wrong? | `lessons_learned.md` | §2 |
| How is touch configured? | `display_notes.md` | §3 Touch |
| Touch not working — help! | `troubleshooting_log.md` | Issue #4 |

**Key file:** `include/LovyanGFX_Driver.h` (touch config section)

### Build & Flashing

| Question | Doc | Section |
|----------|-----|---------|
| PlatformIO configuration? | `setup.txt` | All |
| Flash mode (DIO vs QIO)? | `lessons_learned.md` | §3 |
| Boot loop / crash? | `troubleshooting_log.md` | Issue #2 |

**Key file:** `platformio.ini`

### UI Screens & LVGL

| Question | Doc | Section |
|----------|-----|---------|
| What screens exist? | `project_status.md` | §Screens |
| How do I add a screen? | `README_ON_EVERY_PROMPT.md` | Example Workflow |
| LVGL 9 color format? | `lessons_learned.md` | §5 |

**Key files:** `include/ui*.h`, `src/ui*.c`

### Feature Modules

| Question | Doc | Section |
|----------|-----|---------|
| What features are implemented? | `project_status.md` | §Feature Modules |
| How to wire a new module? | Example: `Desktop_Assistant_35/*.cpp` | — |
| WiFi, NTP, weather, etc.? | (Not yet documented) | — |

**Key files:** `src/main.cpp`, example modules in `extras/examples/`

### Troubleshooting

| Question | Doc | Section |
|----------|-----|---------|
| Display shows garbled colors? | `troubleshooting_log.md` | Issue #3 |
| Touch coordinates wrong? | `troubleshooting_log.md` | Issue #4 |
| Black screen on boot? | `troubleshooting_log.md` | Issue #1 |
| Boot loop / crash? | `troubleshooting_log.md` | Issue #2 |
| General debugging tips? | `lessons_learned.md` | §10, Summary |

### Best Practices & Lessons

| Question | Doc | Section |
|----------|-----|---------|
| Key lessons from bring-up? | `lessons_learned.md` | All |
| Why these hardware settings? | `display_notes.md` | §4, §6 |
| Coding style & patterns? | `coding_guidelines.md` | All |

---

## All Documentation Files

| File | Purpose | Size | Last Updated |
|------|---------|------|--------------|
| `doc_index.md` | This file — topic → doc map | Small | 2026-04-07 |
| `README_ON_EVERY_PROMPT.md` | Documentation philosophy & workflow | Medium | 2026-04-07 |
| `display_notes.md` | Display hardware, LovyanGFX, LVGL | Large | 2026-04-07 |
| `troubleshooting_log.md` | Bug history & fixes | Medium | 2026-04-07 |
| `lessons_learned.md` | Key insights & best practices | Large | 2026-04-07 |
| `project_status.md` | Current state, features, roadmap | Medium | 2026-04-07 |
| `coding_guidelines.md` | Code style, naming, patterns | Small | (old) |
| `setup.txt` | PlatformIO config, build notes | Small | (old) |
| `design.txt` | Original design notes | Small | (old) |
| `update_lvgl.txt` | LVGL upgrade notes | Small | (old) |

---

## When to Read What

### Before Modifying Display Code
1. `display_notes.md` (§3 LovyanGFX config)
2. `lessons_learned.md` (§1 color depth, §6 pushImage)
3. `include/LovyanGFX_Driver.h` (reference)

### Before Modifying Touch Code
1. `display_notes.md` (§3 Touch config, §6 Touch mapping)
2. `lessons_learned.md` (§2 touch coordinates)
3. `include/LovyanGFX_Driver.h` (reference)

### Before Adding a Feature Module
1. `project_status.md` (§Feature Modules)
2. `extras/examples/Desktop_Assistant_35/*.cpp` (reference impl)
3. `src/main.cpp` (wiring pattern)

### When You Hit a Bug
1. `troubleshooting_log.md` (has it been seen before?)
2. Relevant subsystem doc (`display_notes.md`, etc.)
3. `lessons_learned.md` (related lessons)

### When Starting Fresh (New Contributor)
1. `README_ON_EVERY_PROMPT.md` (workflow)
2. `project_status.md` (current state)
3. `lessons_learned.md` (avoid known pitfalls)

---

## Maintenance

**When creating a new doc:**
1. Add it to the "All Documentation Files" table above
2. Add topic entries to "By Topic" section
3. Keep it focused (one topic per file)

**When splitting a doc:**
1. Create new file(s)
2. Update this index with new entries
3. Update `README_ON_EVERY_PROMPT.md` if it references the old structure

**When refactoring:**
- Keep this index up-to-date
- Old docs can be moved to `extras/docs/archive/` if superseded

---

**Last Updated:** April 7, 2026
