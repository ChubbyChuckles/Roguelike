#include "game/weapons.h"
#include <string.h>

/* Static weapon table (could be externalized later) */
static const RogueWeaponDef g_weapon_table[] = {
    {0,"Training Sword", 8, 0.65f,0.15f,0.0f, 1.0f,1.0f, 100.0f},
    {1,"Great Hammer", 18,0.85f,0.05f,0.0f, 1.25f,1.35f, 140.0f},
    {2,"Rapier",        6, 0.40f,0.55f,0.0f, 0.85f,0.80f, 90.0f},
    {3,"Focus Catalyst",4, 0.10f,0.15f,0.70f, 1.10f,0.75f, 80.0f},
};
static const int g_weapon_count = (int)(sizeof(g_weapon_table)/sizeof(g_weapon_table[0]));

/* Familiarity simple array */
#define FAM_CAP 16
static RogueWeaponFamiliarity g_fam[FAM_CAP];
static int g_fam_init = 0;

/* Durability runtime values */
static float g_durability[FAM_CAP]; /* index by weapon id while within range */
static int g_durability_init = 0;

const RogueWeaponDef* rogue_weapon_get(int id){
    if(id<0) return 0; for(int i=0;i<g_weapon_count;i++){ if(g_weapon_table[i].id==id) return &g_weapon_table[i]; } return 0; }

static RogueWeaponFamiliarity* fam_slot(int weapon_id){
    if(!g_fam_init){ for(int i=0;i<FAM_CAP;i++){ g_fam[i].weapon_id = -1; g_fam[i].usage_points = 0.0f; } g_fam_init=1; }
    if(weapon_id<0) return 0;
    /* If weapon id fits within cap, use direct slot for determinism */
    if(weapon_id < FAM_CAP){ if(g_fam[weapon_id].weapon_id==-1) g_fam[weapon_id].weapon_id=weapon_id; return &g_fam[weapon_id]; }
    /* Else fallback: search existing, then find first empty */
    for(int i=0;i<FAM_CAP;i++){ if(g_fam[i].weapon_id==weapon_id) return &g_fam[i]; }
    for(int i=0;i<FAM_CAP;i++){ if(g_fam[i].weapon_id==-1){ g_fam[i].weapon_id=weapon_id; return &g_fam[i]; } }
    return 0;
}

float rogue_weapon_get_familiarity_bonus(int weapon_id){
    RogueWeaponFamiliarity* s = fam_slot(weapon_id); if(!s) return 0.0f; float p=s->usage_points; if(p>10000.0f) p=10000.0f; /* soft cap */ float bonus = (p/10000.0f)*0.10f; if(bonus>0.10f) bonus=0.10f; return bonus;
}

void rogue_weapon_register_hit(int weapon_id, float damage_done){
    RogueWeaponFamiliarity* s = fam_slot(weapon_id); if(!s) return; if(damage_done < 0.0f) damage_done = 0.0f; /* no regression */
    /* Award small base progression even for low damage so tests with tiny numbers grow */
    float inc = damage_done * 0.5f + 1.0f;
    s->usage_points += inc; if(s->usage_points>10000.0f) s->usage_points=10000.0f;
}

void rogue_weapon_tick_durability(int weapon_id, float amount){
    if(weapon_id<0 || weapon_id>=FAM_CAP) return; if(!g_durability_init){ for(int i=0;i<FAM_CAP;i++) g_durability[i]=-1.0f; g_durability_init=1; }
    const RogueWeaponDef* wd = rogue_weapon_get(weapon_id); if(!wd) return; if(g_durability[weapon_id] < 0) g_durability[weapon_id] = wd->durability_max;
    g_durability[weapon_id] -= amount; if(g_durability[weapon_id] < 0) g_durability[weapon_id]=0;
}
float rogue_weapon_current_durability(int weapon_id){ if(weapon_id<0 || weapon_id>=FAM_CAP) return 0; if(!g_durability_init) return 0; return g_durability[weapon_id]; }

RogueStanceModifiers rogue_stance_get_mods(int stance){
    RogueStanceModifiers m = {1.0f,1.0f,1.0f};
    switch(stance){
        case 1: /* aggressive */ m.damage_mult=1.15f; m.stamina_mult=1.15f; m.poise_damage_mult=1.10f; break;
        case 2: /* defensive */ m.damage_mult=0.90f; m.stamina_mult=0.85f; m.poise_damage_mult=0.95f; break;
        default: break; /* balanced */
    }
    return m;
}

void rogue_stance_apply_frame_adjustments(int stance, float base_windup_ms, float base_recover_ms, float* out_windup_ms, float* out_recover_ms){
    float w = base_windup_ms; float r = base_recover_ms;
    if(stance==1){ /* aggressive */ w *= 0.95f; r *= 0.97f; }
    else if(stance==2){ /* defensive */ w *= 1.06f; r *= 1.08f; }
    if(out_windup_ms) *out_windup_ms = w; if(out_recover_ms) *out_recover_ms = r;
}
