#include "game/lock_on.h"
#include <float.h>
#include <math.h>
#include <stdio.h>

static float rogue_lockon_vec_len2(float x, float y) { return x * x + y * y; }
static float rogue_lockon_angle(float dx, float dy) { return (float) atan2(dy, dx); }

void rogue_lockon_reset(RoguePlayer* p)
{
    if (!p)
        return;
    p->lock_on_active = 0;
    p->lock_on_target_index = -1;
    p->lock_on_switch_cooldown_ms = 0;
    if (p->lock_on_radius <= 0.0f)
        p->lock_on_radius = 6.0f;
}

/* Internal candidate gather: returns number of valid targets within radius (fills idxs up to max).
 */
static int rogue_lockon_collect(RoguePlayer* p, RogueEnemy enemies[], int enemy_count,
                                int* out_indices, int max_out)
{
    if (!p || !enemies)
        return 0;
    if (enemy_count <= 0)
        return 0;
    int c = 0;
    float pr = p->lock_on_radius;
    float pr2 = pr * pr;
    float px = p->base.pos.x;
    float py = p->base.pos.y;
    int scan = enemy_count;
    if (scan > ROGUE_MAX_ENEMIES)
        scan = ROGUE_MAX_ENEMIES;
    for (int i = 0; i < scan && c < max_out; i++)
    {
        if (!enemies[i].alive)
            continue;
        float dx = enemies[i].base.pos.x - px;
        float dy = enemies[i].base.pos.y - py;
        if (rogue_lockon_vec_len2(dx, dy) <= pr2)
        {
            out_indices[c++] = i;
        }
    }
    return c;
}

int rogue_lockon_acquire(RoguePlayer* p, RogueEnemy enemies[], int enemy_count)
{
    if (!p || !enemies || enemy_count <= 0)
        return 0;
    int idxs[256];
    int n = rogue_lockon_collect(p, enemies, enemy_count, idxs, 256);
    if (n <= 0)
    {
        p->lock_on_active = 0;
        p->lock_on_target_index = -1;
        return 0;
    }
#ifdef COMBAT_DEBUG
    printf("[lock_on_acquire] candidates=%d radius=%.2f\n", n, p->lock_on_radius);
#endif
    float best_score = FLT_MAX;
    int best = -1;
    float px = p->base.pos.x;
    float py = p->base.pos.y;
    float facing_dx = 0, facing_dy = 1; /* approximate facing vector */
    switch (p->facing)
    {
    case 0:
        facing_dx = 0;
        facing_dy = 1;
        break;
    case 1:
        facing_dx = -1;
        facing_dy = 0;
        break;
    case 2:
        facing_dx = 1;
        facing_dy = 0;
        break;
    case 3:
        facing_dx = 0;
        facing_dy = -1;
        break;
    }
    for (int k = 0; k < n; k++)
    {
        int ei = idxs[k];
        float dx = enemies[ei].base.pos.x - px;
        float dy = enemies[ei].base.pos.y - py;
        float d2 = dx * dx + dy * dy;
        if (d2 < 0.0001f)
            d2 = 0.0001f;
        float dist = d2; /* squared distance primary */
        float norm = (float) sqrt(d2);
        dx /= norm;
        dy /= norm;
        float ang_bias = 1.0f - (dx * facing_dx + dy * facing_dy);
        if (ang_bias < 0)
            ang_bias = 0; /* smaller is better */
        float score = dist * 1.0f + ang_bias * 0.15f;
        if (score < best_score)
        {
            best_score = score;
            best = ei;
        }
    }
    if (best >= 0)
    {
        p->lock_on_active = 1;
        p->lock_on_target_index = best;
        p->lock_on_switch_cooldown_ms = 0;
        return 1;
    }
    p->lock_on_active = 0;
    p->lock_on_target_index = -1;
    return 0;
}

void rogue_lockon_validate(RoguePlayer* p, RogueEnemy enemies[], int enemy_count)
{
    if (!p)
        return;
    if (!enemies || enemy_count <= 0)
        return;
    if (!p->lock_on_active)
        return;
    int i = p->lock_on_target_index;
    if (i < 0 || i >= enemy_count || !enemies[i].alive)
    {
        p->lock_on_active = 0;
        p->lock_on_target_index = -1;
        return;
    }
    float dx = enemies[i].base.pos.x - p->base.pos.x;
    float dy = enemies[i].base.pos.y - p->base.pos.y;
    float r2 = dx * dx + dy * dy;
    float maxr = p->lock_on_radius * 1.25f;
    if (r2 > maxr * maxr)
    {
        p->lock_on_active = 0;
        p->lock_on_target_index = -1;
    }
}

int rogue_lockon_cycle(RoguePlayer* p, RogueEnemy enemies[], int enemy_count, int direction)
{
    if (!p || !enemies || enemy_count <= 0)
        return 0;
    if (p->lock_on_switch_cooldown_ms > 0)
        return 0;
    int idxs[256];
    int n = rogue_lockon_collect(p, enemies, enemy_count, idxs, 256);
    if (n <= 1)
    {
        return 0;
    }
    /* Sort candidates by angle around player */
    float angles[256];
    float px = p->base.pos.x;
    float py = p->base.pos.y;
    for (int i = 0; i < n; i++)
    {
        float dx = enemies[idxs[i]].base.pos.x - px;
        float dy = enemies[idxs[i]].base.pos.y - py;
        angles[i] = rogue_lockon_angle(dx, dy);
    }
    /* Simple insertion sort (n small) */
    for (int i = 1; i < n; i++)
    {
        int ii = idxs[i];
        float aa = angles[i];
        int j = i - 1;
        while (j >= 0 && angles[j] > aa)
        {
            angles[j + 1] = angles[j];
            idxs[j + 1] = idxs[j];
            j--;
        }
        angles[j + 1] = aa;
        idxs[j + 1] = ii;
    }
    /* Find current index in ordered list */
    int cur_pos = -1;
    for (int i = 0; i < n; i++)
    {
        if (idxs[i] == p->lock_on_target_index)
        {
            cur_pos = i;
            break;
        }
    }
    if (cur_pos < 0)
    { /* if lost, auto acquire first */
        p->lock_on_target_index = idxs[0];
        p->lock_on_active = 1;
        return 1;
    }
    int next = (cur_pos + (direction > 0 ? 1 : -1) + n) % n;
    if (idxs[next] == p->lock_on_target_index)
        return 0;
    p->lock_on_target_index = idxs[next];
    p->lock_on_active = 1;
    p->lock_on_switch_cooldown_ms = 180.0f;
    return 1;
}

void rogue_lockon_tick(RoguePlayer* p, float dt_ms)
{
    if (!p)
        return;
    if (p->lock_on_switch_cooldown_ms > 0)
    {
        p->lock_on_switch_cooldown_ms -= dt_ms;
        if (p->lock_on_switch_cooldown_ms < 0)
            p->lock_on_switch_cooldown_ms = 0;
    }
}

int rogue_lockon_get_dir(RoguePlayer* p, RogueEnemy enemies[], int enemy_count, float* out_dx,
                         float* out_dy)
{
    if (!p)
        return 0;
    if (!enemies || enemy_count <= 0)
        return 0;
    rogue_lockon_validate(p, enemies, enemy_count);
    if (!p->lock_on_active)
        return 0;
    int i = p->lock_on_target_index;
    if (i < 0 || i >= enemy_count)
        return 0;
    if (!enemies[i].alive)
        return 0;
    float dx = enemies[i].base.pos.x - p->base.pos.x;
    float dy = enemies[i].base.pos.y - p->base.pos.y;
    float len = (float) sqrt(dx * dx + dy * dy);
    if (len < 0.0001f)
        return 0;
    dx /= len;
    dy /= len;
    if (out_dx)
        *out_dx = dx;
    if (out_dy)
        *out_dy = dy; /* also snap facing  (4-dir) */
    float ax = fabsf(dx);
    float ay = fabsf(dy);
    int f = 0;
    if (ax > ay)
    {
        f = (dx < 0) ? 1 : 2;
    }
    else
    {
        f = (dy < 0) ? 3 : 0;
    }
    p->facing = f;
    return 1;
}
