#include "panel_skills_visuals.h"
#include "../../../core/skills/skill_debug.h"
#include "../../../core/skills/skill_sprite_loader.h"
#include "../../overlay_core.h"
#include "../../widgets/overlay_widgets.h"
#include "../../widgets/overlay_widgets_internal.h" /* g_ui for positioning */
#include "../shared/panel_skills_shared.h"
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
    /* Live preview cache/state */
    static RogueTexture s_prev_tex; /* cached texture for preview target */
    static int s_prev_loaded = 0;
    static char s_prev_path[256] = {0};
    static int s_anim_elapsed_ms = 0; /* accumulative for preview */
    static int s_preview_play = 1;    /* play/pause */
    static int s_preview_loop = -1;   /* -1 = follow vis.animation_loops, 0/1 = override */
    static int s_manual_frame = 0;    /* scrub value when paused */
    enum
    {
        PREV_CAST = 0,
        PREV_PROJECTILE = 1,
        PREV_IMPACT = 2,
        PREV_AOE = 3
    };
    static int s_preview_target = PREV_CAST;
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
        /* Unified Asset File Picker (images/audio/json) */
        static int s_asset_picker_open = 0;
        static int s_asset_picker_category = 0; /* 0 Images, 1 Audio, 2 All */
        static char s_asset_filter[64] = {0};
        if (overlay_button("Asset File Picker"))
            s_asset_picker_open = 1;

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
        /* Asset Import Wizard (Phase 2.3) */
        if (overlay_button("Asset Import Wizard"))
        {
            static int s_show_import = 0;
            s_show_import = 1;
        }
        static int s_show_import = 0;
        if (s_show_import)
        {
            /* Temporary fixed position (x=420,y=60,width=360) until layout prefs integrated */
            overlay_begin_panel("Asset Import Wizard", 420, 60, 360);
            overlay_label("Drag & Drop sprite/audio files here or use 'Add Files' (prototype)");
            /* Placeholder: list staged files (non-persistent). Real drag-and-drop not yet
             * integrated. */
            static char s_staged[8][128];
            static int s_staged_count = 0;
            if (overlay_button("Add Files (scan assets/skills)") && s_staged_count < 8)
            {
                /* Placeholder: fabricate demo entries */
                int remaining = 8 - s_staged_count;
                while (remaining-- > 0)
                {
                    snprintf(s_staged[s_staged_count], sizeof s_staged[s_staged_count],
                             "demo_asset_%d.png", s_staged_count);
                    ++s_staged_count;
                }
            }
            for (int i = 0; i < s_staged_count; ++i)
            {
                overlay_label(s_staged[i]);
            }
            if (s_staged_count > 0 && overlay_button("Import (noop prototype)"))
            {
                /* Future: copy into structured skill asset dirs and register overrides */
                /* Clear staged list */
                s_staged_count = 0;
            }
            if (overlay_button("Close"))
                s_show_import = 0;
            overlay_end_panel();
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
            {
                const char* aoe_items[] = {"None", "Circle", "Cone", "Line"};
                vchanged |= overlay_combo("AoE Shape", &vis.aoe_shape, aoe_items, 4);
            }
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
            {
                const char* traj_items[] = {"Linear", "Arc", "Homing", "Scatter"};
                vchanged |= overlay_combo("Trajectory Type", &vis.trajectory_type, traj_items, 4);
            }
            vchanged |= overlay_slider_int("Pierce Count", &vis.pierce_count, 0, 50);
            vchanged |= overlay_slider_float("Homing Strength", &vis.homing_strength, 0.0f, 100.0f);
        }

        /* Prototype: Multi-select status tags (bitfield placeholder, non-persistent) */
        static unsigned int s_status_mask = 0; /* bits map to status effects planned */
        const char* status_items[] = {"Burn", "Freeze", "Shock", "Bleed", "Poison", "Slow"};
        if (overlay_multiselect_bits("Status Tags", status_items,
                                     (int) (sizeof status_items / sizeof status_items[0]),
                                     &s_status_mask))
        {
            /* Future: persist into skill metadata */
        }

        /* Prototype: Dynamic Combo Chain editor (non-persistent example) */
        static int s_combo_count = 0;
        static char s_combo_entries[8][64];
        if (overlay_list_editor("Combo Chain (prototype)", s_combo_entries, &s_combo_count, 8, 64))
        {
            /* Future: authoring integration for melee combo sequences */
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

        /* Asset File Picker implementation */
        if (s_asset_picker_open)
        {
            overlay_label("Asset File Picker");
            const char* cat_items[] = {"Images", "Audio", "All"};
            overlay_combo("Category", &s_asset_picker_category, cat_items, 3);
            overlay_input_text("Filter (substring)", s_asset_filter, (int) sizeof s_asset_filter);
            /* Build list each open for simplicity */
            static char apaths[4096][260];
            int acount = 0;
#if defined(_WIN32)
            typedef struct
            {
                char path[260];
                int depth;
            } APNode;
            APNode stack[512];
            int sp2 = 0;
            strncpy(stack[sp2].path, "assets", sizeof stack[sp2].path - 1);
            stack[sp2].path[sizeof stack[sp2].path - 1] = '\0';
            stack[sp2].depth = 0;
            sp2++;
            while (sp2 > 0)
            {
                APNode cur = stack[--sp2];
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
                    if (is_dir && cur.depth < 4)
                    {
                        if (sp2 < (int) (sizeof stack / sizeof stack[0]))
                        {
                            strncpy(stack[sp2].path, child, sizeof stack[sp2].path - 1);
                            stack[sp2].path[sizeof stack[sp2].path - 1] = '\0';
                            stack[sp2].depth = cur.depth + 1;
                            sp2++;
                        }
                    }
                    else if (!is_dir)
                    {
                        const char* ext = strrchr(child, '.');
                        int keep = 0;
                        if (ext)
                        {
                            char low[8] = {0};
                            for (int i = 0; ext[i] && i < 7; ++i)
                                low[i] = (char) tolower((unsigned char) ext[i]);
                            int is_img = (strcmp(low, ".png") == 0 || strcmp(low, ".bmp") == 0 ||
                                          strcmp(low, ".jpg") == 0 || strcmp(low, ".jpeg") == 0);
                            int is_audio = (strcmp(low, ".wav") == 0 || strcmp(low, ".ogg") == 0 ||
                                            strcmp(low, ".mp3") == 0);
                            if (s_asset_picker_category == 0)
                                keep = is_img;
                            else if (s_asset_picker_category == 1)
                                keep = is_audio;
                            else
                                keep = (is_img || is_audio);
                        }
                        if (keep && acount < (int) (sizeof apaths / sizeof apaths[0]))
                        {
                            char norm[260];
                            snprintf(norm, sizeof norm, "%s", child);
                            for (char* p = norm; *p; ++p)
                                if (*p == '\\')
                                    *p = '/';
                            strncpy(apaths[acount], norm, sizeof apaths[acount] - 1);
                            apaths[acount][sizeof apaths[acount] - 1] = '\0';
                            acount++;
                        }
                    }
                } while (FindNextFileA(h, &fd));
                FindClose(h);
            }
#endif
            const char* headers_ap[] = {"Path"};
            int sca = 0, sda = 0;
            if (overlay_table_begin("asset_file_picker", headers_ap, 1, &sca, &sda, s_asset_filter))
            {
                int selrow = -1;
                for (int i = 0; i < acount; ++i)
                {
                    const char* row[1] = {apaths[i]};
                    if (overlay_table_row(row, 1, i, &selrow))
                    {
                        /* Assign only if an image and target relevant */
                        const char* chosen = apaths[i];
                        const char* ext = strrchr(chosen, '.');
                        int is_img = 0;
                        if (ext)
                        {
                            char lo[8] = {0};
                            for (int k = 0; ext[k] && k < 7; ++k)
                                lo[k] = (char) tolower((unsigned char) ext[k]);
                            is_img = (strcmp(lo, ".png") == 0 || strcmp(lo, ".bmp") == 0 ||
                                      strcmp(lo, ".jpg") == 0 || strcmp(lo, ".jpeg") == 0);
                        }
                        if (is_img)
                        {
                            if (s_browser_target == TARGET_CAST)
                            {
                                strncpy(vis.cast_sprite_sheet, chosen,
                                        sizeof vis.cast_sprite_sheet - 1);
                                vis.cast_sprite_sheet[sizeof vis.cast_sprite_sheet - 1] = '\0';
                            }
                            else if (s_browser_target == TARGET_PROJECTILE)
                            {
                                strncpy(vis.projectile_sprite, chosen,
                                        sizeof vis.projectile_sprite - 1);
                                vis.projectile_sprite[sizeof vis.projectile_sprite - 1] = '\0';
                            }
                            else if (s_browser_target == TARGET_IMPACT)
                            {
                                strncpy(vis.impact_sprite, chosen, sizeof vis.impact_sprite - 1);
                                vis.impact_sprite[sizeof vis.impact_sprite - 1] = '\0';
                            }
                            else if (s_browser_target == TARGET_AOE)
                            {
                                strncpy(vis.aoe_sprite, chosen, sizeof vis.aoe_sprite - 1);
                                vis.aoe_sprite[sizeof vis.aoe_sprite - 1] = '\0';
                            }
                            vchanged = 1;
                            s_asset_picker_open = 0;
                        }
                        else
                        {
                            overlay_label("(Selected file not an image – no assignment)");
                        }
                        break;
                    }
                }
                overlay_table_end();
            }
            if (overlay_button("Close Asset Picker"))
                s_asset_picker_open = 0;
        }

        if (vchanged)
        {
            (void) rogue_skill_debug_set_visuals(sel, &vis);
            panel_skills_save_overrides_and_refresh();
        }

        /* --- Live Preview -------------------------------------------------- */
        overlay_label("Preview");
        /* Choose preview target and resolve path */
        (void) overlay_combo("Preview Target", &s_preview_target,
                             (const char*[]){"Cast Sheet", "Projectile", "Impact", "AoE"}, 4);
        const char* path = NULL;
        if (s_preview_target == PREV_CAST)
            path = vis.cast_sprite_sheet;
        else if (s_preview_target == PREV_PROJECTILE)
            path = vis.projectile_sprite;
        else if (s_preview_target == PREV_IMPACT)
            path = vis.impact_sprite;
        else if (s_preview_target == PREV_AOE)
            path = vis.aoe_sprite;

        /* Reload texture if the path changed */
        if (strncmp(s_prev_path, path ? path : "", sizeof s_prev_path) != 0)
        {
            if (s_prev_loaded)
            {
                rogue_texture_destroy(&s_prev_tex);
                s_prev_loaded = 0;
            }
            s_prev_path[0] = '\0';
            if (path && *path)
            {
                if (rogue_texture_load(&s_prev_tex, path))
                {
                    s_prev_loaded = 1;
                    strncpy(s_prev_path, path, sizeof s_prev_path - 1);
                    s_prev_path[sizeof s_prev_path - 1] = '\0';
                    s_anim_elapsed_ms = 0; /* restart anim on new source */
                    s_manual_frame = 0;
                }
            }
        }

        /* Compute frames for current target */
        RogueSprite frames[512];
        int effective = 0;
        int fdur = (int) (vis.frame_duration_ms > 0.0f ? vis.frame_duration_ms : 100.0f);
        int anim_loops =
            (s_preview_loop < 0) ? (vis.animation_loops ? 1 : 0) : (s_preview_loop ? 1 : 0);
        int can_animate = 0;
        if (s_prev_loaded && s_prev_tex.w > 0 && s_prev_tex.h > 0)
        {
            if (s_preview_target == PREV_CAST && vis.grid_width > 0 && vis.grid_height > 0)
            {
                int max_possible = vis.grid_width * vis.grid_height;
                if (max_possible > (int) (sizeof frames / sizeof frames[0]))
                    max_possible = (int) (sizeof frames / sizeof frames[0]);
                int built = rogue_skill_build_grid_frames(&s_prev_tex, vis.grid_width,
                                                          vis.grid_height, frames, max_possible);
                effective = built;
                if (vis.frame_count > 0 && vis.frame_count < effective)
                    effective = vis.frame_count;
                can_animate = effective > 1;
            }
            else
            {
                /* Single-frame preview (full texture) */
                memset(&frames[0], 0, sizeof(frames[0]));
                frames[0].tex = &s_prev_tex;
                frames[0].sx = 0;
                frames[0].sy = 0;
                frames[0].sw = s_prev_tex.w;
                frames[0].sh = s_prev_tex.h;
                effective = 1;
                can_animate = 0;
            }
        }

        /* Playback controls */
        if (effective > 0)
        {
            /* Play/Pause, Step, Reset on one row via 3 columns */
            if (overlay_columns_begin(3, NULL))
            {
                if (overlay_button(s_preview_play ? "Pause" : "Play"))
                    s_preview_play = !s_preview_play;
                overlay_next_column();
                if (overlay_button("Step Frame"))
                {
                    s_preview_play = 0;
                    s_manual_frame = (s_manual_frame + 1) % (effective > 0 ? effective : 1);
                    s_anim_elapsed_ms = s_manual_frame * (fdur > 1 ? fdur : 100);
                }
                overlay_next_column();
                if (overlay_button("Reset"))
                {
                    s_anim_elapsed_ms = 0;
                    s_manual_frame = 0;
                }
                overlay_columns_end();
            }
            /* Loop override checkbox (preview only) */
            int loop_override =
                (s_preview_loop >= 0) ? s_preview_loop : (vis.animation_loops ? 1 : 0);
            if (overlay_checkbox("Loop (Preview)", &loop_override))
            {
                s_preview_loop = loop_override; /* start overriding */
            }
            /* Advance animation time using overlay dt when playing and animatable */
            if (s_preview_play && can_animate)
            {
                int dt_ms = (int) (overlay_last_dt() * 1000.0f);
                if (dt_ms < 0)
                    dt_ms = 0;
                s_anim_elapsed_ms += dt_ms;
                if (!anim_loops)
                {
                    /* Clamp at end */
                    int max_ms = (effective - 1) * (fdur > 1 ? fdur : 100);
                    if (s_anim_elapsed_ms > max_ms)
                        s_anim_elapsed_ms = max_ms;
                }
            }
        }

        if (effective > 0)
        {
            int idx = 0;
            if (can_animate)
            {
                idx = rogue_skill_anim_sample_index(effective, (fdur > 1 ? fdur : 100),
                                                    s_anim_elapsed_ms, anim_loops);
                if (idx < 0)
                    idx = 0;
                if (idx >= effective)
                    idx = effective - 1;
                if (!s_preview_play)
                {
                    /* When paused, allow scrubbing */
                    s_manual_frame = idx;
                    if (overlay_slider_int("Frame", &s_manual_frame, 0, effective - 1))
                    {
                        if (s_manual_frame < 0)
                            s_manual_frame = 0;
                        if (s_manual_frame >= effective)
                            s_manual_frame = effective - 1;
                        s_anim_elapsed_ms = s_manual_frame * (fdur > 1 ? fdur : 100);
                        idx = s_manual_frame;
                    }
                }
            }
            /* Compute draw position within current panel area */
            int px = g_ui.cur_x;
            int py = g_ui.cur_y + 6;
            int avail_w = g_ui.width;
            int fw = frames[idx].sw > 0 ? frames[idx].sw : 1;
            int fh = frames[idx].sh > 0 ? frames[idx].sh : 1;
            int scale = avail_w / fw;
            if (scale < 1)
                scale = 1;
            if (scale > 6)
                scale = 6;
            rogue_sprite_draw(&frames[idx], px, py, scale);
            /* Reserve some vertical space under the preview to avoid overlap */
            g_ui.cur_y = py + fh * scale + 12;
        }
        else
        {
            overlay_label("(Set a sprite path; for Cast, also set Grid WxH to animate)");
        }
    }
}
#endif
