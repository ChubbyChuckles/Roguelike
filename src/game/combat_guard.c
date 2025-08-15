#include "game/combat.h"
#include "game/combat_internal.h"
#include <math.h>

/* Guard & Poise / Incoming Melee */
static void rogue_player_face(RoguePlayer* p, int dir){ if(!p) return; if(dir<0||dir>3) return; p->facing = dir; }
static void rogue_player_facing_dir(const RoguePlayer* p, float* dx,float*dy){ switch(p->facing){ case 0:*dx=0;*dy=1;break; case 1:*dx=-1;*dy=0;break; case 2:*dx=1;*dy=0;break; case 3:*dx=0;*dy=-1;break; default:*dx=0;*dy=1;break; } }

int rogue_player_begin_guard(RoguePlayer* p, int guard_dir){ if(!p) return 0; if(p->guard_meter <= 0.0f){ p->guarding=0; return 0; } p->guarding=1; p->guard_active_time_ms = 0.0f; rogue_player_face(p,guard_dir); return 1; }
int rogue_player_update_guard(RoguePlayer* p, float dt_ms){ if(!p) return 0; int chip=0; if(p->guarding){ p->guard_active_time_ms += dt_ms; p->guard_meter -= dt_ms * ROGUE_GUARD_METER_DRAIN_HOLD_PER_MS; if(p->guard_meter <= 0.0f){ p->guard_meter=0.0f; p->guarding=0; } } else { p->guard_meter += dt_ms * ROGUE_GUARD_METER_RECOVER_PER_MS; if(p->guard_meter > p->guard_meter_max) p->guard_meter = p->guard_meter_max; }
    rogue_player_poise_regen_tick(p, dt_ms); return chip; }

int rogue_player_apply_incoming_melee(RoguePlayer* p, float raw_damage, float attack_dir_x, float attack_dir_y, int poise_damage, int *out_blocked, int *out_perfect){
    if(out_blocked) *out_blocked=0; if(out_perfect) *out_perfect=0; if(!p) return (int)raw_damage; if(p->iframes_ms > 0.0f) return 0;
    if(raw_damage < 0) raw_damage = 0; float fdx,fdy; rogue_player_facing_dir(p,&fdx,&fdy);
    float alen = sqrtf(attack_dir_x*attack_dir_x + attack_dir_y*attack_dir_y); if(alen>0.0001f){ attack_dir_x/=alen; attack_dir_y/=alen; }
    float dot = fdx*attack_dir_x + fdy*attack_dir_y; int blocked = 0; int perfect = 0;
    if(p->guarding && p->guard_meter > 0.0f && dot >= ROGUE_GUARD_CONE_DOT){
        blocked = 1; perfect = (p->guard_active_time_ms <= p->perfect_guard_window_ms)?1:0; float chip = raw_damage * ROGUE_GUARD_CHIP_PCT; if(chip < 1.0f) chip = (raw_damage>0)?1.0f:0.0f;
        if(perfect){ chip = 0.0f; p->guard_meter += ROGUE_PERFECT_GUARD_REFUND; if(p->guard_meter>p->guard_meter_max) p->guard_meter = p->guard_meter_max; p->poise += ROGUE_PERFECT_GUARD_POISE_BONUS; if(p->poise>p->poise_max) p->poise=p->poise_max; }
        else { p->guard_meter -= ROGUE_GUARD_METER_DRAIN_ON_BLOCK; if(p->guard_meter < 0.0f) p->guard_meter = 0.0f; if(poise_damage>0){ float pd = (float)poise_damage * ROGUE_GUARD_BLOCK_POISE_SCALE; p->poise -= pd; if(p->poise < 0.0f) p->poise = 0.0f; p->poise_regen_delay_ms = ROGUE_POISE_REGEN_DELAY_AFTER_HIT; } }
        if(out_blocked) *out_blocked = 1; if(perfect && out_perfect) *out_perfect = 1; return (int)chip; }
    int triggered_reaction = 0; if(poise_damage>0 && !_rogue_player_is_hyper_armor_active()){ float before = p->poise; p->poise -= (float)poise_damage; if(p->poise < 0.0f) p->poise=0.0f; if(before > 0.0f && p->poise <= 0.0f){ rogue_player_apply_reaction(p, 2); triggered_reaction=1; } }
    if(!triggered_reaction){ if(raw_damage >= 80){ rogue_player_apply_reaction(p,3); } else if(raw_damage >= 25){ rogue_player_apply_reaction(p,1); } }
    p->poise_regen_delay_ms = ROGUE_POISE_REGEN_DELAY_AFTER_HIT; return (int)raw_damage; }

void rogue_player_poise_regen_tick(RoguePlayer* p, float dt_ms){ if(!p) return; if(p->poise_regen_delay_ms>0){ p->poise_regen_delay_ms -= dt_ms; if(p->poise_regen_delay_ms<0) p->poise_regen_delay_ms=0; }
    if(p->poise_regen_delay_ms<=0 && p->poise < p->poise_max){ float missing = p->poise_max - p->poise; float ratio = missing / p->poise_max; if(ratio<0) ratio=0; if(ratio>1) ratio=1; float regen = (ROGUE_POISE_REGEN_BASE_PER_MS * dt_ms) * (1.0f + 1.75f * ratio * ratio); p->poise += regen; if(p->poise > p->poise_max) p->poise = p->poise_max; } }
