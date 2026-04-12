// Project  : HelpDesk
// File     : ui_tictactoe.c
// Purpose  : Tic-Tac-Toe mini-game — random AI opponent or 2-player mode
// Depends  : ui.h (LVGL 9.x)

#include "ui.h"

/* ── Colours ─────────────────────────────────────────────────── */
#define CLR_BG          0x1A1A2E
#define CLR_HDR         0x16213E
#define CLR_ACCENT      0xE91E63   /* Pink — matches GameBreak tile */
#define CLR_X           0xEF5350   /* Red  — X marks                */
#define CLR_O           0x42A5F5   /* Blue — O marks                */
#define CLR_CELL        0x252540   /* Cell background               */
#define CLR_CELL_HIT    0x303058   /* Cell pressed state            */
#define CLR_WIN_CELL    0x1B5E20   /* Winning cell highlight        */
#define CLR_MODE_ACT    0x37474F   /* Active mode button            */
#define CLR_MODE_INACT  0x1A1A2E   /* Inactive mode button          */

/* ── Dimensions ─────────────────────────────────────────────── */
#define SCREEN_W    480
#define HDR_H        36            /* Top header bar               */
#define MODE_BAR_H   28            /* Mode-toggle bar              */
#define STATUS_Y     66            /* Status label top-y           */
#define CELL_SIZE    76            /* Each cell width and height   */
#define CELL_GAP      3            /* Gap between cells            */
#define BOARD_SIZE   (CELL_SIZE * 3 + CELL_GAP * 2)   /* 234 px  */
#define BOARD_X      ((SCREEN_W - BOARD_SIZE) / 2)    /* 123 px  */
#define BOARD_Y      (STATUS_Y + 18 + 2)              /* 86  px  */

/* ── Public object ───────────────────────────────────────────── */
lv_obj_t * ui_TicTacToe = NULL;

/* ── Private widgets ─────────────────────────────────────────── */
static lv_obj_t * s_cells[9];       /* 9 cell container objects     */
static lv_obj_t * s_cell_lbls[9];   /* X / O label inside each cell */
static lv_obj_t * s_status_label = NULL;
static lv_obj_t * s_mode_ai_btn  = NULL;
static lv_obj_t * s_mode_2p_btn  = NULL;

/* ── Game state ──────────────────────────────────────────────── */
/* Board values: 0 = empty, 1 = X, -1 = O */
static int8_t  s_board[9];
static int     s_current;      /* 1 (X's turn) or -1 (O's turn) */
static bool    s_ai_mode = true;  /* default: vs AI; persists across visits */
static bool    s_game_over;
static uint32_t s_rng;         /* Xorshift32 state, seeded per game */

/* Win-check line definitions */
static const int WIN_LINES[8][3] = {
    {0,1,2},{3,4,5},{6,7,8},   /* rows      */
    {0,3,6},{1,4,7},{2,5,8},   /* columns   */
    {0,4,8},{2,4,6}            /* diagonals */
};

/* ── Private utilities ───────────────────────────────────────── */

/* Xorshift32 PRNG — fast, no stdlib dependency, good enough for a game */
static uint32_t game_rand(void)
{
    s_rng ^= s_rng << 13;
    s_rng ^= s_rng >> 17;
    s_rng ^= s_rng << 5;
    return s_rng;
}

static int check_winner(void)
{
    int i;
    for (i = 0; i < 8; i++) {
        int sum = (int)s_board[WIN_LINES[i][0]]
                + (int)s_board[WIN_LINES[i][1]]
                + (int)s_board[WIN_LINES[i][2]];
        if (sum ==  3) return  1;
        if (sum == -3) return -1;
    }
    return 0;
}

static bool board_full(void)
{
    int i;
    for (i = 0; i < 9; i++) if (s_board[i] == 0) return false;
    return true;
}

/* Minimax — recursively scores all positions from the AI's perspective.
 * AI is -1 (maximising), human is +1 (minimising).
 * Works in-place on s_board; callers must restore the cell after the call. */
static int minimax(bool is_ai_turn)
{
    int w = check_winner();
    if (w == -1) return  10;   /* AI wins  */
    if (w ==  1) return -10;   /* Human wins */
    if (board_full()) return 0;

    int i;
    if (is_ai_turn) {
        int best = -100;
        for (i = 0; i < 9; i++) {
            if (s_board[i] == 0) {
                s_board[i] = -1;
                int score = minimax(false);
                s_board[i] = 0;
                if (score > best) best = score;
            }
        }
        return best;
    } else {
        int best = 100;
        for (i = 0; i < 9; i++) {
            if (s_board[i] == 0) {
                s_board[i] = 1;
                int score = minimax(true);
                s_board[i] = 0;
                if (score < best) best = score;
            }
        }
        return best;
    }
}

/* Pick the best move for the AI.  When multiple cells share the best score,
 * one is chosen randomly so the AI doesn't always open identically. */
static int ai_best_move(void)
{
    int best_score = -100;
    int best_moves[9], n_best = 0, i;

    for (i = 0; i < 9; i++) {
        if (s_board[i] == 0) {
            s_board[i] = -1;
            int score = minimax(false);
            s_board[i] = 0;
            if (score > best_score) {
                best_score = score;
                n_best = 0;
                best_moves[n_best++] = i;
            } else if (score == best_score) {
                best_moves[n_best++] = i;
            }
        }
    }
    if (n_best == 0) return -1;
    return best_moves[game_rand() % (uint32_t)n_best];
}

/* 15% of the time the AI forgets the optimal move and picks randomly.
 * This gives the player a real chance to win without making the AI feel broken. */
#define AI_BLUNDER_PCT  15

static int ai_choose_move(void)
{
    if ((game_rand() % 100u) < AI_BLUNDER_PCT) {
        /* Blunder: pick any empty cell at random */
        int empty[9], n = 0, i;
        for (i = 0; i < 9; i++) if (s_board[i] == 0) empty[n++] = i;
        if (n > 0) return empty[game_rand() % (uint32_t)n];
    }
    return ai_best_move();
}

/* ── UI update helpers ───────────────────────────────────────── */

static void update_cell_display(int idx)
{
    if (s_board[idx] == 1) {
        lv_label_set_text(s_cell_lbls[idx], "X");
        lv_obj_set_style_text_color(s_cell_lbls[idx], lv_color_hex(CLR_X), 0);
    } else if (s_board[idx] == -1) {
        lv_label_set_text(s_cell_lbls[idx], "O");
        lv_obj_set_style_text_color(s_cell_lbls[idx], lv_color_hex(CLR_O), 0);
    } else {
        lv_label_set_text(s_cell_lbls[idx], "");
    }
}

static void highlight_winning_cells(int winner)
{
    int i;
    for (i = 0; i < 8; i++) {
        int sum = (int)s_board[WIN_LINES[i][0]]
                + (int)s_board[WIN_LINES[i][1]]
                + (int)s_board[WIN_LINES[i][2]];
        if ((winner == 1 && sum == 3) || (winner == -1 && sum == -3)) {
            int j;
            for (j = 0; j < 3; j++) {
                lv_obj_set_style_bg_color(s_cells[WIN_LINES[i][j]],
                                          lv_color_hex(CLR_WIN_CELL), 0);
            }
        }
    }
}

static void update_turn_status(void)
{
    if (s_ai_mode) {
        lv_label_set_text(s_status_label, "Your turn  (X)");
    } else {
        lv_label_set_text(s_status_label,
                          s_current == 1 ? "X's turn" : "O's turn");
    }
}

/* ── Game flow ───────────────────────────────────────────────── */

static void finish_game(int winner)
{
    s_game_over = true;
    if (winner != 0) {
        highlight_winning_cells(winner);
        if (s_ai_mode) {
            lv_label_set_text(s_status_label,
                winner == -1 ? "AI wins!  Tap New Game." : "You win!  Tap New Game.");
        } else {
            lv_label_set_text(s_status_label,
                winner == 1 ? "X wins!  Tap New Game." : "O wins!  Tap New Game.");
        }
    } else {
        lv_label_set_text(s_status_label, "Draw!  Tap New Game.");
    }
}

/* Place mark at idx, hand off to AI if needed, check for end of game */
static void apply_mark(int idx, int8_t mark)
{
    int winner;
    s_board[idx] = mark;
    update_cell_display(idx);

    winner = check_winner();
    if (winner != 0 || board_full()) { finish_game(winner); return; }

    /* Switch active player */
    s_current = -s_current;

    /* In AI mode, O always plays immediately after the human's move */
    if (s_ai_mode && s_current == -1) {
        int ai_idx = ai_choose_move();
        if (ai_idx >= 0) {
            s_board[ai_idx] = -1;
            update_cell_display(ai_idx);
            winner = check_winner();
            if (winner != 0 || board_full()) { finish_game(winner); return; }
            s_current = 1;  /* hand turn back to human */
        }
    }

    update_turn_status();
}

static void reset_board_display(void)
{
    int i;
    for (i = 0; i < 9; i++) {
        s_board[i] = 0;
        update_cell_display(i);
        lv_obj_set_style_bg_color(s_cells[i], lv_color_hex(CLR_CELL), 0);
    }
}

static void start_new_game(void)
{
    /* Seed with current tick — different every time New Game is pressed */
    s_rng = lv_tick_get() | 1u;  /* xorshift32 must not be zero */
    s_game_over = false;
    reset_board_display();

    /* Randomly decide who moves first */
    s_current = (game_rand() & 1u) ? 1 : -1;

    if (s_ai_mode && s_current == -1) {
        /* AI goes first: place one mark immediately, then give turn to human */
        int ai_idx = ai_choose_move();
        if (ai_idx >= 0) {
            s_board[ai_idx] = -1;
            update_cell_display(ai_idx);
            s_current = 1;
        }
        lv_label_set_text(s_status_label, "AI went first — your turn  (X)");
    } else {
        update_turn_status();
    }
}

/* ── Event callbacks ─────────────────────────────────────────── */

static void back_ev(lv_event_t * e)
{
    (void)e;
    _ui_screen_change(&ui_Screen8, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT, 300, 0,
                      ui_Screen8_screen_init);
}

static void new_game_ev(lv_event_t * e)
{
    (void)e;
    start_new_game();
}

static void mode_ai_ev(lv_event_t * e)
{
    (void)e;
    if (s_ai_mode) return;
    s_ai_mode = true;
    lv_obj_set_style_bg_color(s_mode_ai_btn, lv_color_hex(CLR_MODE_ACT),   0);
    lv_obj_set_style_bg_color(s_mode_2p_btn, lv_color_hex(CLR_MODE_INACT), 0);
    start_new_game();
}

static void mode_2p_ev(lv_event_t * e)
{
    (void)e;
    if (!s_ai_mode) return;
    s_ai_mode = false;
    lv_obj_set_style_bg_color(s_mode_ai_btn, lv_color_hex(CLR_MODE_INACT), 0);
    lv_obj_set_style_bg_color(s_mode_2p_btn, lv_color_hex(CLR_MODE_ACT),   0);
    start_new_game();
}

static void cell_ev(lv_event_t * e)
{
    int idx = (int)(uintptr_t)lv_event_get_user_data(e);
    if (s_game_over)        return;  /* Game already ended           */
    if (s_board[idx] != 0) return;  /* Cell already occupied         */
    /* In AI mode the human is always X (1); ignore taps during AI's turn */
    if (s_ai_mode && s_current != 1) return;
    apply_mark(idx, (int8_t)s_current);
}

/* ── Build helpers ───────────────────────────────────────────── */

static void build_header(lv_obj_t * scr)
{
    lv_obj_t * hdr = lv_obj_create(scr);
    lv_obj_set_size(hdr, SCREEN_W, HDR_H);
    lv_obj_set_pos(hdr, 0, 0);
    lv_obj_remove_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(hdr, lv_color_hex(CLR_HDR), 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_radius(hdr, 0, 0);
    lv_obj_set_style_pad_all(hdr, 0, 0);

    lv_obj_t * back_btn = lv_button_create(hdr);
    lv_obj_set_size(back_btn, 40, HDR_H - 2);
    lv_obj_set_pos(back_btn, 2, 1);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(CLR_HDR), 0);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x0D1321),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(back_btn, 0, 0);
    lv_obj_set_style_shadow_width(back_btn, 0, 0);
    lv_obj_set_style_pad_all(back_btn, 0, 0);
    lv_obj_add_event_cb(back_btn, back_ev, LV_EVENT_CLICKED, NULL);

    lv_obj_t * ico = lv_label_create(back_btn);
    lv_label_set_text(ico, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(ico, lv_color_white(), 0);
    lv_obj_center(ico);

    lv_obj_t * title = lv_label_create(hdr);
    lv_label_set_text(title, "Tic-Tac-Toe");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t * ng_btn = lv_button_create(hdr);
    lv_obj_set_size(ng_btn, 80, HDR_H - 8);
    lv_obj_align(ng_btn, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_set_style_bg_color(ng_btn, lv_color_hex(CLR_ACCENT), 0);
    lv_obj_set_style_bg_color(ng_btn,
        lv_color_mix(lv_color_black(), lv_color_hex(CLR_ACCENT), LV_OPA_30),
        LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(ng_btn, 0, 0);
    lv_obj_set_style_shadow_width(ng_btn, 0, 0);
    lv_obj_set_style_radius(ng_btn, 4, 0);
    lv_obj_set_style_pad_all(ng_btn, 0, 0);
    lv_obj_add_event_cb(ng_btn, new_game_ev, LV_EVENT_CLICKED, NULL);

    lv_obj_t * ng_lbl = lv_label_create(ng_btn);
    lv_label_set_text(ng_lbl, "New Game");
    lv_obj_set_style_text_color(ng_lbl, lv_color_white(), 0);
    lv_obj_center(ng_lbl);
}

static void build_mode_bar(lv_obj_t * scr)
{
    lv_obj_t * bar = lv_obj_create(scr);
    lv_obj_set_size(bar, SCREEN_W, MODE_BAR_H);
    lv_obj_set_pos(bar, 0, HDR_H);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(bar, lv_color_hex(CLR_HDR), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);

    lv_obj_t * lbl = lv_label_create(bar);
    lv_label_set_text(lbl, "Mode:");
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x8090A0), 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 8, 0);

    s_mode_ai_btn = lv_button_create(bar);
    lv_obj_set_size(s_mode_ai_btn, 60, MODE_BAR_H - 8);
    lv_obj_align(s_mode_ai_btn, LV_ALIGN_LEFT_MID, 58, 0);
    lv_obj_set_style_border_width(s_mode_ai_btn, 0, 0);
    lv_obj_set_style_shadow_width(s_mode_ai_btn, 0, 0);
    lv_obj_set_style_radius(s_mode_ai_btn, 4, 0);
    lv_obj_add_event_cb(s_mode_ai_btn, mode_ai_ev, LV_EVENT_CLICKED, NULL);

    lv_obj_t * ai_lbl = lv_label_create(s_mode_ai_btn);
    lv_label_set_text(ai_lbl, "vs AI");
    lv_obj_set_style_text_color(ai_lbl, lv_color_white(), 0);
    lv_obj_center(ai_lbl);

    s_mode_2p_btn = lv_button_create(bar);
    lv_obj_set_size(s_mode_2p_btn, 74, MODE_BAR_H - 8);
    lv_obj_align(s_mode_2p_btn, LV_ALIGN_LEFT_MID, 123, 0);
    lv_obj_set_style_border_width(s_mode_2p_btn, 0, 0);
    lv_obj_set_style_shadow_width(s_mode_2p_btn, 0, 0);
    lv_obj_set_style_radius(s_mode_2p_btn, 4, 0);
    lv_obj_add_event_cb(s_mode_2p_btn, mode_2p_ev, LV_EVENT_CLICKED, NULL);

    lv_obj_t * p2_lbl = lv_label_create(s_mode_2p_btn);
    lv_label_set_text(p2_lbl, "2 Player");
    lv_obj_set_style_text_color(p2_lbl, lv_color_white(), 0);
    lv_obj_center(p2_lbl);

    /* Apply correct highlight based on current mode state */
    lv_obj_set_style_bg_color(s_mode_ai_btn,
        lv_color_hex(s_ai_mode ? CLR_MODE_ACT : CLR_MODE_INACT), 0);
    lv_obj_set_style_bg_color(s_mode_2p_btn,
        lv_color_hex(s_ai_mode ? CLR_MODE_INACT : CLR_MODE_ACT), 0);
}

static void build_status_label(lv_obj_t * scr)
{
    s_status_label = lv_label_create(scr);
    lv_obj_set_width(s_status_label, SCREEN_W - 20);
    lv_obj_set_pos(s_status_label, 10, STATUS_Y);
    lv_label_set_long_mode(s_status_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0xCCCCDD), 0);
    lv_obj_set_style_text_align(s_status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_status_label, "");
}

static void build_cell(lv_obj_t * scr, int idx)
{
    int row = idx / 3;
    int col = idx % 3;
    int x   = BOARD_X + col * (CELL_SIZE + CELL_GAP);
    int y   = BOARD_Y + row * (CELL_SIZE + CELL_GAP);

    lv_obj_t * cell = lv_obj_create(scr);
    lv_obj_set_size(cell, CELL_SIZE, CELL_SIZE);
    lv_obj_set_pos(cell, x, y);
    lv_obj_remove_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(cell, lv_color_hex(CLR_CELL), 0);
    lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(cell, lv_color_hex(0x404060), 0);
    lv_obj_set_style_border_width(cell, 1, 0);
    lv_obj_set_style_radius(cell, 8, 0);
    lv_obj_set_style_pad_all(cell, 0, 0);
    lv_obj_set_style_bg_color(cell, lv_color_hex(CLR_CELL_HIT),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_event_cb(cell, cell_ev, LV_EVENT_CLICKED, (void *)(uintptr_t)idx);

    lv_obj_t * mark_lbl = lv_label_create(cell);
    lv_label_set_text(mark_lbl, "");
    lv_obj_set_style_text_font(mark_lbl, &lv_font_montserrat_36, 0);
    lv_obj_center(mark_lbl);

    s_cells[idx]     = cell;
    s_cell_lbls[idx] = mark_lbl;
}

static void build_board(lv_obj_t * scr)
{
    int i;
    for (i = 0; i < 9; i++) build_cell(scr, i);
}

/* ── Public lifecycle ────────────────────────────────────────── */

void ui_tictactoe_screen_init(void)
{
    ui_TicTacToe = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_TicTacToe, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_TicTacToe, lv_color_hex(CLR_BG), 0);
    lv_obj_set_style_bg_opa(ui_TicTacToe, LV_OPA_COVER, 0);

    lv_obj_add_event_cb(ui_TicTacToe, scr_unloaded_delete_cb,
                        LV_EVENT_SCREEN_UNLOADED, ui_tictactoe_screen_destroy);

    build_header(ui_TicTacToe);
    build_mode_bar(ui_TicTacToe);
    build_status_label(ui_TicTacToe);
    build_board(ui_TicTacToe);

    start_new_game();
}

void ui_tictactoe_screen_destroy(void)
{
    int i;
    s_status_label = NULL;
    s_mode_ai_btn  = NULL;
    s_mode_2p_btn  = NULL;
    for (i = 0; i < 9; i++) {
        s_cells[i]     = NULL;
        s_cell_lbls[i] = NULL;
    }
    if (ui_TicTacToe) lv_obj_delete(ui_TicTacToe);
    ui_TicTacToe = NULL;
}
