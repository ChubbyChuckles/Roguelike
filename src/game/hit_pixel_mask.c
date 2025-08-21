#include "game/hit_pixel_mask.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

int g_hit_use_pixel_masks = 0; /* default off until pixel path validated */

#define MAX_PIXEL_MASK_SETS 16
static RogueHitPixelMaskSet g_sets[MAX_PIXEL_MASK_SETS];
static int g_set_count = 0;

static RogueHitPixelMaskSet* find_set(int weapon_id)
{
    for (int i = 0; i < g_set_count; i++)
        if (g_sets[i].weapon_id == weapon_id)
            return &g_sets[i];
    return NULL;
}

static void alloc_frame(RogueHitPixelMaskFrame* f, int w, int h)
{
    if (!f)
        return;
    if (w <= 0 || h <= 0)
        return;
    f->width = w;
    f->height = h;
    f->origin_x = 0;
    f->origin_y = 0;
    f->pitch_words = (w + 31) / 32;
    size_t words = (size_t) f->pitch_words * (size_t) h;
    f->bits = (uint32_t*) malloc(words * sizeof(uint32_t));
    if (f->bits)
        memset(f->bits, 0, words * sizeof(uint32_t));
}

RogueHitPixelMaskSet* rogue_hit_pixel_masks_ensure(int weapon_id)
{
    RogueHitPixelMaskSet* s = find_set(weapon_id);
    if (s && s->ready)
        return s;
    if (!s)
    {
        if (g_set_count >= MAX_PIXEL_MASK_SETS)
            return NULL;
        s = &g_sets[g_set_count++];
        memset(s, 0, sizeof *s);
        s->weapon_id = weapon_id;
    }
    /* Placeholder generation: simple horizontal bar mask representing weapon length */
    s->frame_count = 8;
    for (int i = 0; i < 8; i++)
    {
        alloc_frame(&s->frames[i], 48, 16); /* 48x16 area */
        /* Fill a simple line with thickness 4 pixels; simulate slight forward progression per frame
         */
        int advance = i * 4;
        if (advance > 24)
            advance = 24;
        for (int y = 6; y < 10; y++)
        {
            for (int x = advance; x < advance + 24; ++x)
            {
                rogue_hit_mask_set(&s->frames[i], x, y);
            }
        }
    }
    s->ready = 1;
    return s;
}

void rogue_hit_pixel_masks_reset_all(void)
{
    for (int i = 0; i < g_set_count; i++)
    {
        for (int f = 0; f < g_sets[i].frame_count; ++f)
        {
            free(g_sets[i].frames[f].bits);
            g_sets[i].frames[f].bits = NULL;
        }
    }
    g_set_count = 0;
}
void rogue_hit_mask_frame_aabb(const RogueHitPixelMaskFrame* f, int* out_w, int* out_h)
{
    if (out_w)
        *out_w = f ? f->width : 0;
    if (out_h)
        *out_h = f ? f->height : 0;
}
int rogue_hit_mask_enemy_test(const RogueHitPixelMaskFrame* f, float enemy_cx_local,
                              float enemy_cy_local, float enemy_radius, int* out_lx, int* out_ly)
{
    if (!f || !f->bits)
        return 0; /* sample center first */
    int cx = (int) enemy_cx_local, cy = (int) enemy_cy_local;
    if (rogue_hit_mask_test(f, cx, cy))
    {
        if (out_lx)
            *out_lx = cx;
        if (out_ly)
            *out_ly = cy;
        return 1;
    } /* ring sample 8 points */
    float r = enemy_radius * 0.7f;
    for (int i = 0; i < 8; i++)
    {
        float ang = (float) (i * 3.14159265358979323846 * 0.25);
        float sx = enemy_cx_local + r * (float) cos(ang);
        float sy = enemy_cy_local + r * (float) sin(ang);
        int ix = (int) sx, iy = (int) sy;
        if (rogue_hit_mask_test(f, ix, iy))
        {
            if (out_lx)
                *out_lx = ix;
            if (out_ly)
                *out_ly = iy;
            return 1;
        }
    }
    return 0;
}
void rogue_hit_mask_local_pixel_to_world(const RogueHitPixelMaskFrame* f, int lx, int ly,
                                         float player_x, float player_y, float pose_dx,
                                         float pose_dy, float scale, float angle_rad, float* out_wx,
                                         float* out_wy)
{
    if (!f)
    {
        if (out_wx)
            *out_wx = player_x;
        if (out_wy)
            *out_wy = player_y;
        return;
    }
    float x = (float) (lx - f->origin_x + 0.5f) * scale;
    float y = (float) (ly - f->origin_y + 0.5f) * scale;
    float ca = (float) cos(angle_rad), sa = (float) sin(angle_rad);
    float rx = x * ca - y * sa;
    float ry = x * sa + y * ca;
    if (out_wx)
        *out_wx = player_x + pose_dx + rx;
    if (out_wy)
        *out_wy = player_y + pose_dy + ry;
}
