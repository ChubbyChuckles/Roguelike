#include "player_render.h"
#include "../../game/hit_system.h"
#include "../../game/weapon_pose.h"
#include "../../graphics/font.h"
#include "../../graphics/sprite.h"
#include "../scene_drawlist.h"
#include <math.h>
#include <stddef.h>
#include <string.h>
#ifdef ROGUE_HAVE_SDL
extern void draw_text(int x, int y, const char* msg);
#endif
#ifdef ROGUE_HAVE_SDL
/* drawlist API for weapon overlay */
void rogue_scene_drawlist_push_weapon_overlay(void* sdl_texture, float x, float y, float w, float h,
                                              float pivot_x, float pivot_y, float angle_deg,
                                              int flip, unsigned char r, unsigned char g,
                                              unsigned char b, unsigned char a);
#endif
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

void rogue_player_render(void)
{
#ifdef ROGUE_HAVE_SDL
    if (!g_app.renderer)
        return;
    if (!g_app.player_loaded)
        return;
    int tsz = g_app.tile_size;
    int scale = 1;
    int dir = g_app.player.facing;
    int sheet_dir = (dir == 1 || dir == 2) ? 1 : dir;
    int render_state = g_app.player_state;
    if (g_app.player_combat.phase == ROGUE_ATTACK_WINDUP ||
        g_app.player_combat.phase == ROGUE_ATTACK_STRIKE ||
        g_app.player_combat.phase == ROGUE_ATTACK_RECOVER)
        render_state = 3;
    const RogueSprite* spr = &g_app.player_frames[render_state][sheet_dir][g_app.player.anim_frame];
    if (!spr->sw)
    {
        for (int f = 0; f < 8; f++)
        {
            if (g_app.player_frames[render_state][sheet_dir][f].sw)
            {
                spr = &g_app.player_frames[render_state][sheet_dir][f];
                break;
            }
        }
    }
    if (spr->sw && spr->tex && spr->tex->handle)
    {
        int px = (int) (g_app.player.base.pos.x * tsz * scale - g_app.cam_x);
        int py = (int) (g_app.player.base.pos.y * tsz * scale - g_app.cam_y);
        if (g_app.levelup_aura_timer_ms > 0.0f)
        {
            g_app.levelup_aura_timer_ms -= (float) (g_app.dt * 1000.0);
            float tnorm = g_app.levelup_aura_timer_ms / 2000.0f;
            if (tnorm < 0)
                tnorm = 0;
            if (tnorm > 1)
                tnorm = 1;
            float pulse =
                0.5f + 0.5f * (float) sin((2000.0f - g_app.levelup_aura_timer_ms) * 0.025f);
            int radius = (int) (spr->sw * scale * (1.2f + 0.3f * (1.0f - tnorm)));
            int cx = px + spr->sw * scale / 2;
            int cy = py + spr->sh * scale / 2;
            unsigned char cr = (unsigned char) (120 + 90 * pulse);
            unsigned char cg = (unsigned char) (80 + 120 * pulse);
            unsigned char cb = 255;
            unsigned char ca = (unsigned char) (120 * tnorm + 60);
            SDL_SetRenderDrawColor(g_app.renderer, cr, cg, cb, ca);
            for (int dy = -radius; dy <= radius; ++dy)
            {
                int dx_lim = (int) sqrt(radius * radius - dy * dy);
                SDL_RenderDrawLine(g_app.renderer, cx - dx_lim, cy + dy, cx + dx_lim, cy + dy);
            }
        }
        int dst_x = px;
        int dst_y = py;
        int y_base = py + spr->sh / 2;
        int flip_flag = (dir == 1) ? 1 : 0;
        rogue_scene_drawlist_push_sprite(spr, dst_x, dst_y, y_base, flip_flag, 255, 255, 255, 255);
        if (render_state == 3)
        {
            int wid = g_app.player.equipped_weapon_id;
            int dir_group = (dir == 3) ? 1 : (dir == 0 ? 0 : 2);
            int facing_left = (dir == 1);
            if (rogue_weapon_pose_ensure_dir(wid, dir_group))
            {
                const RogueWeaponPoseFrame* pf =
                    rogue_weapon_pose_get_dir(wid, dir_group, g_app.player.anim_frame);
                if (pf)
                {
                    int ww = 0, wh = 0;
                    void* tex = rogue_weapon_pose_get_texture_single(wid, &ww, &wh);
                    if (tex)
                    {
                        float center_x = px + spr->sw * 0.5f;
                        float center_y = py + spr->sh * 0.5f;
                        float eff_dx = rogue_weapon_pose_effective_dx(pf, facing_left);
                        float eff_angle = pf->angle * (facing_left ? -1.0f : 1.0f);
                        float wdst = ww * pf->scale;
                        float hdst = wh * pf->scale;
                        float draw_x = center_x + eff_dx - wdst * pf->pivot_x;
                        float draw_y = center_y + pf->dy - hdst * pf->pivot_y;
                        rogue_scene_drawlist_push_weapon_overlay(
                            tex, draw_x, draw_y, wdst, hdst, pf->pivot_x, pf->pivot_y, eff_angle,
                            facing_left, 255, 255, 255, 255);
                    }
                }
            }
        }
    } /* end sprite present */
    else
    {
        SDL_SetRenderDrawColor(g_app.renderer, 255, 0, 255, 255);
        SDL_Rect pr = {(int) (g_app.player.base.pos.x * tsz * scale - g_app.cam_x),
                       (int) (g_app.player.base.pos.y * tsz * scale - g_app.cam_y),
                       g_app.player_frame_size * scale, g_app.player_frame_size * scale};
        SDL_RenderFillRect(g_app.renderer, &pr);
    }
    if (g_hit_debug_enabled)
    {
        const RogueHitDebugFrame* dbg = rogue_hit_debug_last();
        if (dbg && dbg->capsule_valid)
        {
            /* Offset by half a tile so physics (top-left anchored) appears centered on sprites */
            const float anchor = 0.5f;
            SDL_SetRenderDrawColor(g_app.renderer, 0, 220, 255, 180);
            int sx = (int) (((dbg->last_capsule.x0 + anchor) * tsz) - g_app.cam_x);
            int sy = (int) (((dbg->last_capsule.y0 + anchor) * tsz) - g_app.cam_y);
            int ex = (int) (((dbg->last_capsule.x1 + anchor) * tsz) - g_app.cam_x);
            int ey = (int) (((dbg->last_capsule.y1 + anchor) * tsz) - g_app.cam_y);
            SDL_RenderDrawLine(g_app.renderer, sx, sy, ex, ey);
            int rr = (int) (dbg->last_capsule.r * tsz);
            for (int dy = -rr; dy <= rr; ++dy)
            {
                int dx_lim = (int) sqrt(rr * rr - dy * dy);
                SDL_RenderDrawLine(g_app.renderer, sx - dx_lim, sy + dy, sx + dx_lim, sy + dy);
                SDL_RenderDrawLine(g_app.renderer, ex - dx_lim, ey + dy, ex + dx_lim, ey + dy);
            }
            /* Draw enemy circles (collision approximation) */
            const RogueHitboxTuning* tune = rogue_hitbox_tuning_get();
            float enemy_r_cfg = (tune->enemy_radius > 0) ? tune->enemy_radius : 0.40f;
            /* Iterate full capacity instead of enemy_count to avoid skipping alive enemies with
             * index >= enemy_count when holes exist. */
            for (int ei = 0; ei < ROGUE_MAX_ENEMIES; ++ei)
            {
                if (!g_app.enemies[ei].alive)
                    continue;
                float exw = g_app.enemies[ei].base.pos.x + anchor + tune->enemy_offset_x;
                float eyw = g_app.enemies[ei].base.pos.y + anchor + tune->enemy_offset_y;
                int ecx = (int) (exw * tsz - g_app.cam_x);
                int ecy = (int) (eyw * tsz - g_app.cam_y);
                int er = (int) (enemy_r_cfg * tsz);
                SDL_SetRenderDrawColor(g_app.renderer, 40, 255, 120, 120);
                for (int dy = -er; dy <= er; ++dy)
                {
                    int dx_lim = (int) sqrt(er * er - dy * dy);
                    SDL_RenderDrawLine(g_app.renderer, ecx - dx_lim, ecy + dy, ecx + dx_lim,
                                       ecy + dy);
                }
            }
            /* Draw enemy pursuit target (AI path destination relative to player) */
            if (tune)
            {
                float txw = g_app.player.base.pos.x + tune->pursue_offset_x + anchor;
                float tyw = g_app.player.base.pos.y + tune->pursue_offset_y + anchor;
                int tcx = (int) (txw * tsz - g_app.cam_x);
                int tcy = (int) (tyw * tsz - g_app.cam_y);
                int tr = 4; /* fixed small radius in screen pixels */
                SDL_SetRenderDrawColor(g_app.renderer, 255, 40, 40, 210);
                for (int dy = -tr; dy <= tr; ++dy)
                {
                    int dx_lim = (int) sqrt(tr * tr - dy * dy);
                    SDL_RenderDrawLine(g_app.renderer, tcx - dx_lim, tcy + dy, tcx + dx_lim,
                                       tcy + dy);
                }
            }
            /* Highlight hits + normals */
            SDL_SetRenderDrawColor(g_app.renderer, 255, 235, 0, 200);
            for (int i = 0; i < dbg->hit_count; i++)
            {
                int ei = dbg->last_hits[i];
                if (ei >= 0 && ei < ROGUE_MAX_ENEMIES && g_app.enemies[ei].alive)
                {
                    int hx = (int) (((g_app.enemies[ei].base.pos.x + anchor) * tsz) - g_app.cam_x);
                    int hy = (int) (((g_app.enemies[ei].base.pos.y + anchor) * tsz) - g_app.cam_y);
                    SDL_Rect r = {hx - 2, hy - 2, 4, 4};
                    SDL_RenderFillRect(g_app.renderer, &r);
                    SDL_SetRenderDrawColor(g_app.renderer, 255, 120, 0, 230);
                    int nx = (int) (hx + dbg->normals[i][0] * 12);
                    int ny = (int) (hy + dbg->normals[i][1] * 12);
                    SDL_RenderDrawLine(g_app.renderer, hx, hy, nx, ny);
                    SDL_SetRenderDrawColor(g_app.renderer, 255, 235, 0, 200);
                }
            }
        }
        /* Integrated pixel mask visualization (shares same toggle) */
        if (dbg && dbg->pixel_mask_valid && dbg->mask_bits)
        {
            /* Pose dx/dy captured as pixel offsets (NOT tiles). Player base position is world
             * tiles; convert to screen pixels first, then apply offsets. */
            int player_px = (int) (g_app.player.base.pos.x * tsz - g_app.cam_x);
            int player_py = (int) (g_app.player.base.pos.y * tsz - g_app.cam_y);
            int player_cx = player_px + tsz / 2; /* approximate sprite center */
            int player_cy = player_py + tsz / 2;
            const RogueHitboxTuning* tune = rogue_hitbox_tuning_get();
            int facing = g_app.player.facing;
            if (facing < 0 || facing > 3)
                facing = 0;
            /* Use captured pose + scale directly (avoid double applying tuning which is already in
             * capture) */
            float live_dx = dbg->mask_pose_dx;
            float live_dy = dbg->mask_pose_dy;
            float scale_x = dbg->mask_scale_x;
            float scale_y = dbg->mask_scale_y;
            int ox = (int) (player_cx + live_dx); /* rotation origin (weapon pivot) */
            int oy = (int) (player_cy + live_dy);
            /* AABB for visualization */
            int base_x = ox - (int) (dbg->mask_origin_x * scale_x);
            int base_y = oy - (int) (dbg->mask_origin_y * scale_y);
            SDL_Rect aabb = {base_x, base_y, (int) (dbg->mask_w * scale_x),
                             (int) (dbg->mask_h * scale_y)};
            SDL_SetRenderDrawColor(g_app.renderer, 60, 60, 60, 120);
            SDL_RenderDrawRect(g_app.renderer, &aabb);
            int step = (dbg->mask_w * dbg->mask_h > 8000) ? 2 : 1;
            for (int y = 0; y < dbg->mask_h; y += step)
            {
                const uint32_t* row = dbg->mask_bits + (size_t) y * dbg->mask_pitch_words;
                for (int x = 0; x < dbg->mask_w; x += step)
                {
                    uint32_t w = row[x >> 5];
                    if (w & (1u << (x & 31)))
                    {
                        int sx = base_x + (int) (x * scale_x);
                        int sy = base_y + (int) (y * scale_y);
                        SDL_SetRenderDrawColor(g_app.renderer, 120, 120, 255, 180);
                        SDL_RenderDrawPoint(g_app.renderer, sx, sy);
                    }
                }
            }
            SDL_SetRenderDrawColor(g_app.renderer, 255, 0, 255, 200);
            SDL_RenderDrawLine(g_app.renderer, ox - 5, oy, ox + 5, oy);
            SDL_RenderDrawLine(g_app.renderer, ox, oy - 5, ox, oy + 5);
            /* Display quick tuning HUD segment */
            char tune_buf[96];
            snprintf(tune_buf, sizeof(tune_buf), "F%d dx=%.0f dy=%.0f sx=%.2f sy=%.2f", facing,
                     tune->mask_dx[facing], tune->mask_dy[facing],
                     tune->mask_scale_x[facing] > 0 ? tune->mask_scale_x[facing] : 1.0f,
                     tune->mask_scale_y[facing] > 0 ? tune->mask_scale_y[facing] : 1.0f);
            SDL_Rect tb = {base_x, base_y - 14, (int) strlen(tune_buf) * 6, 10};
            SDL_SetRenderDrawColor(g_app.renderer, 0, 0, 0, 150);
            SDL_RenderFillRect(g_app.renderer, &tb);
            /* reuse debug tiny font */
            draw_text(base_x + 2, base_y - 14, tune_buf);
        }
    }
#endif
}
