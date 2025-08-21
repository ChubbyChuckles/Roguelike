#include "game/hit_feedback.h"
#include "core/app_state.h"
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <stdlib.h>
#include <string.h>

static RogueHitParticle g_particles[ROGUE_HIT_PARTICLE_MAX];
static int g_particle_count = 0;
static int g_last_explosion_frame = -1000;

void rogue_hit_particles_reset(void)
{
    g_particle_count = 0;
    memset(g_particles, 0, sizeof g_particles);
}

void rogue_hit_particles_update(float dt_ms)
{
    for (int i = 0; i < g_particle_count;)
    {
        RogueHitParticle* p = &g_particles[i];
        p->age_ms += dt_ms;
        if (p->age_ms >= p->lifetime_ms)
        {
            g_particles[i] = g_particles[g_particle_count - 1];
            g_particle_count--;
            continue;
        }
        float life_frac = p->age_ms / p->lifetime_ms;
        (void) life_frac; /* reserved for fade */
        p->x += p->vx * (dt_ms / 1000.0f);
        p->y += p->vy * (dt_ms / 1000.0f);
        p->vy += -0.3f * (dt_ms / 1000.0f); /* slight upward drift */
        ++i;
    }
}

int rogue_hit_particles_active(void) { return g_particle_count; }
const RogueHitParticle* rogue_hit_particles_get(int* out_count)
{
    if (out_count)
        *out_count = g_particle_count;
    return g_particles;
}

static void add_particle(float x, float y, float vx, float vy, float life, unsigned char type)
{
    if (g_particle_count >= ROGUE_HIT_PARTICLE_MAX)
        return;
    RogueHitParticle* p = &g_particles[g_particle_count++];
    p->x = x;
    p->y = y;
    p->vx = vx;
    p->vy = vy;
    p->lifetime_ms = life;
    p->age_ms = 0;
    p->type = type;
}

int rogue_hit_particles_spawn_impact(float x, float y, float nx, float ny, int overkill)
{
    int count = overkill ? 24 : 10 + rand() % 5;
    if (count > 32)
        count = 32;
    float base_speed = overkill ? 5.5f : 3.2f;
    for (int i = 0; i < count; i++)
    {
        float ang_spread =
            ((float) rand() / (float) RAND_MAX - 0.5f) * (float) M_PI * 0.7f; /* +-~63deg */
        float nlen = sqrtf(nx * nx + ny * ny);
        float bx = nx, by = ny;
        if (nlen > 0)
        {
            bx /= nlen;
            by /= nlen;
        }
        float cs = cosf(ang_spread), sn = sinf(ang_spread);
        float rx = bx * cs - by * sn;
        float ry = bx * sn + by * cs;
        float speed = base_speed * (0.55f + 0.45f * ((float) rand() / (float) RAND_MAX));
        add_particle(x, y, rx * speed, ry * speed, overkill ? 480.0f : 340.0f, overkill ? 2 : 1);
    }
    return count;
}

float rogue_hit_calc_knockback_mag(int player_level, int enemy_level, int player_str, int enemy_str)
{
    if (player_level < 0)
        player_level = 0;
    if (enemy_level < 0)
        enemy_level = 0;
    if (player_str < 0)
        player_str = 0;
    if (enemy_str < 0)
        enemy_str = 0;
    float level_diff = (float) player_level - (float) enemy_level;
    if (level_diff < 0)
        level_diff = 0;
    if (level_diff > 20)
        level_diff = 20;
    float str_diff = (float) player_str - (float) enemy_str;
    if (str_diff < 0)
        str_diff = 0;
    if (str_diff > 60)
        str_diff = 60;
    float mag = 0.18f + 0.02f * level_diff + 0.015f * str_diff;
    if (mag > 0.55f)
        mag = 0.55f;
    return mag;
}

void rogue_hit_play_impact_sfx(int weapon_id, int variant)
{
    (void) weapon_id;
    (void) variant; /* stub – future SDL_mixer hook */
}

void rogue_hit_mark_explosion(void) { g_last_explosion_frame = g_app.frame_count; }
int rogue_hit_last_explosion_frame(void) { return g_last_explosion_frame; }
