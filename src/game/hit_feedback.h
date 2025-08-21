/* Hit feedback (Phase 4): impact SFX, particles, overkill explosion, knockback magnitude */
#pragma once
#include "entities/enemy.h"
#include "entities/player.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* Simple particle representation (CPU side only – renderer can sample buffer) */
    typedef struct RogueHitParticle
    {
        float x, y, vx, vy, lifetime_ms, age_ms;
        unsigned char type;
    } RogueHitParticle;

#define ROGUE_HIT_PARTICLE_MAX 128

    void rogue_hit_particles_reset(void);
    void rogue_hit_particles_update(float dt_ms);
    int rogue_hit_particles_active(void);
    const RogueHitParticle* rogue_hit_particles_get(int* out_count);

    /* Spawn standard impact burst (returns count spawned) */
    int rogue_hit_particles_spawn_impact(float x, float y, float nx, float ny, int overkill);

    /* Compute refined knockback magnitude using level & strength differential (clamped). */
    float rogue_hit_calc_knockback_mag(int player_level, int enemy_level, int player_str,
                                       int enemy_str);

    /* Impact SFX stub (wired to SDL_mixer in future) */
    void rogue_hit_play_impact_sfx(int weapon_id, int variant);

    /* Overkill explosion flag setter (tracks last explosion frame for debug overlay) */
    void rogue_hit_mark_explosion(void);
    int rogue_hit_last_explosion_frame(void);

#ifdef __cplusplus
}
#endif
