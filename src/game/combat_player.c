#include "game/combat.h"
#include "core/buffs.h"
#include "game/combat_attacks.h"
#include "game/weapons.h"
#include "game/infusions.h"
#include <math.h>
#include <stdlib.h>

/* Forward declarations from strike module */
int rogue_combat_player_strike(RoguePlayerCombat* pc, RoguePlayer* player, RogueEnemy enemies[], int enemy_count);

int rogue_force_attack_active = 0; /* exported */
int g_attack_frame_override = -1; /* tests set this */
static int g_player_hyper_armor_active = 0; /* transient state */

void rogue_player_set_hyper_armor_active(int active){ g_player_hyper_armor_active = active?1:0; }

void rogue_combat_init(RoguePlayerCombat* pc){
    pc->phase=ROGUE_ATTACK_IDLE; pc->timer=0; pc->combo=0; pc->stamina=100.0f; pc->stamina_regen_delay=0.0f;
    pc->buffered_attack=0; pc->hit_confirmed=0; pc->strike_time_ms=0.0f;
    pc->archetype = ROGUE_WEAPON_LIGHT; pc->chain_index = 0; pc->queued_branch_archetype = ROGUE_WEAPON_LIGHT; pc->queued_branch_pending=0;
    pc->precise_accum_ms = 0.0; pc->blocked_this_strike=0; pc->recovered_recently=0; pc->idle_since_recover_ms=0.0f; pc->processed_window_mask=0; pc->emitted_events_mask=0; pc->event_count=0;
    pc->charging=0; pc->charge_time_ms=0.0f; pc->pending_charge_damage_mult=1.0f;
    pc->parry_active=0; pc->parry_window_ms=160.0f; pc->parry_timer_ms=0.0f; pc->riposte_ready=0; pc->riposte_window_ms=650.0f; pc->backstab_cooldown_ms=0.0f;
    pc->aerial_attack_pending=0; pc->landing_lag_ms=0.0f; pc->guard_break_ready=0;
    pc->backstab_pending_mult=1.0f; pc->riposte_pending_mult=1.0f; pc->guard_break_pending_mult=1.0f; pc->force_crit_next_strike=0;
}

void rogue_combat_test_force_strike(RoguePlayerCombat* pc, float strike_time_ms){ if(!pc) return; pc->phase = ROGUE_ATTACK_STRIKE; pc->strike_time_ms = strike_time_ms; pc->processed_window_mask=0; }

void rogue_combat_update_player(RoguePlayerCombat* pc, float dt_ms, int attack_pressed){
    extern struct RoguePlayer g_exposed_player_for_stats; /* player stats */
    int suppressed = 0;
    if(g_exposed_player_for_stats.cc_stun_ms > 0.0f || g_exposed_player_for_stats.cc_disarm_ms > 0.0f){ suppressed = 1; }
    if(attack_pressed && !suppressed){ pc->buffered_attack=1; }
    const RogueAttackDef* def = rogue_attack_get(pc->archetype, pc->chain_index);
    float WINDUP_MS  = def? def->startup_ms : 110.0f;
    float STRIKE_MS  = def? def->active_ms  : 70.0f;
    float RECOVER_MS = def? def->recovery_ms: 120.0f;
    {
        float adj_w = WINDUP_MS, adj_r = RECOVER_MS;
        rogue_stance_apply_frame_adjustments(g_exposed_player_for_stats.combat_stance, WINDUP_MS, RECOVER_MS, &adj_w, &adj_r);
        WINDUP_MS = adj_w; RECOVER_MS = adj_r;
    }
    pc->precise_accum_ms += (double)dt_ms; pc->timer = (float)pc->precise_accum_ms;
    if(pc->phase==ROGUE_ATTACK_IDLE){
        if(pc->recovered_recently){ pc->idle_since_recover_ms += dt_ms; if(pc->idle_since_recover_ms > 130.0f){ pc->recovered_recently=0; }}
        if(pc->buffered_attack && def && pc->stamina >= def->stamina_cost && !suppressed){
            if(pc->recovered_recently && pc->idle_since_recover_ms < 130.0f){
                if(pc->queued_branch_pending){ pc->archetype = pc->queued_branch_archetype; pc->chain_index = 0; pc->queued_branch_pending=0; }
                else { int len = rogue_attack_chain_length(pc->archetype); pc->chain_index = (pc->chain_index + 1) % (len>0?len:1); }
                def = rogue_attack_get(pc->archetype, pc->chain_index);
                WINDUP_MS = def?def->startup_ms:WINDUP_MS; STRIKE_MS = def?def->active_ms:STRIKE_MS; RECOVER_MS= def?def->recovery_ms:RECOVER_MS;
            }
            if(pc->queued_branch_pending){
                pc->archetype = pc->queued_branch_archetype; pc->chain_index = 0; pc->queued_branch_pending=0; def = rogue_attack_get(pc->archetype, pc->chain_index);
                if(def){ WINDUP_MS=def->startup_ms; STRIKE_MS=def->active_ms; RECOVER_MS=def->recovery_ms; }
            }
            RogueStanceModifiers sm = rogue_stance_get_mods(g_exposed_player_for_stats.combat_stance);
            float cost = def?def->stamina_cost:14.0f; cost *= sm.stamina_mult;
            pc->phase = ROGUE_ATTACK_WINDUP; pc->timer = 0; pc->stamina -= cost; pc->stamina_regen_delay = 500.0f; pc->buffered_attack=0; pc->hit_confirmed=0; pc->strike_time_ms=0; }
    } else if(pc->phase==ROGUE_ATTACK_WINDUP){
        if(pc->timer >= WINDUP_MS){ pc->phase = ROGUE_ATTACK_STRIKE; pc->timer = 0; pc->precise_accum_ms=0.0; pc->strike_time_ms=0; pc->blocked_this_strike=0; pc->processed_window_mask=0; pc->emitted_events_mask=0; pc->event_count=0; }
    } else if(pc->phase==ROGUE_ATTACK_STRIKE){
        pc->strike_time_ms += dt_ms;
        unsigned short active_window_flags = 0; if(def && def->num_windows>0){ for(int wi=0; wi<def->num_windows; ++wi){ const RogueAttackWindow* w = &def->windows[wi]; if(pc->strike_time_ms >= w->start_ms && pc->strike_time_ms < w->end_ms){ active_window_flags = w->flags; break; } } }
        float on_hit_threshold = STRIKE_MS * 0.40f; if(on_hit_threshold < 15.0f) on_hit_threshold = 15.0f;
        int allow_hit_cancel = 0; unsigned short hit_flag_mask = (active_window_flags? active_window_flags : def?def->cancel_flags:0);
        if(pc->hit_confirmed && def && (hit_flag_mask & ROGUE_CANCEL_ON_HIT)){
            int all_windows_done = 0; if(def->num_windows>0){ unsigned int all_bits = (def->num_windows>=32)? 0xFFFFFFFFu : ((1u<<def->num_windows)-1u); all_windows_done = (pc->processed_window_mask & all_bits) == all_bits; } else { all_windows_done = 1; }
            if(pc->strike_time_ms >= on_hit_threshold || g_attack_frame_override >= 0 || all_windows_done){ allow_hit_cancel = 1; }
        }
        int allow_whiff_cancel = 0; if(!pc->hit_confirmed && def){ int has_whiff_flag = (hit_flag_mask & ROGUE_CANCEL_ON_WHIFF) != 0; if(has_whiff_flag){ float needed = def->whiff_cancel_pct * STRIKE_MS; if(pc->strike_time_ms >= needed){ allow_whiff_cancel = 1; } } }
        int allow_block_cancel = 0; if(pc->blocked_this_strike && def && (hit_flag_mask & ROGUE_CANCEL_ON_BLOCK)){ float block_thresh = STRIKE_MS * 0.30f; float whiff_equiv = def->whiff_cancel_pct * STRIKE_MS; if(block_thresh > whiff_equiv) block_thresh = whiff_equiv; if(pc->strike_time_ms >= block_thresh){ allow_block_cancel = 1; } }
        if(pc->strike_time_ms >= STRIKE_MS || allow_hit_cancel || allow_whiff_cancel || allow_block_cancel){ pc->phase = ROGUE_ATTACK_RECOVER; pc->timer = 0; pc->combo++; if(pc->combo>5) pc->combo=5; if(pc->landing_lag_ms>0){ pc->precise_accum_ms = - (double)pc->landing_lag_ms; pc->landing_lag_ms = 0.0f; } }
    } else if(pc->phase==ROGUE_ATTACK_RECOVER){
        float rw=0, rr=0; rogue_stance_apply_frame_adjustments(g_exposed_player_for_stats.combat_stance, def?def->startup_ms:WINDUP_MS, def?def->recovery_ms:RECOVER_MS, &rw, &rr); RECOVER_MS = rr;
        if(pc->timer >= RECOVER_MS){
            if(pc->buffered_attack && def){
                if(pc->queued_branch_pending){ pc->archetype = pc->queued_branch_archetype; pc->chain_index = 0; pc->queued_branch_pending=0; }
                else { int len = rogue_attack_chain_length(pc->archetype); pc->chain_index = (pc->chain_index + 1) % (len>0?len:1); }
                def = rogue_attack_get(pc->archetype, pc->chain_index);
                WINDUP_MS  = def?def->startup_ms:WINDUP_MS; STRIKE_MS  = def?def->active_ms:STRIKE_MS; RECOVER_MS = def?def->recovery_ms:RECOVER_MS;
                RogueStanceModifiers sm2 = rogue_stance_get_mods(g_exposed_player_for_stats.combat_stance);
                float cost = def? def->stamina_cost : 10.0f; cost *= sm2.stamina_mult;
                if(pc->stamina >= cost){ pc->phase = ROGUE_ATTACK_WINDUP; pc->timer = 0; pc->precise_accum_ms=0.0; pc->stamina -= cost; pc->stamina_regen_delay=450.0f; pc->buffered_attack=0; pc->hit_confirmed=0; pc->strike_time_ms=0; pc->blocked_this_strike=0; }
                else { pc->phase = ROGUE_ATTACK_IDLE; pc->timer = 0; pc->hit_confirmed=0; pc->buffered_attack=0; pc->recovered_recently=1; pc->idle_since_recover_ms=0.0f; }
            } else { pc->phase = ROGUE_ATTACK_IDLE; pc->timer = 0; pc->combo = (pc->combo>0)?pc->combo:0; pc->hit_confirmed=0; pc->buffered_attack=0; pc->blocked_this_strike=0; pc->recovered_recently=1; pc->idle_since_recover_ms=0.0f; }
        }
    }
    if(pc->stamina_regen_delay>0){ pc->stamina_regen_delay -= dt_ms; }
    else {
        extern RoguePlayer g_exposed_player_for_stats; float dex = (float)g_exposed_player_for_stats.dexterity; float intel = (float)g_exposed_player_for_stats.intelligence;
        float regen = 0.055f + (dex*0.00085f) + (intel*0.00055f);
        switch(g_exposed_player_for_stats.encumbrance_tier){ case 1: regen *= 0.92f; break; case 2: regen *= 0.80f; break; case 3: regen *= 0.60f; break; default: break; }
        pc->stamina += dt_ms * regen; if(pc->stamina>100.0f) pc->stamina=100.0f; }
    if(pc->parry_active){ pc->parry_timer_ms += dt_ms; if(pc->parry_timer_ms >= pc->parry_window_ms){ pc->parry_active=0; pc->parry_timer_ms=0.0f; } }
    if(pc->riposte_ready){ pc->riposte_window_ms -= dt_ms; if(pc->riposte_window_ms <= 0){ pc->riposte_ready=0; } }
    if(pc->backstab_cooldown_ms>0){ pc->backstab_cooldown_ms -= dt_ms; if(pc->backstab_cooldown_ms<0) pc->backstab_cooldown_ms=0; }
}

void rogue_combat_notify_blocked(RoguePlayerCombat* pc){ if(!pc) return; if(pc->phase==ROGUE_ATTACK_STRIKE){ pc->blocked_this_strike=1; } }

int rogue_combat_consume_events(RoguePlayerCombat* pc, struct RogueCombatEvent* out_events, int max_events){
    if(!pc || !out_events || max_events<=0) return 0; int n = pc->event_count; if(n>max_events) n = max_events;
    for(int i=0;i<n;i++) out_events[i] = pc->events[i]; int remaining = pc->event_count - n;
    if(remaining>0){ for(int i=0;i<remaining && i < (int)(sizeof(pc->events)/sizeof(pc->events[0])); ++i) pc->events[i] = pc->events[n+i]; }
    pc->event_count = remaining>0? remaining:0; return n;
}

void rogue_combat_set_archetype(RoguePlayerCombat* pc, RogueWeaponArchetype arch){ if(!pc) return; pc->archetype = arch; pc->chain_index = 0; }
RogueWeaponArchetype rogue_combat_current_archetype(const RoguePlayerCombat* pc){ return pc? pc->archetype : ROGUE_WEAPON_LIGHT; }
int rogue_combat_current_chain_index(const RoguePlayerCombat* pc){ return pc? pc->chain_index : 0; }
void rogue_combat_queue_branch(RoguePlayerCombat* pc, RogueWeaponArchetype branch_arch){ if(!pc) return; pc->queued_branch_archetype = branch_arch; pc->queued_branch_pending=1; }

/* Guard & Poise */
static void rogue_player_face(RoguePlayer* p, int dir){ if(!p) return; if(dir<0||dir>3) return; p->facing = dir; }
int rogue_player_begin_guard(RoguePlayer* p, int guard_dir){ if(!p) return 0; if(p->guard_meter <= 0.0f){ p->guarding=0; return 0; } p->guarding=1; p->guard_active_time_ms = 0.0f; rogue_player_face(p,guard_dir); return 1; }
int rogue_player_update_guard(RoguePlayer* p, float dt_ms){ if(!p) return 0; int chip=0; if(p->guarding){ p->guard_active_time_ms += dt_ms; p->guard_meter -= dt_ms * ROGUE_GUARD_METER_DRAIN_HOLD_PER_MS; if(p->guard_meter <= 0.0f){ p->guard_meter=0.0f; p->guarding=0; } } else { p->guard_meter += dt_ms * ROGUE_GUARD_METER_RECOVER_PER_MS; if(p->guard_meter > p->guard_meter_max) p->guard_meter = p->guard_meter_max; }
    rogue_player_poise_regen_tick(p, dt_ms); return chip; }
static void rogue_player_facing_dir(const RoguePlayer* p, float* dx,float*dy){ switch(p->facing){ case 0:*dx=0;*dy=1;break; case 1:*dx=-1;*dy=0;break; case 2:*dx=1;*dy=0;break; case 3:*dx=0;*dy=-1;break; default:*dx=0;*dy=1;break; } }
int rogue_player_apply_incoming_melee(RoguePlayer* p, float raw_damage, float attack_dir_x, float attack_dir_y, int poise_damage, int *out_blocked, int *out_perfect){
    if(out_blocked) *out_blocked=0; if(out_perfect) *out_perfect=0; if(!p) return (int)raw_damage; if(p->iframes_ms > 0.0f) return 0;
    if(p){ extern RoguePlayerCombat g_combat_for_parry_temp; }
    if(raw_damage < 0) raw_damage = 0; float fdx,fdy; rogue_player_facing_dir(p,&fdx,&fdy);
    float alen = sqrtf(attack_dir_x*attack_dir_x + attack_dir_y*attack_dir_y); if(alen>0.0001f){ attack_dir_x/=alen; attack_dir_y/=alen; }
    float dot = fdx*attack_dir_x + fdy*attack_dir_y; int blocked = 0; int perfect = 0;
    if(p->guarding && p->guard_meter > 0.0f && dot >= ROGUE_GUARD_CONE_DOT){
        blocked = 1; perfect = (p->guard_active_time_ms <= p->perfect_guard_window_ms)?1:0; float chip = raw_damage * ROGUE_GUARD_CHIP_PCT; if(chip < 1.0f) chip = (raw_damage>0)?1.0f:0.0f;
        if(perfect){ chip = 0.0f; p->guard_meter += ROGUE_PERFECT_GUARD_REFUND; if(p->guard_meter>p->guard_meter_max) p->guard_meter = p->guard_meter_max; p->poise += ROGUE_PERFECT_GUARD_POISE_BONUS; if(p->poise>p->poise_max) p->poise=p->poise_max; }
        else { p->guard_meter -= ROGUE_GUARD_METER_DRAIN_ON_BLOCK; if(p->guard_meter < 0.0f) p->guard_meter = 0.0f; if(poise_damage>0){ float pd = (float)poise_damage * ROGUE_GUARD_BLOCK_POISE_SCALE; p->poise -= pd; if(p->poise < 0.0f) p->poise = 0.0f; p->poise_regen_delay_ms = ROGUE_POISE_REGEN_DELAY_AFTER_HIT; } }
        if(out_blocked) *out_blocked = 1; if(perfect && out_perfect) *out_perfect = 1; return (int)chip; }
    int triggered_reaction = 0; if(poise_damage>0 && !g_player_hyper_armor_active){ float before = p->poise; p->poise -= (float)poise_damage; if(p->poise < 0.0f) p->poise=0.0f; if(before > 0.0f && p->poise <= 0.0f){ rogue_player_apply_reaction(p, 2); triggered_reaction=1; } }
    if(!triggered_reaction){ if(raw_damage >= 80){ rogue_player_apply_reaction(p,3); } else if(raw_damage >= 25){ rogue_player_apply_reaction(p,1); } }
    p->poise_regen_delay_ms = ROGUE_POISE_REGEN_DELAY_AFTER_HIT; return (int)raw_damage; }
void rogue_player_poise_regen_tick(RoguePlayer* p, float dt_ms){ if(!p) return; if(p->poise_regen_delay_ms>0){ p->poise_regen_delay_ms -= dt_ms; if(p->poise_regen_delay_ms<0) p->poise_regen_delay_ms=0; }
    if(p->poise_regen_delay_ms<=0 && p->poise < p->poise_max){ float missing = p->poise_max - p->poise; float ratio = missing / p->poise_max; if(ratio<0) ratio=0; if(ratio>1) ratio=1; float regen = (ROGUE_POISE_REGEN_BASE_PER_MS * dt_ms) * (1.0f + 1.75f * ratio * ratio); p->poise += regen; if(p->poise > p->poise_max) p->poise = p->poise_max; } }
void rogue_player_update_reactions(RoguePlayer* p, float dt_ms){ if(!p) return; if(p->reaction_timer_ms>0){ p->reaction_timer_ms -= dt_ms; if(p->reaction_timer_ms<=0){ p->reaction_timer_ms=0; p->reaction_type=0; p->reaction_total_ms=0; p->reaction_di_accum_x=0; p->reaction_di_accum_y=0; p->reaction_di_max=0; } } if(p->iframes_ms>0){ p->iframes_ms -= dt_ms; if(p->iframes_ms<0) p->iframes_ms=0; } }
static void rogue_player_init_reaction_params(RoguePlayer* p){ switch(p->reaction_type){ case 1: p->reaction_di_max = 0.35f; break; case 2: p->reaction_di_max = 0.55f; break; case 3: p->reaction_di_max = 0.85f; break; case 4: p->reaction_di_max = 1.00f; break; default: p->reaction_di_max = 0.0f; break; } p->reaction_di_accum_x = 0; p->reaction_di_accum_y = 0; p->reaction_canceled_early=0; }
void rogue_player_apply_reaction(RoguePlayer* p, int reaction_type){ if(!p) return; if(reaction_type<=0) return; p->reaction_type = reaction_type; switch(reaction_type){ case 1: p->reaction_timer_ms = 220.0f; break; case 2: p->reaction_timer_ms = 600.0f; break; case 3: p->reaction_timer_ms = 900.0f; break; case 4: p->reaction_timer_ms = 1100.0f; break; default: p->reaction_timer_ms = 300.0f; break; } p->reaction_total_ms = p->reaction_timer_ms; rogue_player_init_reaction_params(p); }
int rogue_player_try_reaction_cancel(RoguePlayer* p){ if(!p) return 0; if(p->reaction_type==0 || p->reaction_timer_ms<=0) return 0; if(p->reaction_canceled_early) return 0; float min_frac=0.0f, max_frac=0.0f; switch(p->reaction_type){ case 1: min_frac=0.40f; max_frac=0.75f; break; case 2: min_frac=0.55f; max_frac=0.85f; break; case 3: min_frac=0.60f; max_frac=0.80f; break; case 4: min_frac=0.65f; max_frac=0.78f; break; default: return 0; } if(p->reaction_total_ms <= 0) return 0; float elapsed = p->reaction_total_ms - p->reaction_timer_ms; float frac = elapsed / p->reaction_total_ms; if(frac >= min_frac && frac <= max_frac){ p->reaction_timer_ms = 0; p->reaction_type=0; p->reaction_canceled_early=1; return 1; } return 0; }
void rogue_player_apply_reaction_di(RoguePlayer* p, float dx, float dy){ if(!p) return; if(p->reaction_type==0 || p->reaction_timer_ms<=0) return; if(p->reaction_di_max<=0) return; float mag = sqrtf(dx*dx + dy*dy); if(mag>1.0f && mag>0){ dx/=mag; dy/=mag; } p->reaction_di_accum_x += dx * 0.08f; p->reaction_di_accum_y += dy * 0.08f; float acc_mag = sqrtf(p->reaction_di_accum_x*p->reaction_di_accum_x + p->reaction_di_accum_y*p->reaction_di_accum_y); if(acc_mag > p->reaction_di_max && acc_mag>0){ float scale = p->reaction_di_max / acc_mag; p->reaction_di_accum_x *= scale; p->reaction_di_accum_y *= scale; } }
void rogue_player_add_iframes(RoguePlayer* p, float ms){ if(!p) return; if(ms<=0) return; if(p->iframes_ms < ms) p->iframes_ms = ms; }

/* Charged attacks */
void rogue_combat_charge_begin(RoguePlayerCombat* pc){ if(!pc) return; if(pc->phase!=ROGUE_ATTACK_IDLE) return; pc->charging=1; pc->charge_time_ms=0.0f; }
void rogue_combat_charge_tick(RoguePlayerCombat* pc, float dt_ms, int still_holding){ if(!pc) return; if(!pc->charging) return; if(!still_holding){ float t = pc->charge_time_ms; float mult = 1.0f + fminf(t/800.0f,1.0f) * 1.5f; if(mult>2.5f) mult=2.5f; pc->pending_charge_damage_mult = mult; pc->charging=0; pc->charge_time_ms=0.0f; return; } pc->charge_time_ms += dt_ms; if(pc->charge_time_ms > 1600.0f) pc->charge_time_ms=1600.0f; }
float rogue_combat_charge_progress(const RoguePlayerCombat* pc){ if(!pc||!pc->charging) return 0.0f; float p = pc->charge_time_ms/800.0f; if(p>1.0f) p=1.0f; if(p<0) p=0; return p; }

/* Dodge */
int rogue_player_dodge_roll(RoguePlayer* p, RoguePlayerCombat* pc, int dir){ if(!p||!pc) return 0; if(p->reaction_type!=0) return 0; if(pc->phase==ROGUE_ATTACK_STRIKE) return 0; const float COST=18.0f; if(pc->stamina < COST) return 0; pc->stamina -= COST; if(pc->stamina<0) pc->stamina=0; pc->stamina_regen_delay = 350.0f; p->iframes_ms = 400.0f; if(p->poise + 10.0f < p->poise_max) p->poise += 10.0f; else p->poise = p->poise_max; if(dir>=0 && dir<=3) p->facing=dir; return 1; }

/* Backstab */
int rogue_combat_try_backstab(RoguePlayer* p, RoguePlayerCombat* pc, RogueEnemy* e){ if(!p||!pc||!e) return 0; if(!e->alive) return 0; if(pc->backstab_cooldown_ms>0) return 0; float ex=e->base.pos.x, ey=e->base.pos.y; float px=p->base.pos.x, py=p->base.pos.y; float dx = px - ex; float dy = py - ey; float dist2 = dx*dx + dy*dy; if(dist2 > 2.25f) return 0; float fdx= (e->facing==1)? -1.0f : (e->facing==2)? 1.0f : 0.0f; float fdy=(e->facing==0)?1.0f: (e->facing==3)? -1.0f:0.0f; float len = sqrtf(dx*dx + dy*dy); if(len<0.0001f) return 0; dx/=len; dy/=len; float dot = dx*fdx + dy*fdy; if(dot > -0.70f) return 0; pc->backstab_cooldown_ms = 650.0f; pc->backstab_pending_mult = 1.75f; return 1; }

/* Parry / Riposte */
void rogue_player_begin_parry(RoguePlayer* p, RoguePlayerCombat* pc){ if(!p||!pc) return; if(pc->parry_active) return; pc->parry_active=1; pc->parry_timer_ms=0.0f; }
int rogue_player_is_parry_active(const RoguePlayerCombat* pc){ return (pc && pc->parry_active)?1:0; }
int rogue_player_register_incoming_attack_parry(RoguePlayer* p, RoguePlayerCombat* pc, float attack_dir_x, float attack_dir_y){ if(!p||!pc) return 0; if(!pc->parry_active) return 0; float fdx=0,fdy=0; switch(p->facing){ case 0:fdy=1;break; case 1:fdx=-1;break; case 2:fdx=1;break; case 3:fdy=-1;break; } float alen = sqrtf(attack_dir_x*attack_dir_x + attack_dir_y*attack_dir_y); if(alen>0.0001f){ attack_dir_x/=alen; attack_dir_y/=alen; } float dot = fdx*attack_dir_x + fdy*attack_dir_y; if(dot < 0.10f) return 0; pc->parry_active=0; pc->riposte_ready=1; pc->riposte_window_ms=650.0f; p->iframes_ms = 350.0f; p->riposte_ms = pc->riposte_window_ms; return 1; }
int rogue_player_try_riposte(RoguePlayer* p, RoguePlayerCombat* pc, RogueEnemy* e){ if(!p||!pc||!e) return 0; if(!pc->riposte_ready) return 0; if(!e->alive) return 0; pc->riposte_ready=0; p->riposte_ms=0.0f; pc->riposte_pending_mult = 2.25f; return 1; }

/* Guard Break */
void rogue_player_set_guard_break(RoguePlayer* p, RoguePlayerCombat* pc){ (void)p; if(!pc) return; pc->guard_break_ready=1; pc->riposte_ready=1; pc->riposte_window_ms=800.0f; pc->guard_break_pending_mult = 1.50f; pc->force_crit_next_strike=1; }
int rogue_player_consume_guard_break_bonus(RoguePlayerCombat* pc){ if(!pc) return 0; if(!pc->guard_break_ready) return 0; pc->guard_break_ready=0; return 1; }
float rogue_combat_peek_backstab_mult(const RoguePlayerCombat* pc){ return pc? pc->backstab_pending_mult : 1.0f; }

/* Aerial */
void rogue_player_set_airborne(RoguePlayer* p, RoguePlayerCombat* pc){ (void)p; if(!pc) return; pc->aerial_attack_pending=1; }
int rogue_player_is_airborne(const RoguePlayer* p){ (void)p; return 0; }
int rogue_player_try_deflect_projectile(RoguePlayer* p, RoguePlayerCombat* pc, float proj_dir_x, float proj_dir_y, float *out_reflect_dir_x, float *out_reflect_dir_y){ if(!p||!pc) return 0; int can_deflect = 0; if(pc->parry_active) can_deflect=1; if(pc->riposte_ready) can_deflect=1; if(!can_deflect) return 0; float fdx=0,fdy=0; switch(p->facing){ case 0:fdy=1;break; case 1:fdx=-1;break; case 2:fdx=1;break; case 3:fdy=-1;break; } if(out_reflect_dir_x) *out_reflect_dir_x = fdx; if(out_reflect_dir_y) *out_reflect_dir_y = fdy; (void)proj_dir_x; (void)proj_dir_y; return 1; }

/* Obstruction test hook (registered by tests) */
static int (*g_obstruction_line_test)(float sx,float sy,float ex,float ey) = NULL;
void rogue_combat_set_obstruction_line_test(int (*fn)(float,float,float,float)){ g_obstruction_line_test = fn; }
int _rogue_combat_call_obstruction_test(float sx,float sy,float ex,float ey){ return g_obstruction_line_test? g_obstruction_line_test(sx,sy,ex,ey) : -1; }
