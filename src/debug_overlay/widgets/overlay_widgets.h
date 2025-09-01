#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef ROGUE_ENABLE_DEBUG_OVERLAY
#define ROGUE_ENABLE_DEBUG_OVERLAY 0
#endif

#include <stddef.h>

    /* Simple style and context for drawing; uses existing font and SDL renderer via global app. */
    typedef struct OverlayStyle
    {
        unsigned char bg_r, bg_g, bg_b, bg_a;
        unsigned char fg_r, fg_g, fg_b, fg_a;
        int pad;
    } OverlayStyle;

#if ROGUE_ENABLE_DEBUG_OVERLAY

    /* Begin an overlay window/panel at x,y with width and optional title bar. Returns 1 if visible.
     */
    int overlay_begin_panel(const char* title, int x, int y, int w);
    void overlay_end_panel(void);

    /* Basic widgets. Return value indicates interaction:
     * - button: 1 if clicked this frame
     * - checkbox: toggles the value and returns 1 if changed
     * - sliders: clamp and write back; return 1 if changed
     * - input_text: edits the buffer; return 1 if changed
     */
    void overlay_label(const char* text);
    int overlay_button(const char* label);
    int overlay_checkbox(const char* label, int* value);
    int overlay_slider_int(const char* label, int* value, int minv, int maxv);
    int overlay_slider_float(const char* label, float* value, float minv, float maxv);
    int overlay_input_text(const char* label, char* buf, size_t buf_size);

    /* Advanced widgets */
    /* Combo/Dropdown: cycles or arrow-adjusts through items; returns 1 if selection changed */
    int overlay_combo(const char* label, int* current_index, const char* const* items, int count);
    /* Tree node: caller owns persistent open state; returns 1 when currently open */
    int overlay_tree_node(const char* label, int* open);
    void overlay_tree_pop(void);
    /* Color editor (RGBA 0..255); returns 1 if changed */
    int overlay_color_edit_rgba(const char* label, unsigned char rgba[4]);

    /* Table widget (minimal v1):
     * - overlay_table_begin draws a header row with clickable columns and updates sort state.
     *   sort_dir: +1 ascending, -1 descending (toggled when clicking the same column)
     *   filter_text (optional, may be NULL) currently unused (reserved for future filtering)
     * - overlay_table_row draws a row of text cells; clicking the row selects it and returns 1 if
     * selection changed.
     * - overlay_table_end closes the table block and advances layout.
     */
    int overlay_table_begin(const char* id, const char* const* headers, int col_count,
                            int* sort_col, int* sort_dir, const char* filter_text /* optional */);
    int overlay_table_row(const char* const* cells, int col_count, int row_index,
                          int* selected_row);
    void overlay_table_end(void);

    /* Style adjust */
    void overlay_style_set(OverlayStyle s);

    /* Layout: simple columns API */
    int overlay_columns_begin(int cols, const int* widths /* optional, NULL = equal */);
    void overlay_next_column(void);
    void overlay_columns_end(void);

#else

static inline int overlay_begin_panel(const char* title, int x, int y, int w)
{
    (void) title;
    (void) x;
    (void) y;
    (void) w;
    return 0;
}
static inline void overlay_end_panel(void) {}
static inline void overlay_label(const char* text) { (void) text; }
static inline int overlay_button(const char* label)
{
    (void) label;
    return 0;
}
static inline int overlay_checkbox(const char* label, int* value)
{
    (void) label;
    (void) value;
    return 0;
}
static inline int overlay_slider_int(const char* label, int* value, int minv, int maxv)
{
    (void) label;
    (void) value;
    (void) minv;
    (void) maxv;
    return 0;
}
static inline int overlay_slider_float(const char* label, float* value, float minv, float maxv)
{
    (void) label;
    (void) value;
    (void) minv;
    (void) maxv;
    return 0;
}
static inline int overlay_input_text(const char* label, char* buf, size_t buf_size)
{
    (void) label;
    (void) buf;
    (void) buf_size;
    return 0;
}
static inline int overlay_combo(const char* label, int* current_index, const char* const* items,
                                int count)
{
    (void) label;
    (void) current_index;
    (void) items;
    (void) count;
    return 0;
}
static inline int overlay_tree_node(const char* label, int* open)
{
    (void) label;
    (void) open;
    return 0;
}
static inline void overlay_tree_pop(void) {}
static inline int overlay_color_edit_rgba(const char* label, unsigned char rgba[4])
{
    (void) label;
    (void) rgba;
    return 0;
}
static inline int overlay_table_begin(const char* id, const char* const* headers, int col_count,
                                      int* sort_col, int* sort_dir, const char* filter_text)
{
    (void) id;
    (void) headers;
    (void) col_count;
    (void) sort_col;
    (void) sort_dir;
    (void) filter_text;
    return 0;
}
static inline int overlay_table_row(const char* const* cells, int col_count, int row_index,
                                    int* selected_row)
{
    (void) cells;
    (void) col_count;
    (void) row_index;
    (void) selected_row;
    return 0;
}
static inline void overlay_table_end(void) {}
static inline void overlay_style_set(OverlayStyle s) { (void) s; }
static inline int overlay_columns_begin(int cols, const int* widths)
{
    (void) cols;
    (void) widths;
    return 0;
}
static inline void overlay_next_column(void) {}
static inline void overlay_columns_end(void) {}

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */

#ifdef __cplusplus
}
#endif
