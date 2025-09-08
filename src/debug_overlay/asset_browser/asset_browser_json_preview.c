/* Extracted from panels_asset_browser.c: lightweight JSON preview renderer. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "asset_browser_json_preview.h"

/* Use same relative include style as panels_asset_browser.c */
#include "../../graphics/font.h"     /* rogue_font_draw_text & g_rogue_builtin_font */
#include "../../graphics/renderer.h" /* RogueColor */
#include "../overlay_core.h"
#include "../overlay_theme.h"
#include "../widgets/overlay_widgets_internal.h"

/* SDL header not required here; keep macro check minimal to avoid extra dependency. */

void rogue_asset_browser_json_draw_preview(const char* buffer)
{
    if (!buffer)
        return;
#if !ROGUE_ENABLE_DEBUG_OVERLAY
    (void) buffer;
    return;
#else
    const OverlayTheme* th = overlay_theme_get();
    const char* p = buffer;
    int lines = 0;
    while (*p && lines < 12)
    {
        const char* line_start = p;
        const char* line_end = p;
        int newline = 0;
        while (*line_end && *line_end != '\n' && (line_end - line_start) < 512)
            line_end++;
        if (*line_end == '\n')
            newline = 1;
        const char* cur = line_start;
        while (cur < line_end)
        {
            unsigned char r = th->text.r, g = th->text.g, b = th->text.b, a = th->text.a;
            const char* tok_start = cur;
            const char* tok_end = cur + 1;
            char tmp[256];
            if (*cur == ' ' || *cur == '\t')
            {
                while (tok_end < line_end && (*tok_end == ' ' || *tok_end == '\t'))
                    tok_end++;
            }
            else if (*cur == '"')
            {
                tok_end = cur + 1;
                while (tok_end < line_end)
                {
                    if (*tok_end == '"')
                    {
                        int bs = 0;
                        const char* q = tok_end - 1;
                        while (q >= cur && *q == '\\')
                        {
                            bs++;
                            q--;
                        }
                        if ((bs & 1) == 0)
                        {
                            tok_end++;
                            break;
                        }
                    }
                    tok_end++;
                }
                r = th->text_accent.r;
                g = th->text_accent.g;
                b = th->text_accent.b;
                a = th->text_accent.a;
            }
            else if ((*cur >= '0' && *cur <= '9') ||
                     (*cur == '-' && (cur + 1) < line_end && cur[1] >= '0' && cur[1] <= '9'))
            {
                while (tok_end < line_end &&
                       ((*tok_end >= '0' && *tok_end <= '9') || *tok_end == '.' ||
                        *tok_end == 'e' || *tok_end == 'E' || *tok_end == '+' || *tok_end == '-'))
                    tok_end++;
                r = th->accent_1.r;
                g = th->accent_1.g;
                b = th->accent_1.b;
                a = th->accent_1.a;
            }
            else if ((*cur >= 'a' && *cur <= 'z') || (*cur >= 'A' && *cur <= 'Z'))
            {
                while (tok_end < line_end && ((*tok_end >= 'a' && *tok_end <= 'z') ||
                                              (*tok_end >= 'A' && *tok_end <= 'Z')))
                    tok_end++;
                int len = (int) (tok_end - tok_start);
                if ((len == 4 &&
                     (strncmp(tok_start, "true", 4) == 0 || strncmp(tok_start, "null", 4) == 0)) ||
                    (len == 5 && strncmp(tok_start, "false", 5) == 0))
                {
                    r = th->accent_2.r;
                    g = th->accent_2.g;
                    b = th->accent_2.b;
                    a = th->accent_2.a;
                }
            }
            else if (*cur == '{' || *cur == '}' || *cur == '[' || *cur == ']' || *cur == ':' ||
                     *cur == ',')
            {
                r = th->text_muted.r;
                g = th->text_muted.g;
                b = th->text_muted.b;
                a = th->text_muted.a;
            }
            int copy_len = (int) (tok_end - tok_start);
            if (copy_len > (int) sizeof tmp - 1)
                copy_len = (int) sizeof tmp - 1;
            memcpy(tmp, tok_start, copy_len);
            tmp[copy_len] = '\0';
            if (g_ui.panel_active)
            {
                rogue_font_draw_text(g_ui.cur_x, g_ui.cur_y + 4, tmp, 1, (RogueColor){r, g, b, a});
                g_ui.cur_x += copy_len * (g_rogue_builtin_font.glyph_w + 1);
                if (g_ui.row_max_h < 20)
                    g_ui.row_max_h = 20;
            }
            cur = tok_end;
        }
        g_ui.cur_x = g_ui.col_x0[g_ui.col_index];
        ui_next_line();
        p = newline ? line_end + 1 : line_end;
        lines++;
    }
#endif
}
