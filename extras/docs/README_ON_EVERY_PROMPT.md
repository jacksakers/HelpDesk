# Documentation Philosophy — READ THIS FIRST

**Context-driven development with minimal overhead.**

---

## Core Principle

Every code change should update **only the relevant documentation**, keeping each doc **small, focused, and up-to-date**. Think of docs as **context shards** — small, single-purpose files that capture one aspect of the project.

**Goal:** Anyone (including AI assistants) can read 2–3 docs and understand enough context to make a change correctly, without reading the entire codebase.

---

## Rules

### 1. Read Context Before Changing Code

**Before making any change:**
- Identify which subsystem you're modifying (display, touch, WiFi, UI, etc.)
- Read the relevant context doc(s) from `extras/docs/`
- Check `extras/examples/Desktop_Assistant_35/` for working reference code
- READ the coding_guidelines.md and follow the rules

**Don't:** Guess at implementation details. The example code is the ground truth.

### 2. Update Docs After Every Change

**After fixing a bug or adding a feature:**
- Update the **most specific** doc that covers the changed subsystem
- If the change teaches a lesson, add it to `troubleshooting_log.md` or `lessons_learned.md`
- If it's a new feature, update `project_status.md`

**Update only what's necessary.** Don't rewrite entire docs unless refactoring.

### 3. Keep Docs Small and Single-Purpose

Each doc should focus on **one topic**:
- `display_notes.md` — Hardware config, LovyanGFX, LVGL integration
- `troubleshooting_log.md` — Problems encountered and how they were fixed
- `lessons_learned.md` — Key insights and best practices
- `project_status.md` — Current state, feature list, roadmap
- `coding_guidelines.md` — Code style, patterns, conventions

**Don't:** Create kitchen-sink docs that cover everything. **Do:** Split topics into focused files.

### 4. Refactor Docs as They Grow

If a doc exceeds ~300 lines or covers multiple topics:
- Split it into smaller, focused files
- Update `doc_index.md` (the reference map)
- Keep the old file as a redirect or stub if needed

**Example:** If `display_notes.md` grows to cover WiFi, audio, and touch, split into:
- `display_notes.md` (display only)
- `touch_notes.md` (touch only)
- `audio_notes.md` (audio only)

### 5. Limit Documentation Overhead

**Don't spend more time documenting than coding.**

Update docs in this priority order:
1. **Critical path docs** — Things that will cause bugs if misunderstood (hardware config, init sequence)
2. **Troubleshooting** — Lessons from bugs you just fixed
3. **Status tracking** — Feature completion state
4. **Nice-to-have** — API docs, design decisions (only if time permits)

**Stop when the next person (or AI) has enough context to understand your change.**

### 6. Use the Reference Index

`doc_index.md` is the **map** of all context shards. It answers:
- "Where do I find info about X?"
- "Which docs should I read before changing Y?"

**Always update `doc_index.md` when you create or split a doc.**

---

## Documentation Shards (Current)

| Doc                         | Purpose                                      |
|-----------------------------|----------------------------------------------|
| `doc_index.md`              | Master index — maps topics to docs           |
| `display_notes.md`          | LovyanGFX, ILI9488, LVGL, hardware config    |
| `troubleshooting_log.md`    | Bug history and fixes                        |
| `lessons_learned.md`        | Key insights and best practices              |
| `project_status.md`         | Current state, feature roadmap               |
| `coding_guidelines.md`      | Code style, naming, patterns                 |
| `setup.txt`                 | PlatformIO, library versions, build config   |

---

## Example Workflow

### Scenario: You're adding a new screen to the UI

**Step 1: Read context**
- `project_status.md` → see current screen list
- `display_notes.md` (§5 LVGL Integration) → understand screen lifecycle
- `extras/examples/Desktop_Assistant_35/ui_events.cpp` → see how screens are wired

**Step 2: Make the change**
- Add new screen file: `src/ui_Screen9.c`
- Wire event handlers in `ui_events.cpp`
- Test on hardware

**Step 3: Update docs (minimal)**
- `project_status.md` → add Screen 9 to the table (1 line edit)
- **Done.** (No need to rewrite the entire doc or explain LVGL again)

### Scenario: You fix a display bug

**Step 1: Read context**
- `troubleshooting_log.md` → check if this bug was seen before
- `display_notes.md` → understand the hardware config

**Step 2: Fix the bug**
- Adjust `LovyanGFX_Driver.h`
- Test on hardware

**Step 3: Update docs**
- `troubleshooting_log.md` → add new issue entry with symptom, cause, fix
- `lessons_learned.md` → if the fix teaches a reusable lesson, summarize it
- `display_notes.md` → update the relevant section if config changed
- **Stop here.** Don't rewrite unrelated sections.

---

## Anti-Patterns (Don't Do This)

❌ **"Let me document everything before starting"** → You'll waste time documenting things that will change.  
✅ **Document as you learn.** Write down the "aha!" moments when you fix a bug or discover a quirk.

❌ **"I'll update docs later"** → You'll forget the details. Context decays fast.  
✅ **Update immediately after the change.** 2 minutes now saves 20 minutes of debugging later.

❌ **"This doc is incomplete, let me rewrite it from scratch"** → You'll lose existing knowledge.  
✅ **Refactor incrementally.** Fix one section at a time, preserving what works.

❌ **"I'll create a comprehensive guide to everything"** → Nobody reads 50-page docs.  
✅ **Keep docs task-focused.** "How do I fix touch?" beats "Complete Hardware Reference Manual."

---

## Why This Matters

**Problem:** Large codebases lose context. Six months from now, you (or someone else) will ask:
- "Why is `offset_rotation = 2` and not 0?"
- "Why did we remove `setColorDepth(16)`?"
- "Where's the example code for the working timer?"

**Solution:** Small, focused docs that answer specific questions quickly.

**BMAD-style context sharding** means breaking knowledge into **discoverable, digestible chunks** that can be read independently. Each doc is a "shard" of context. The index maps topics to shards.

---

## Reference: Example Code Location

**Working reference implementation:**  
`extras/examples/Desktop_Assistant_35/`

This folder contains the **known-good** Elecrow factory example that boots correctly. When in doubt about hardware config, pin assignments, or library usage, **check this folder first**.

Key files:
- `LovyanGFX_Driver.h` → Panel and touch config that works
- `display_Screen.cpp` → Init sequence
- `Desktop_Assistant_35.ino` → Feature wiring (alarm, timer, music, etc.)
- `get_Time.cpp`, `timer.cpp`, etc. → Feature module examples

**Rule:** If the example does it one way and the docs say something else, **the example is correct.** Update the docs.

---

## TL;DR

1. **Read context docs before coding** (2–3 files max, use `doc_index.md`)
2. **Update only relevant docs after coding** (don't overdo it)
3. **Keep docs small and focused** (one topic per file)
4. **Refactor docs when they grow** (split into smaller shards)
5. **Stop when the next person has enough context** (don't document everything)

**Documentation is a tool, not a goal.** Write just enough to prevent the next bug.

---

**Last Updated:** April 7, 2026
