// Project  : HelpDesk
// File     : ui_tictactoe.h
// Purpose  : Tic-Tac-Toe game screen — public interface
// Depends  : ui.h (LVGL 9.x)

#ifndef UI_TICTACTOE_H
#define UI_TICTACTOE_H

#ifdef __cplusplus
extern "C" {
#endif

/* SCREEN: ui_TicTacToe — Tic-Tac-Toe mini-game (AI or 2-player) */
void ui_tictactoe_screen_init(void);
void ui_tictactoe_screen_destroy(void);
extern lv_obj_t * ui_TicTacToe;

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
