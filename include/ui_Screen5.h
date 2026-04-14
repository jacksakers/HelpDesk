// Project  : HelpDesk
// File     : ui_Screen5.h
// Purpose  : TaskMaster screen — public interface and exported widget handles
// Depends  : ui.h (LVGL 9.x)

#ifndef UI_SCREEN5_H
#define UI_SCREEN5_H

#ifdef __cplusplus
extern "C" {
#endif

/* SCREEN: ui_Screen5 — TaskMaster */
void ui_Screen5_screen_init(void);
void ui_Screen5_screen_destroy(void);
extern lv_obj_t * ui_Screen5;

/* ── Exported widget handles (valid only while Screen5 is loaded) ─────────── */

/* Scrollable flex-column container — task rows are direct children.
   task_master.cpp calls lv_obj_clean() and rebuilds rows into this object. */
extern lv_obj_t * ui_TaskList;

/* Daily XP progress bar.  Range is [0, TASK_DAILY_GOAL_XP]. */
extern lv_obj_t * ui_TaskXpBar;

/* Stats label: "30/50 XP  |  3 done today  |  2 left" */
extern lv_obj_t * ui_TaskXpLabel;

/* Level indicator shown in the header: "Lv.3" */
extern lv_obj_t * ui_TaskLevelLabel;

/* Streak indicator shown in the header: "3d streak" */
extern lv_obj_t * ui_TaskStreakLabel;

/* Task input textarea inside the keyboard overlay.
   voice_input.cpp writes transcribed text here via lv_textarea_set_text(). */
extern lv_obj_t * ui_TaskInputArea;

/* ── Animations ───────────────────────────────────────────────────────────── */

/* Briefly animate a floating "+N XP!" label on Screen5.
   Called from task_master.cpp after taskComplete() awards XP. */
void ui_Screen5_show_xp_popup(int xp);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
