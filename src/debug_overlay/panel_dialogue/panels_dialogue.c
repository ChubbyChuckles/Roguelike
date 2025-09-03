/* Debug Overlay: Dialogue Panel (M4.5)
 * - Branch graph mini-view (textual, headless-safe)
 * - Rich text style inspector (edits RogueDialogueStyle)
 * - Test conversation runner (register sample + start/advance/reset)
 */
#include "../../game/dialogue.h"
#include "../../util/log.h"
#include "../overlay_core.h"
#include "../overlay_icon.h"
#include "../widgets/overlay_widgets.h"
#include <stdio.h>
#include <string.h>

#if ROGUE_ENABLE_DEBUG_OVERLAY

typedef struct DialoguePanelState
{
    int left_w;
    int selected_script_idx; /* index in registry enumeration order */
    int use_sample;          /* 1 = register and use built-in sample */
    int show_style;
} DialoguePanelState;

static DialoguePanelState g_dp = {360, 0, 1, 1};

/* Small built-in sample script (id 101) for quick preview */
static const char* k_sample_script = "Narrator|Welcome to the Dialogue preview.\n"
                                     "Guide|Use the controls on the left to Start and Advance.\n"
                                     "Guide|Style settings below affect the runtime renderer.\n";

static void dp_register_sample_if_needed(void)
{
    /* If there's no script with id 101, register the sample. */
    const RogueDialogueScript* probe = rogue_dialogue_get(101);
    if (!probe)
    {
        (void) rogue_dialogue_register_from_buffer(101, k_sample_script,
                                                   (int) strlen(k_sample_script));
    }
}

static int dp_script_count(void) { return rogue_dialogue_script_count(); }

static const RogueDialogueScript* dp_script_by_index(int idx)
{
    /* Registry enumeration helper: iterate through possible ids; this API exposes fetch by id. */
    /* We don’t have a direct iterator; assume ids are small-ish and search linearly for now. */
    /* To keep deterministic and safe, scan a fixed range of ids. */
    int found = 0;
    int i;
    for (i = 0; i < 2048; ++i)
    {
        const RogueDialogueScript* s = rogue_dialogue_get(i);
        if (!s)
            continue;
        if (found == idx)
            return s;
        found++;
    }
    /* Fallback: check our sample id specifically */
    if (idx == 0)
        return rogue_dialogue_get(101);
    return NULL;
}

static void dp_render_style_editor(void)
{
    const RogueDialogueStyle* cur = rogue_dialogue_style_get();
    RogueDialogueStyle st = *cur; /* copy so we can edit */

    overlay_label("Style (affects runtime renderer)");

    /* Unpack/edit colors as RGBA; RogueDialogueStyle stores ARGB. */
    unsigned char panel_top_rgba[4] = {(unsigned char) ((st.panel_color_top >> 16) & 255),
                                       (unsigned char) ((st.panel_color_top >> 8) & 255),
                                       (unsigned char) (st.panel_color_top & 255),
                                       (unsigned char) ((st.panel_color_top >> 24) & 255)};
    unsigned char panel_bot_rgba[4] = {(unsigned char) ((st.panel_color_bottom >> 16) & 255),
                                       (unsigned char) ((st.panel_color_bottom >> 8) & 255),
                                       (unsigned char) (st.panel_color_bottom & 255),
                                       (unsigned char) ((st.panel_color_bottom >> 24) & 255)};
    unsigned char border_rgba[4] = {(unsigned char) ((st.border_color >> 16) & 255),
                                    (unsigned char) ((st.border_color >> 8) & 255),
                                    (unsigned char) (st.border_color & 255),
                                    (unsigned char) ((st.border_color >> 24) & 255)};
    unsigned char speaker_rgba[4] = {(unsigned char) ((st.speaker_color >> 16) & 255),
                                     (unsigned char) ((st.speaker_color >> 8) & 255),
                                     (unsigned char) (st.speaker_color & 255),
                                     (unsigned char) ((st.speaker_color >> 24) & 255)};
    unsigned char text_rgba[4] = {
        (unsigned char) ((st.text_color >> 16) & 255), (unsigned char) ((st.text_color >> 8) & 255),
        (unsigned char) (st.text_color & 255), (unsigned char) ((st.text_color >> 24) & 255)};

    if (overlay_color_edit_rgba("Panel Top", panel_top_rgba))
        st.panel_color_top = ((unsigned) panel_top_rgba[3] << 24) |
                             ((unsigned) panel_top_rgba[0] << 16) |
                             ((unsigned) panel_top_rgba[1] << 8) | panel_top_rgba[2];
    if (overlay_color_edit_rgba("Panel Bottom", panel_bot_rgba))
        st.panel_color_bottom = ((unsigned) panel_bot_rgba[3] << 24) |
                                ((unsigned) panel_bot_rgba[0] << 16) |
                                ((unsigned) panel_bot_rgba[1] << 8) | panel_bot_rgba[2];
    if (overlay_color_edit_rgba("Border", border_rgba))
        st.border_color = ((unsigned) border_rgba[3] << 24) | ((unsigned) border_rgba[0] << 16) |
                          ((unsigned) border_rgba[1] << 8) | border_rgba[2];
    if (overlay_color_edit_rgba("Speaker", speaker_rgba))
        st.speaker_color = ((unsigned) speaker_rgba[3] << 24) | ((unsigned) speaker_rgba[0] << 16) |
                           ((unsigned) speaker_rgba[1] << 8) | speaker_rgba[2];
    if (overlay_color_edit_rgba("Text", text_rgba))
        st.text_color = ((unsigned) text_rgba[3] << 24) | ((unsigned) text_rgba[0] << 16) |
                        ((unsigned) text_rgba[1] << 8) | text_rgba[2];

    (void) overlay_checkbox("Gradient", &st.enable_gradient);
    (void) overlay_checkbox("Text Shadow", &st.enable_text_shadow);
    (void) overlay_checkbox("Blink Prompt", &st.show_blink_prompt);
    (void) overlay_checkbox("Caret", &st.show_caret);
    (void) overlay_checkbox("Parchment", &st.use_parchment);
    (void) overlay_slider_int("Border Thickness", &st.border_thickness, 0, 8);
    (void) overlay_slider_int("Panel Height (0=auto)", &st.panel_height, 0, 400);

    (void) rogue_dialogue_style_set(&st);
}

static void dp_render_controls(void)
{
    /* Sample registration */
    (void) overlay_checkbox("Register sample (id 101)", &g_dp.use_sample);
    if (g_dp.use_sample)
        dp_register_sample_if_needed();

    /* Scripts combo */
    int scount = dp_script_count();
    char combo_label[64];
    snprintf(combo_label, sizeof combo_label, "Scripts (%d)", scount);

    /* Build a lightweight list of up to 64 entries for the combo */
    enum
    {
        MAX_LIST = 64
    };
    const char* items[MAX_LIST];
    char storage[MAX_LIST][64];
    int i;
    int shown = 0;
    for (i = 0; i < MAX_LIST; ++i)
    {
        items[i] = storage[i];
        storage[i][0] = '\0';
    }
    for (i = 0; i < MAX_LIST; ++i)
    {
        const RogueDialogueScript* s = dp_script_by_index(i);
        if (!s)
            break;
        snprintf(storage[i], sizeof storage[i], "#%d lines=%d", s->id, s->line_count);
        shown++;
    }
    if (shown == 0)
    {
        items[0] = "(none)";
        shown = 1;
        g_dp.selected_script_idx = 0;
    }
    (void) overlay_combo(combo_label, &g_dp.selected_script_idx, items, shown);

    /* Playback controls */
    if (overlay_icon_button("Start", OVERLAY_ICON_PLAY))
    {
        const RogueDialogueScript* s = dp_script_by_index(g_dp.selected_script_idx);
        if (s)
            rogue_dialogue_start(s->id);
    }
    if (overlay_icon_button("Advance", OVERLAY_ICON_PLAY))
    {
        (void) rogue_dialogue_advance();
    }
    if (overlay_icon_button("Reset", OVERLAY_ICON_UNDO))
    {
        RogueDialoguePersistState st;
        st.active = 0;
        st.script_id = 0;
        st.line_index = 0;
        st.reveal_ms = 0.0f;
        (void) rogue_dialogue_restore(&st);
    }

    (void) overlay_checkbox("Show Style", &g_dp.show_style);
    if (g_dp.show_style)
        dp_render_style_editor();
}

static void dp_render_preview_and_graph(void)
{
    /* Current line preview */
    overlay_label("Preview:");
    char buf[1024];
    const RogueDialoguePlayback* pb = rogue_dialogue_playback();
    if (pb && pb->active)
    {
        int n = rogue_dialogue_current_text(buf, sizeof buf);
        if (n > 0)
        {
            buf[(n < (int) sizeof buf) ? n : ((int) sizeof buf - 1)] = '\0';
            overlay_label(buf);
        }
        else
        {
            overlay_label("(no text)");
        }
    }
    else
    {
        overlay_label("(inactive)");
    }

    /* Mini branch graph (textual): list lines and an ASCII chain */
    overlay_label("Graph:");
    const RogueDialogueScript* s = dp_script_by_index(g_dp.selected_script_idx);
    if (!s)
    {
        overlay_label("(no script)");
        return;
    }
    /* List lines with index and speaker */
    int i;
    for (i = 0; i < s->line_count && i < 24; ++i)
    {
        const char* sp = s->lines[i].speaker_id ? s->lines[i].speaker_id : "?";
        const char* tx = s->lines[i].text ? s->lines[i].text : "";
        char row[160];
        int is_cur = (pb && pb->active && pb->script_id == s->id && pb->line_index == i);
        /* Truncate text for compact display */
        char trunc[64];
        size_t tlen = 0;
        size_t j;
        for (j = 0; tx[j] && j < sizeof trunc - 1; ++j)
        {
            char c = tx[j];
            if (c == '\n' || c == '\r')
                break;
            trunc[j] = c;
            tlen++;
        }
        trunc[tlen] = '\0';
        snprintf(row, sizeof row, "%s%02d: %s | %s", is_cur ? "> " : "  ", i, sp, trunc);
        overlay_label(row);
    }
    /* Render an ASCII chain like 0 -> 1 -> 2 ... */
    if (s->line_count > 0)
    {
        char chain[256];
        chain[0] = '\0';
        size_t out = 0;
        int limit = s->line_count;
        if (limit > 16)
            limit = 16;
        for (i = 0; i < limit; ++i)
        {
            char seg[16];
            snprintf(seg, sizeof seg, "%d%s", i, (i + 1 < limit) ? " -> " : "");
            size_t sl = strlen(seg);
            if (out + sl >= sizeof chain)
                break;
            memcpy(chain + out, seg, sl);
            out += sl;
        }
        chain[out < sizeof chain ? out : (sizeof chain - 1)] = '\0';
        overlay_label(chain);
    }
}

static void dialogue_panel_render(void* user)
{
    (void) user;
    if (!overlay_begin_panel_auto("dialogue", "Dialogue", 60, 60, 860))
        return;

    if (overlay_splitter_begin("dialogue.split", &g_dp.left_w, 240, 640))
    {
        dp_render_controls();
        overlay_splitter_end();
        overlay_next_column();
        dp_render_preview_and_graph();
        overlay_columns_end();
    }

    overlay_end_panel();
}

void rogue_overlay_register_panel_dialogue(void)
{
    overlay_register_panel("dialogue", "Dialogue", dialogue_panel_render, NULL);
}

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
