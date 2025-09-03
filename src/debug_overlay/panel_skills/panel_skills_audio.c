#include "panel_skills_audio.h"
#include "../../core/skills/skill_debug.h"
#include "../overlay_core.h"
#include "../widgets/overlay_widgets.h"
#include "panel_skills_shared.h"
#include <stdio.h>

#if ROGUE_ENABLE_DEBUG_OVERLAY
void panel_skills_draw_audio(int sel)
{
    overlay_label("Audio");
    static RogueSkillVisualParams vis;
    if (rogue_skill_debug_get_visuals(sel, &vis) == 0)
    {
        int vchanged = 0;
        /* Sound ID picker controls */
        enum
        {
            TARGET_CAST = 0,
            TARGET_IMPACT = 1,
            TARGET_LOOP = 2
        };
        static int s_browser_open = 0;
        static int s_browser_target = TARGET_CAST;
        static char s_filter[64] = {0};
        const char* target_items[] = {"Cast Sound", "Impact Sound", "Loop Sound"};
        (void) overlay_combo("Assign To", &s_browser_target, target_items, 3);
        if (overlay_button("Browse Sounds (sounds.cfg)"))
            s_browser_open = 1;
        vchanged |=
            overlay_input_text("Cast Sound Id", vis.cast_sound_id, (int) sizeof vis.cast_sound_id);
        vchanged |= overlay_input_text("Impact Sound Id", vis.impact_sound_id,
                                       (int) sizeof vis.impact_sound_id);
        vchanged |=
            overlay_input_text("Loop Sound Id", vis.loop_sound_id, (int) sizeof vis.loop_sound_id);
        vchanged |= overlay_slider_int("Sound Volume", &vis.sound_volume, 0, 100);
        vchanged |= overlay_slider_float("Sound Pitch Var", &vis.sound_pitch_variance, 0.0f, 12.0f);
        /* Browser window */
        if (s_browser_open)
        {
            overlay_label("Sound ID Browser (from assets/sounds.cfg)");
            overlay_input_text("Filter (substring)", s_filter, (int) sizeof s_filter);
            static char s_ids[512][64];
            static char s_paths[512][260];
            static int s_count = 0;
            FILE* f = fopen("assets/sounds.cfg", "rb");
            if (f)
            {
                char line[512];
                int n = 0;
                while (fgets(line, (int) sizeof line, f) &&
                       n < (int) (sizeof s_ids / sizeof s_ids[0]))
                {
                    /* skip comments/blank */
                    char* p = line;
                    while (*p == ' ' || *p == '\t')
                        ++p;
                    if (*p == '#' || *p == '\0' || *p == '\r' || *p == '\n')
                        continue;
                    char* comma = strchr(p, ',');
                    if (!comma)
                        continue;
                    *comma = '\0';
                    char* id = p;
                    char* path = comma + 1;
                    /* trim id */
                    char* e = id + strlen(id);
                    while (e > id && (e[-1] == ' ' || e[-1] == '\t'))
                        --e;
                    *e = '\0';
                    /* trim path */
                    char* q = path + strlen(path);
                    while (q > path && (q[-1] == '\r' || q[-1] == '\n' || q[-1] == ' '))
                        --q;
                    *q = '\0';
                    if (*id && *path)
                    {
                        strncpy(s_ids[n], id, sizeof s_ids[n] - 1);
                        s_ids[n][sizeof s_ids[n] - 1] = '\0';
                        strncpy(s_paths[n], path, sizeof s_paths[n] - 1);
                        s_paths[n][sizeof s_paths[n] - 1] = '\0';
                        ++n;
                    }
                }
                fclose(f);
                s_count = n;
            }
            else
            {
                overlay_label("(sounds.cfg not found)");
                s_count = 0;
            }
            const char* headers[] = {"Sound Id", "Path"};
            int sc = 0, sd = 0;
            if (overlay_table_begin("sound_id_browser", headers, 2, &sc, &sd, s_filter))
            {
                int selected_row = -1;
                for (int i = 0; i < s_count; ++i)
                {
                    const char* row[2] = {s_ids[i], s_paths[i]};
                    if (overlay_table_row(row, 2, i, &selected_row))
                    {
                        if (s_browser_target == TARGET_CAST)
                        {
                            strncpy(vis.cast_sound_id, s_ids[i], sizeof vis.cast_sound_id - 1);
                            vis.cast_sound_id[sizeof vis.cast_sound_id - 1] = '\0';
                        }
                        else if (s_browser_target == TARGET_IMPACT)
                        {
                            strncpy(vis.impact_sound_id, s_ids[i], sizeof vis.impact_sound_id - 1);
                            vis.impact_sound_id[sizeof vis.impact_sound_id - 1] = '\0';
                        }
                        else
                        {
                            strncpy(vis.loop_sound_id, s_ids[i], sizeof vis.loop_sound_id - 1);
                            vis.loop_sound_id[sizeof vis.loop_sound_id - 1] = '\0';
                        }
                        vchanged = 1;
                        s_browser_open = 0;
                        break;
                    }
                }
                overlay_table_end();
            }
            if (overlay_button("Close Browser"))
                s_browser_open = 0;
        }
        if (vchanged)
        {
            (void) rogue_skill_debug_set_visuals(sel, &vis);
            panel_skills_save_overrides_and_refresh();
        }
    }
}
#endif
