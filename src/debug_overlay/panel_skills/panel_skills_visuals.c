#include "panel_skills_visuals.h"
#include "../../core/skills/skill_debug.h"
#include "../overlay_core.h"
#include "../widgets/overlay_widgets.h"
#include "panel_skills_shared.h"
#include <ctype.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#if ROGUE_ENABLE_DEBUG_OVERLAY
void panel_skills_draw_visuals(int sel)
{
    overlay_label("Visuals");
    static RogueSkillVisualParams vis;
    if (rogue_skill_debug_get_visuals(sel, &vis) == 0)
    {
        int stype = 0;
        (void) rogue_skill_debug_get_type(sel, &stype);
        int vchanged = 0;
        /* --- Sprite Sheet Browser (simple, inline) --- */
        enum
        {
            TARGET_CAST = 0,
            TARGET_PROJECTILE = 1,
            TARGET_IMPACT = 2,
            TARGET_AOE = 3
        };
        static int s_browser_open = 0;
        static int s_browser_target = TARGET_CAST;
        static char s_filter[64] = {0};

        const char* target_items[] = {"Cast Sheet", "Projectile", "Impact", "AoE"};
        (void) overlay_combo("Assign To", &s_browser_target, target_items, 4);

        if (overlay_button("Browse Sprites (assets/*)"))
            s_browser_open = 1;

        /* Grid Configurator quick helpers (doesn't mutate silently) */
        int derived_total =
            (vis.grid_width > 0 && vis.grid_height > 0) ? (vis.grid_width * vis.grid_height) : 0;
        if (derived_total > 0)
        {
            char buf[96];
            snprintf(buf, sizeof buf, "Grid Cells: %d x %d  (max frames: %d)", vis.grid_width,
                     vis.grid_height, derived_total);
            overlay_label(buf);
            if (overlay_button("Set Frame Count = Grid Cells"))
            {
                vis.frame_count = derived_total;
                vchanged = 1;
            }
        }

        /* Core animation/sprite-sheet (non-passive) */
        if (stype != 8 /* PASSIVE */)
        {
            vchanged |= overlay_input_text("Cast Sprite Sheet", vis.cast_sprite_sheet,
                                           (int) sizeof vis.cast_sprite_sheet);
            vchanged |= overlay_slider_int("Frame Count", &vis.frame_count, 0, 512);
            vchanged |=
                overlay_slider_float("Frame Duration (ms)", &vis.frame_duration_ms, 0.0f, 2000.0f);
            vchanged |= overlay_checkbox("Animation Loops", &vis.animation_loops);
            vchanged |= overlay_slider_int("Grid Width", &vis.grid_width, 0, 128);
            vchanged |= overlay_slider_int("Grid Height", &vis.grid_height, 0, 128);
        }
        /* Impact sprite is common */
        vchanged |=
            overlay_input_text("Impact Sprite", vis.impact_sprite, (int) sizeof vis.impact_sprite);
        /* AoE fields for AoE spells */
        if (stype == 3 /* AOE_SPELL */)
        {
            vchanged |=
                overlay_input_text("AoE Sprite", vis.aoe_sprite, (int) sizeof vis.aoe_sprite);
            vchanged |= overlay_slider_int("AoE Shape (0 none,1 circle,2 cone,3 line)",
                                           &vis.aoe_shape, 0, 3);
            vchanged |= overlay_slider_float("AoE Radius", &vis.aoe_radius, 0.0f, 1000.0f);
            vchanged |= overlay_slider_float("AoE Angle", &vis.aoe_angle, 0.0f, 360.0f);
        }
        /* Projectile fields for ranged */
        if (stype == 2 /* RANGED */)
        {
            vchanged |= overlay_input_text("Projectile Sprite", vis.projectile_sprite,
                                           (int) sizeof vis.projectile_sprite);
            vchanged |= overlay_slider_float("Projectile Velocity", &vis.projectile_velocity, 0.0f,
                                             5000.0f);
            vchanged |= overlay_slider_int("Trajectory Type (0 lin,1 arc,2 homing,3 scatter)",
                                           &vis.trajectory_type, 0, 3);
            vchanged |= overlay_slider_int("Pierce Count", &vis.pierce_count, 0, 50);
            vchanged |= overlay_slider_float("Homing Strength", &vis.homing_strength, 0.0f, 100.0f);
        }

        /* Inline sprite browser: shallow recursive scan under assets/ and filter */
        if (s_browser_open)
        {
            overlay_label("Sprite Browser");
            overlay_input_text("Filter (substring)", s_filter, sizeof s_filter);

            /* dynamic list backed by a static array for simplicity */
            static char s_paths[2048][260];
            static int s_count = -1; /* -1 means not scanned yet */

            /* Reset and rescan each open to keep it simple */
            s_count = 0;

#if defined(_WIN32)
            /* BFS-like shallow recursion (depth<=3) */
            typedef struct
            {
                char path[260];
                int depth;
            } Node;
            Node stack[256];
            int sp = 0;
            strncpy(stack[sp].path, "assets", sizeof stack[sp].path - 1);
            stack[sp].path[sizeof stack[sp].path - 1] = '\0';
            stack[sp].depth = 0;
            sp++;
            while (sp > 0)
            {
                Node cur = stack[--sp];
                char pattern[300];
                snprintf(pattern, sizeof pattern, "%s\\*", cur.path);
                WIN32_FIND_DATAA fd;
                HANDLE h = FindFirstFileA(pattern, &fd);
                if (h == INVALID_HANDLE_VALUE)
                    continue;
                do
                {
                    if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
                        continue;
                    int is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                    char child[300];
                    snprintf(child, sizeof child, "%s\\%s", cur.path, fd.cFileName);
                    if (is_dir && cur.depth < 3)
                    {
                        if (sp < (int) (sizeof stack / sizeof stack[0]))
                        {
                            strncpy(stack[sp].path, child, sizeof stack[sp].path - 1);
                            stack[sp].path[sizeof stack[sp].path - 1] = '\0';
                            stack[sp].depth = cur.depth + 1;
                            sp++;
                        }
                    }
                    else if (!is_dir)
                    {
                        const char* ext = strrchr(child, '.');
                        if (ext)
                        {
                            char el[8] = {0};
                            for (int i = 0; ext[i] && i < 7; ++i)
                                el[i] = (char) tolower((unsigned char) ext[i]);
                            if (strcmp(el, ".png") == 0 || strcmp(el, ".bmp") == 0 ||
                                strcmp(el, ".jpg") == 0 || strcmp(el, ".jpeg") == 0)
                            {
                                if (s_count < (int) (sizeof s_paths / sizeof s_paths[0]))
                                {
                                    /* normalize slashes to forward */
                                    char norm[260];
                                    snprintf(norm, sizeof norm, "%s", child);
                                    for (char* p = norm; *p; ++p)
                                        if (*p == '\\')
                                            *p = '/';
                                    strncpy(s_paths[s_count], norm, sizeof s_paths[s_count] - 1);
                                    s_paths[s_count][sizeof s_paths[s_count] - 1] = '\0';
                                    s_count++;
                                }
                            }
                        }
                    }
                } while (FindNextFileA(h, &fd));
                FindClose(h);
            }
#else
            (void) s_paths;
            (void) s_count; /* non-Windows: browser not implemented */
#endif

            /* Show as table */
            const char* headers[] = {"Path (assets/...)"};
            int sc = 0, sd = 0;
            if (overlay_table_begin("sprite_browser", headers, 1, &sc, &sd, s_filter))
            {
                int selected_row = -1;
                for (int i = 0; i < s_count; ++i)
                {
                    const char* row[1] = {s_paths[i]};
                    if (overlay_table_row(row, 1, i, &selected_row))
                    {
                        /* Assign and close */
                        const char* chosen = s_paths[i];
                        if (s_browser_target == TARGET_CAST)
                            strncpy(vis.cast_sprite_sheet, chosen,
                                    sizeof vis.cast_sprite_sheet - 1),
                                vis.cast_sprite_sheet[sizeof vis.cast_sprite_sheet - 1] = '\0';
                        else if (s_browser_target == TARGET_PROJECTILE)
                            strncpy(vis.projectile_sprite, chosen,
                                    sizeof vis.projectile_sprite - 1),
                                vis.projectile_sprite[sizeof vis.projectile_sprite - 1] = '\0';
                        else if (s_browser_target == TARGET_IMPACT)
                            strncpy(vis.impact_sprite, chosen, sizeof vis.impact_sprite - 1),
                                vis.impact_sprite[sizeof vis.impact_sprite - 1] = '\0';
                        else if (s_browser_target == TARGET_AOE)
                            strncpy(vis.aoe_sprite, chosen, sizeof vis.aoe_sprite - 1),
                                vis.aoe_sprite[sizeof vis.aoe_sprite - 1] = '\0';
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
