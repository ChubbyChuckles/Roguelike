#pragma once

#include "../overlay_core.h"
#include "../overlay_input.h"
#include "overlay_widgets.h"

#if ROGUE_ENABLE_DEBUG_OVERLAY

typedef struct UiCtx
{
    int cur_x, cur_y, width;
    int line_h;
    int row_max_h;
    int panel_active;
    OverlayStyle style;
    /* Columns */
    int columns;
    int col_widths[4];
    int col_x0[4];
    int col_index;
    int row_start_y;
    /* Focus & widgets */
    int focus_index;   /* -1 = none */
    int total_widgets; /* count of interactive widgets this frame */
    int first_focus;
    int last_focus;
    /* InputText caret position for focused field (single shared for simplicity) */
    int caret_pos;
    /* Table (transient, per-table block) */
    int table_active;
    int table_cols;
    int table_row_h;
    int table_row_pad;
    int table_hovered; /* 1 if mouse is over table header or any row this frame */
    /* Tooltip: string applied to the next widget only */
    const char* next_tooltip;
} UiCtx;

/* Global UI context owned by overlay widgets implementation */
extern UiCtx g_ui;

/* Shared helpers */
static inline void ui_next_line(void)
{
    if (g_ui.columns <= 1)
    {
        int step = g_ui.row_max_h > 0 ? g_ui.row_max_h : g_ui.line_h;
        g_ui.cur_y += step;
        g_ui.row_max_h = g_ui.line_h;
    }
}

int overlay_mouse_over(int x, int y, int w, int h);

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
