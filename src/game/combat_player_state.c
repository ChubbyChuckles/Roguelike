#include "game/combat.h"
#include "game/combat_attacks.h"
#include "game/weapons.h"
#include "game/infusions.h"
#include "game/combat_internal.h"
#include <math.h>
#include <stdlib.h>

/* Core player combat state machine, archetype chaining, stamina, charged attacks */
int rogue_force_attack_active = 0; /* exported */
int g_attack_frame_override = -1;  /* tests may set */
static int g_player_hyper_armor_active = 0; /* transient internal */

void rogue_player_set_hyper_armor_active(int active){ g_player_hyper_armor_active = active?1:0; }
int _rogue_player_is_hyper_armor_active(void){ return g_player_hyper_armor_active; }

void rogue_combat_init(RoguePlayerCombat* pc){
    if(!pc) return;
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
    if(!pc) return;
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

void rogue_combat_set_archetype(RoguePlayerCombat* pc, RogueWeaponArchetype arch){ if(!pc) return; pc->archetype = arch; pc->chain_index = 0; }
RogueWeaponArchetype rogue_combat_current_archetype(const RoguePlayerCombat* pc){ return pc? pc->archetype : ROGUE_WEAPON_LIGHT; }
int rogue_combat_current_chain_index(const RoguePlayerCombat* pc){ return pc? pc->chain_index : 0; }
void rogue_combat_queue_branch(RoguePlayerCombat* pc, RogueWeaponArchetype branch_arch){ if(!pc) return; pc->queued_branch_archetype = branch_arch; pc->queued_branch_pending=1; }
void rogue_combat_notify_blocked(RoguePlayerCombat* pc){ if(!pc) return; if(pc->phase==ROGUE_ATTACK_STRIKE){ pc->blocked_this_strike=1; } }

/* Pop (consume) queued combat events into caller-provided buffer; returns number copied. */
int rogue_combat_consume_events(RoguePlayerCombat* pc, struct RogueCombatEvent* out_events, int max_events){
    if(!pc || !out_events || max_events<=0) return 0; int n = pc->event_count; if(n>max_events) n = max_events;
    for(int i=0;i<n;i++) out_events[i] = pc->events[i]; int remaining = pc->event_count - n;
    if(remaining>0){ for(int i=0;i<remaining && i < (int)(sizeof(pc->events)/sizeof(pc->events[0])); ++i) pc->events[i] = pc->events[n+i]; }
    pc->event_count = remaining>0? remaining:0; return n;
}

/* Charged attacks */
void rogue_combat_charge_begin(RoguePlayerCombat* pc){ if(!pc) return; if(pc->phase!=ROGUE_ATTACK_IDLE) return; pc->charging=1; pc->charge_time_ms=0.0f; }
void rogue_combat_charge_tick(RoguePlayerCombat* pc, float dt_ms, int still_holding){ if(!pc) return; if(!pc->charging) return; if(!still_holding){ float t = pc->charge_time_ms; float mult = 1.0f + fminf(t/800.0f,1.0f) * 1.5f; if(mult>2.5f) mult=2.5f; pc->pending_charge_damage_mult = mult; pc->charging=0; pc->charge_time_ms=0.0f; return; } pc->charge_time_ms += dt_ms; if(pc->charge_time_ms > 1600.0f) pc->charge_time_ms=1600.0f; }
float rogue_combat_charge_progress(const RoguePlayerCombat* pc){ if(!pc||!pc->charging) return 0.0f; float p = pc->charge_time_ms/800.0f; if(p>1.0f) p=1.0f; if(p<0) p=0; return p; }
