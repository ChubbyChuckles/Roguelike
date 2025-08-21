/* hud_bars.c - UI Phase 6.2 layered HUD bars implementation */
#include "hud_bars.h"

static float clamp01(float v){ if(v<0) return 0.0f; if(v>1) return 1.0f; return v; }

void rogue_hud_bars_reset(RogueHUDBarsState* st){
    if(!st) return; st->health_primary=st->health_secondary=1.0f; st->mana_primary=st->mana_secondary=1.0f; st->ap_primary=st->ap_secondary=1.0f; st->initialized=1; }

void rogue_hud_bars_update(RogueHUDBarsState* st,
    int hp, int hp_max,
    int mp, int mp_max,
    int ap, int ap_max,
    int dt_ms){
    if(!st) return; if(!st->initialized){ rogue_hud_bars_reset(st); }
    float hp_p = (hp_max>0)? (float)hp/(float)hp_max:0.0f;
    float mp_p = (mp_max>0)? (float)mp/(float)mp_max:0.0f;
    float ap_p = (ap_max>0)? (float)ap/(float)ap_max:0.0f;
    if(hp_p<0) hp_p=0; if(hp_p>1) hp_p=1; if(mp_p<0) mp_p=0; if(mp_p>1) mp_p=1; if(ap_p<0) ap_p=0; if(ap_p>1) ap_p=1;
    /* Snap secondary upward instantly (heals); lag downward at fixed rate fraction/sec. */
    const float catchup_rate = 1.5f; /* fraction units per second */
    float dt = (dt_ms<=0)? 0.0f : (dt_ms/1000.0f);
    /* Health */
    if(hp_p >= st->health_secondary) st->health_secondary = hp_p; else {
        float drop = catchup_rate * dt; if(st->health_secondary - drop < hp_p) st->health_secondary = hp_p; else st->health_secondary -= drop;
    }
    st->health_primary = hp_p;
    /* Mana */
    if(mp_p >= st->mana_secondary) st->mana_secondary = mp_p; else {
        float drop = catchup_rate * dt; if(st->mana_secondary - drop < mp_p) st->mana_secondary = mp_p; else st->mana_secondary -= drop;
    }
    st->mana_primary = mp_p;
    /* AP */
    if(ap_p >= st->ap_secondary) st->ap_secondary = ap_p; else {
        float drop = catchup_rate * dt; if(st->ap_secondary - drop < ap_p) st->ap_secondary = ap_p; else st->ap_secondary -= drop;
    }
    st->ap_primary = ap_p;
}

float rogue_hud_health_primary(const RogueHUDBarsState* st){ return clamp01(st?st->health_primary:0.0f); }
float rogue_hud_health_secondary(const RogueHUDBarsState* st){ return clamp01(st?st->health_secondary:0.0f); }
float rogue_hud_mana_primary(const RogueHUDBarsState* st){ return clamp01(st?st->mana_primary:0.0f); }
float rogue_hud_mana_secondary(const RogueHUDBarsState* st){ return clamp01(st?st->mana_secondary:0.0f); }
float rogue_hud_ap_primary(const RogueHUDBarsState* st){ return clamp01(st?st->ap_primary:0.0f); }
float rogue_hud_ap_secondary(const RogueHUDBarsState* st){ return clamp01(st?st->ap_secondary:0.0f); }
