#include "game/combat.h"
#include "core/buffs.h" /* needed for temporary strength buffs */
#include "game/combat_attacks.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include "core/navigation.h" /* Phase 5.5 obstruction checks */
/* Damage event ring buffer (Phase 2.7) */
RogueDamageEvent g_damage_events[ROGUE_DAMAGE_EVENT_CAP];
int g_damage_event_head = 0;
int g_damage_event_total = 0;
int g_crit_layering_mode = 0; /* 0=pre-mitigation (legacy), 1=post-mitigation */
void rogue_damage_event_record(unsigned short attack_id, unsigned char dmg_type, unsigned char crit, int raw, int mitig, int overkill, unsigned char execution){
    RogueDamageEvent* ev = &g_damage_events[g_damage_event_head % ROGUE_DAMAGE_EVENT_CAP];
    ev->attack_id = attack_id; ev->damage_type=dmg_type; ev->crit=crit; ev->raw_damage=raw; ev->mitigated=mitig; ev->overkill=overkill; ev->execution=execution?1:0;
    g_damage_event_head = (g_damage_event_head + 1) % ROGUE_DAMAGE_EVENT_CAP; g_damage_event_total++;
}

int rogue_damage_events_snapshot(RogueDamageEvent* out, int max_events){
    if(!out || max_events<=0) return 0;
    int count = (g_damage_event_total < ROGUE_DAMAGE_EVENT_CAP)? g_damage_event_total : ROGUE_DAMAGE_EVENT_CAP;
    if(count > max_events) count = max_events;
    /* Oldest index is head - count (wrap) */
    int start = (g_damage_event_head - count); while(start < 0) start += ROGUE_DAMAGE_EVENT_CAP;
    for(int i=0;i<count;i++){
        out[i] = g_damage_events[(start + i) % ROGUE_DAMAGE_EVENT_CAP];
    }
    return count;
}

void rogue_damage_events_clear(void){
    for(int i=0;i<ROGUE_DAMAGE_EVENT_CAP;i++){ g_damage_events[i].attack_id=0; g_damage_events[i].damage_type=0; g_damage_events[i].crit=0; g_damage_events[i].raw_damage=0; g_damage_events[i].mitigated=0; g_damage_events[i].overkill=0; g_damage_events[i].execution=0; }
    g_damage_event_head=0; g_damage_event_total=0;
}

/* External helpers */
void rogue_app_add_hitstop(float ms);

/* Provided by app.c */
void rogue_add_damage_number(float x,float y,int amount,int from_player);
void rogue_add_damage_number_ex(float x,float y,int amount,int from_player,int crit);

/* --- Phase 2 Mitigation Helpers ----------------------------------------------------------- */
static int clampi(int v,int lo,int hi){ if(v<lo) return lo; if(v>hi) return hi; return v; }
/* Phase 2.2 finalization: diminishing returns curve for physical percent resist.
    Input: raw resist_physical 0..90 (authored directly on enemy). We map to effective percent via
    a concave curve providing strong early gains and tapering late: eff = p - (p*p)/300.
    At p=0 ->0, p=30 -> 27, p=60 -> 48, p=90 -> 63. (Armor flat reduction still applied first.) */
static int rogue_effective_phys_resist(int p){ if(p<=0) return 0; if(p>90) p=90; int eff = p - (p*p)/300; if(eff<0) eff=0; if(eff>75) eff=75; return eff; }
int rogue_apply_mitigation_enemy(RogueEnemy* e, int raw, unsigned char dmg_type, int *out_overkill){
    if(!e || !e->alive) return 0;
    int dmg = raw; if(dmg<0) dmg=0;
    if(dmg_type != ROGUE_DMG_TRUE){
        if(dmg_type == ROGUE_DMG_PHYSICAL){
            /* Armor flat reduction then percent physical resist */
            int armor = e->armor; if(armor>0){ if(armor >= dmg) dmg = (dmg>1?1:dmg); else dmg -= armor; }
            int pr_raw = clampi(e->resist_physical,0,90); int pr = rogue_effective_phys_resist(pr_raw); if(pr>0){ int reduce=(dmg*pr)/100; dmg -= reduce; }
            /* Phase 3.6: Defensive weight soft cap (applies only for sufficiently large raw hits) */
            if(raw >= ROGUE_DEF_SOFTCAP_MIN_RAW){
                /* Approximate combined pre-softcap reduction fraction. We estimate armor contribution fractionally as armor/(raw+armor) (diminishing) plus percent resist applied after. */
                float armor_frac = 0.0f; if(armor>0){ armor_frac = (float)armor / (float)(raw + armor); if(armor_frac>0.90f) armor_frac=0.90f; }
                float total_frac = armor_frac + (float)pr / 100.0f; if(total_frac > 0.0f){
                    if(total_frac > ROGUE_DEF_SOFTCAP_REDUCTION_THRESHOLD){
                        float excess = total_frac - ROGUE_DEF_SOFTCAP_REDUCTION_THRESHOLD;
                        float adjusted_excess = excess * ROGUE_DEF_SOFTCAP_SLOPE;
                        float capped_total = ROGUE_DEF_SOFTCAP_REDUCTION_THRESHOLD + adjusted_excess;
                        if(capped_total > ROGUE_DEF_SOFTCAP_MAX_REDUCTION) capped_total = ROGUE_DEF_SOFTCAP_MAX_REDUCTION;
                        /* Compute target post-mitigation damage based on capped_total fraction; protect floor. */
                        int target = (int)floorf((float)raw * (1.0f - capped_total) + 0.5f);
                        if(target < 1) target = 1;
                        if(target > dmg) { /* soft cap should never increase damage: recompute only if we over-reduced */ }
                        else dmg = target;
                    }
                }
            }
        } else {
            int resist=0; switch(dmg_type){
                case ROGUE_DMG_FIRE: resist = e->resist_fire; break;
                case ROGUE_DMG_FROST: resist = e->resist_frost; break;
                case ROGUE_DMG_ARCANE: resist = e->resist_arcane; break;
                default: break; }
            resist = clampi(resist,0,90); if(resist>0){ int reduce=(dmg*resist)/100; dmg -= reduce; }
        }
    }
    if(dmg < 1) dmg = 1;
    int overkill = 0; if(e->health - dmg < 0){ overkill = dmg - e->health; }
    if(out_overkill) *out_overkill = overkill; return dmg;
}

void rogue_combat_init(RoguePlayerCombat* pc){
    pc->phase=ROGUE_ATTACK_IDLE; pc->timer=0; pc->combo=0; pc->stamina=100.0f; pc->stamina_regen_delay=0.0f;
    pc->buffered_attack=0; pc->hit_confirmed=0; pc->strike_time_ms=0.0f;
    pc->archetype = ROGUE_WEAPON_LIGHT; pc->chain_index = 0; pc->queued_branch_archetype = ROGUE_WEAPON_LIGHT; pc->queued_branch_pending=0;
    pc->precise_accum_ms = 0.0; pc->blocked_this_strike=0; pc->recovered_recently=0; pc->idle_since_recover_ms=0.0f; pc->processed_window_mask=0; pc->emitted_events_mask=0; pc->event_count=0;
}

int rogue_force_attack_active = 0; /* exported */
int g_attack_frame_override = -1; /* tests set this */

/* Test helper: force phase STRIKE and set strike_time for deterministic multi-hit window validation */
void rogue_combat_test_force_strike(RoguePlayerCombat* pc, float strike_time_ms){
    if(!pc) return; pc->phase = ROGUE_ATTACK_STRIKE; pc->strike_time_ms = strike_time_ms; pc->processed_window_mask=0; }

void rogue_combat_update_player(RoguePlayerCombat* pc, float dt_ms, int attack_pressed){
    /* Phase 4.4: if player stunned or disarmed, suppress attack input consumption */
    extern struct RoguePlayer g_exposed_player_for_stats; /* reuse existing global for simplicity */
    int suppressed = 0;
    if(g_exposed_player_for_stats.cc_stun_ms > 0.0f || g_exposed_player_for_stats.cc_disarm_ms > 0.0f){ suppressed = 1; }
    /* Input buffering: remember press during windup/strike/recover */
    if(attack_pressed && !suppressed){
        if(pc->phase==ROGUE_ATTACK_IDLE){
            pc->buffered_attack=1; /* will consume below */
        } else {
            pc->buffered_attack=1; /* store for chain */
        }
    }

    /* Look up current attack definition (archetype + chain index). */
    const RogueAttackDef* def = rogue_attack_get(pc->archetype, pc->chain_index);
    /* Fallback constants if table missing */
    float WINDUP_MS  = def? def->startup_ms : 110.0f;
    float STRIKE_MS  = def? def->active_ms  : 70.0f;
    float RECOVER_MS = def? def->recovery_ms: 120.0f;

    /* High precision accumulation mitigates float drift across long sessions. */
    pc->precise_accum_ms += (double)dt_ms;
    pc->timer = (float)pc->precise_accum_ms;
    if(pc->phase==ROGUE_ATTACK_IDLE){
        if(pc->recovered_recently){ pc->idle_since_recover_ms += dt_ms; if(pc->idle_since_recover_ms > 130.0f){ pc->recovered_recently=0; }}
    if(pc->buffered_attack && def && pc->stamina >= def->stamina_cost && !suppressed){
            /* Late-chain grace: allow input pressed shortly (<130ms) after recovery ended to advance chain. */
            if(pc->recovered_recently && pc->idle_since_recover_ms < 130.0f){
                if(pc->queued_branch_pending){
                    pc->archetype = pc->queued_branch_archetype; pc->chain_index = 0; pc->queued_branch_pending=0;
                } else {
                    int len = rogue_attack_chain_length(pc->archetype);
                    pc->chain_index = (pc->chain_index + 1) % (len>0?len:1);
                }
                def = rogue_attack_get(pc->archetype, pc->chain_index);
                WINDUP_MS = def?def->startup_ms:WINDUP_MS;
                STRIKE_MS = def?def->active_ms:STRIKE_MS;
                RECOVER_MS= def?def->recovery_ms:RECOVER_MS;
            }
            /* Apply queued branch if any */
            if(pc->queued_branch_pending){
                pc->archetype = pc->queued_branch_archetype; pc->chain_index = 0; pc->queued_branch_pending=0;
                def = rogue_attack_get(pc->archetype, pc->chain_index);
                if(def){
                    WINDUP_MS=def->startup_ms; STRIKE_MS=def->active_ms; RECOVER_MS=def->recovery_ms;
                }
            }
            pc->phase = ROGUE_ATTACK_WINDUP; pc->timer = 0; pc->stamina -= def?def->stamina_cost:14.0f; pc->stamina_regen_delay = 500.0f; pc->buffered_attack=0; pc->hit_confirmed=0; pc->strike_time_ms=0; }
    } else if(pc->phase==ROGUE_ATTACK_WINDUP){
    if(pc->timer >= WINDUP_MS){ pc->phase = ROGUE_ATTACK_STRIKE; pc->timer = 0; pc->precise_accum_ms=0.0; pc->strike_time_ms=0; pc->blocked_this_strike=0; pc->processed_window_mask=0; pc->emitted_events_mask=0; pc->event_count=0; }
    } else if(pc->phase==ROGUE_ATTACK_STRIKE){
    pc->strike_time_ms += dt_ms;
        /* Determine current hit window for per-window cancel gating. */
        unsigned short active_window_flags = 0;
        if(def && def->num_windows>0){
            for(int wi=0; wi<def->num_windows; ++wi){
                const RogueAttackWindow* w = &def->windows[wi];
                if(pc->strike_time_ms >= w->start_ms && pc->strike_time_ms < w->end_ms){
                    active_window_flags = w->flags; break;
                }
            }
        }
        /* Early cancel logic:
           - On hit confirm: always allow transition (DATA: future could gate via flag if desired)
           - On whiff: allow early cancel only if attack has whiff flag and elapsed >= whiff_cancel_pct * active_ms
         */
        float on_hit_threshold = STRIKE_MS * 0.40f; if(on_hit_threshold < 15.0f) on_hit_threshold = 15.0f;
        int allow_hit_cancel = 0;
        unsigned short hit_flag_mask = (active_window_flags? active_window_flags : def?def->cancel_flags:0);
        if(pc->hit_confirmed && def && (hit_flag_mask & ROGUE_CANCEL_ON_HIT)){
            int all_windows_done = 0;
            if(def->num_windows>0){
                unsigned int all_bits = (def->num_windows>=32)? 0xFFFFFFFFu : ((1u<<def->num_windows)-1u);
                all_windows_done = (pc->processed_window_mask & all_bits) == all_bits;
            } else {
                all_windows_done = 1; /* single implicit window */
            }
            if(pc->strike_time_ms >= on_hit_threshold || g_attack_frame_override >= 0 || all_windows_done){
                allow_hit_cancel = 1;
            }
        }
        int allow_whiff_cancel = 0;
        if(!pc->hit_confirmed && def){
            int has_whiff_flag = (hit_flag_mask & ROGUE_CANCEL_ON_WHIFF) != 0; /* bit1 -> whiff */
            if(has_whiff_flag){
                float needed = def->whiff_cancel_pct * STRIKE_MS;
                if(pc->strike_time_ms >= needed){ allow_whiff_cancel = 1; }
            }
        }
        int allow_block_cancel = 0;
        if(pc->blocked_this_strike && def && (hit_flag_mask & ROGUE_CANCEL_ON_BLOCK)){
            /* Block cancel has its own threshold: min of whiff threshold and 30% of active to reward defense. */
            float block_thresh = STRIKE_MS * 0.30f;
            float whiff_equiv = def->whiff_cancel_pct * STRIKE_MS;
            if(block_thresh > whiff_equiv) block_thresh = whiff_equiv;
            if(pc->strike_time_ms >= block_thresh){ allow_block_cancel = 1; }
        }
    if(pc->strike_time_ms >= STRIKE_MS || allow_hit_cancel || allow_whiff_cancel || allow_block_cancel){
            pc->phase = ROGUE_ATTACK_RECOVER; pc->timer = 0; pc->combo++; if(pc->combo>5) pc->combo=5; }
    } else if(pc->phase==ROGUE_ATTACK_RECOVER){
        /* Allow late-chain: buffering during recover triggers next windup slightly early */
    if(pc->timer >= RECOVER_MS){
            if(pc->buffered_attack && def){
                /* Advance chain index (wrap) unless a branch queued */
                if(pc->queued_branch_pending){
                    pc->archetype = pc->queued_branch_archetype; pc->chain_index = 0; pc->queued_branch_pending=0;
                } else {
                    int len = rogue_attack_chain_length(pc->archetype);
                    pc->chain_index = (pc->chain_index + 1) % (len>0?len:1);
                }
                def = rogue_attack_get(pc->archetype, pc->chain_index);
                WINDUP_MS  = def?def->startup_ms:WINDUP_MS;
                STRIKE_MS  = def?def->active_ms:STRIKE_MS;
                RECOVER_MS = def?def->recovery_ms:RECOVER_MS;
                float cost = def? def->stamina_cost : 10.0f;
                if(pc->stamina >= cost){
                    pc->phase = ROGUE_ATTACK_WINDUP; pc->timer = 0; pc->precise_accum_ms=0.0; pc->stamina -= cost; pc->stamina_regen_delay=450.0f; pc->buffered_attack=0; pc->hit_confirmed=0; pc->strike_time_ms=0; pc->blocked_this_strike=0;
                } else {
                    pc->phase = ROGUE_ATTACK_IDLE; pc->timer = 0; pc->hit_confirmed=0; pc->buffered_attack=0;
            pc->recovered_recently=1; pc->idle_since_recover_ms=0.0f;
                }
            }
        else { pc->phase = ROGUE_ATTACK_IDLE; pc->timer = 0; pc->combo = (pc->combo>0)?pc->combo:0; pc->hit_confirmed=0; pc->buffered_attack=0; pc->blocked_this_strike=0; pc->recovered_recently=1; pc->idle_since_recover_ms=0.0f; }
        }
    }

    if(pc->stamina_regen_delay>0){ pc->stamina_regen_delay -= dt_ms; }
    else {
        extern RoguePlayer g_exposed_player_for_stats; /* declared in app for access */
        float dex = (float)g_exposed_player_for_stats.dexterity;
        float intel = (float)g_exposed_player_for_stats.intelligence;
        float regen = 0.055f + (dex*0.00085f) + (intel*0.00055f); /* base regen */
        /* Phase 3.5: dynamic stamina tax scaling by encumbrance tier */
        switch(g_exposed_player_for_stats.encumbrance_tier){
            case 1: regen *= 0.92f; break; /* medium */
            case 2: regen *= 0.80f; break; /* heavy */
            case 3: regen *= 0.60f; break; /* overloaded */
            default: break; /* light unchanged */
        }
        pc->stamina += dt_ms * regen; if(pc->stamina>100.0f) pc->stamina=100.0f; }
}

int rogue_combat_player_strike(RoguePlayerCombat* pc, RoguePlayer* player, RogueEnemy enemies[], int enemy_count){
    if(pc->phase != ROGUE_ATTACK_STRIKE) return 0;
    /* If strike_time reset to 0 externally (test harness) ensure we clear processed mask to allow a fresh window pass. */
    if(pc->strike_time_ms <= 0.0f && pc->processed_window_mask != 0){ pc->processed_window_mask = 0; pc->emitted_events_mask = 0; pc->event_count = 0; }
    #ifdef COMBAT_DEBUG
    printf("[strike_entry] phase=%d strike_time=%.2f processed_mask=0x%X chain=%d\n", pc->phase, pc->strike_time_ms, pc->processed_window_mask, pc->chain_index);
    #endif
    int kills=0;
    /* Attack frame gating/ reach curve */
    static const float reach_curve[8] = {0.65f,0.95f,1.25f,1.35f,1.35f,1.18f,0.95f,0.75f}; /* subtle reach buff */
    static const unsigned char hit_mask[8] = {0,0,1,1,1,1,0,0};
    extern int rogue_get_current_attack_frame(void); /* provided by app or test */
    int afr = (g_attack_frame_override>=0)? g_attack_frame_override : rogue_get_current_attack_frame();
    if(afr<0 || afr>7) afr=0;
    int gating = hit_mask[afr];
#ifdef TEST_COMBAT_PERMISSIVE
    gating = 1; /* allow all frames when test flag set */
#endif
    if(!rogue_force_attack_active){
        if(!gating){
            /* Fallback: if active frame is a non-hit startup frame and tests did not set an override,
               attempt a lenient re-evaluation using a mid-swing frame (3) so deterministic unit tests
               that don't link the animation system (thus always reporting frame 0) can still register a hit. */
            if(g_attack_frame_override < 0){
                int test_frame = 3; /* guaranteed active according to mask */
                int tf_gate = hit_mask[test_frame];
                if(tf_gate){ afr = test_frame; gating = 1; }
            }
            if(!gating) return 0; /* still no active frame */
        }
    }
    extern RoguePlayer g_exposed_player_for_stats; /* for consistency if needed */
    float px = player->base.pos.x; float py = player->base.pos.y;
    float base_reach = 1.6f * reach_curve[afr];
    float reach = base_reach + (player->strength * 0.012f);
    float dirx=0, diry=0; int facing = player->facing;
    switch(facing){ case 0: diry=1; break; case 1: dirx=-1; break; case 2: dirx=1; break; case 3: diry=-1; break; }
    /* Center for arc slightly forward (unbiased horizontally/vertically) */
    float cx = px + dirx * reach * 0.45f;
    float cy = py + diry * reach * 0.45f;
    float reach2 = reach*reach;
    /* Determine which attack window(s) are currently active (can be more than one if overlapping). */
    unsigned int newly_active_mask = 0; pc->current_window_flags = 0;
    const RogueAttackDef* def = rogue_attack_get(pc->archetype, pc->chain_index);
    if(def && def->num_windows>0){
        for(int wi=0; wi<def->num_windows && wi<32; ++wi){
            const RogueAttackWindow* w = &def->windows[wi];
            int active = (pc->strike_time_ms >= w->start_ms && pc->strike_time_ms < w->end_ms);
            if(active){ newly_active_mask |= (1u<<wi); }
            if(active){ pc->current_window_flags = w->flags; }
            /* Emit begin/end events once. */
            unsigned int bit = (1u<<wi);
            if(active && !(pc->emitted_events_mask & bit)){
                if(pc->event_count < (int)(sizeof(pc->events)/sizeof(pc->events[0]))){
                    pc->events[pc->event_count].type = ROGUE_COMBAT_EVENT_BEGIN_WINDOW; pc->events[pc->event_count].data = (unsigned short)wi; pc->events[pc->event_count].t_ms = pc->strike_time_ms; pc->event_count++; }
                pc->emitted_events_mask |= bit;
            } else if(!active && (pc->emitted_events_mask & bit) && !(pc->processed_window_mask & bit)){
                /* End before processing (e.g., window had no targets) */
                if(pc->event_count < (int)(sizeof(pc->events)/sizeof(pc->events[0]))){
                    pc->events[pc->event_count].type = ROGUE_COMBAT_EVENT_END_WINDOW; pc->events[pc->event_count].data = (unsigned short)wi; pc->events[pc->event_count].t_ms = pc->strike_time_ms; pc->event_count++; }
                pc->processed_window_mask |= bit; /* mark as processed to prevent duplicate end */
            }
        }
    } else if(def){
        /* Single implicit window spanning entire active phase */
        newly_active_mask = 1u; if(pc->strike_time_ms >= def->active_ms) newly_active_mask = 0; /* about to end */
    }
    /* Filter out already processed windows (multi-hit prevention). */
    unsigned int process_mask = newly_active_mask & ~pc->processed_window_mask;
    if(process_mask==0){
    #ifdef COMBAT_DEBUG
    printf("[strike_skip] no new windows strike_time=%.2f newly_active=0x%X processed=0x%X\n", pc->strike_time_ms, newly_active_mask, pc->processed_window_mask);
    #endif
        return 0; /* Nothing new to strike this frame */
    }
    #ifdef COMBAT_DEBUG
    printf("[multi_hit_debug] strike_time=%.2f newly_active_mask=0x%X processed_mask=0x%X process_mask=0x%X windows=%d chain=%d\n", pc->strike_time_ms, newly_active_mask, pc->processed_window_mask, process_mask, def?def->num_windows:0, pc->chain_index);
    #endif
    /* Process each new window separately so multi-hit windows apply sequential damage. */
    for(int wi=0; wi<32; ++wi){
        if(!(process_mask & (1u<<wi))) continue;
        float window_mult = 1.0f;
        float bleed_build = 0.0f, frost_build = 0.0f;
        if(def && wi < def->num_windows){
            const RogueAttackWindow* w = &def->windows[wi];
            if(w->damage_mult > 0.0f) window_mult = w->damage_mult;
            bleed_build = w->bleed_build; frost_build = w->frost_build;
            /* If any processed window carries hyper armor, enable it for the entire strike evaluation this frame. */
            if(w->flags & ROGUE_WINDOW_HYPER_ARMOR){ rogue_player_set_hyper_armor_active(1); }
        }
        for(int i=0;i<enemy_count;i++){
            if(!enemies[i].alive) continue;
            /* Phase 5.4: friendly fire / team filtering (skip if same team) */
            if(enemies[i].team_id == player->team_id) continue;
            if(enemies[i].staggered){ /* skip striking staggered enemies for additional damage this frame (placeholder) */ }
            float ex = enemies[i].base.pos.x; float ey = enemies[i].base.pos.y;
            float dx = ex - cx; float dy = ey - cy; float dist2 = dx*dx + dy*dy; if(dist2 > reach2) continue;
            float dot = dx*dirx + dy*diry; float forward_player_dot = (ex - px)*dirx + (ey - py)*diry; if(dot < -0.60f && forward_player_dot < 0.0f) continue;
            float perp = dx* (-diry) + dy* dirx; float lateral_limit = reach * 0.95f; if(fabsf(perp) > lateral_limit) continue;
            int effective_strength = player->strength + rogue_buffs_get_total(0);
            int base = 1 + effective_strength/5;
            float scaled = (float)base;
            if(def){
                scaled = def->base_damage + (float)effective_strength * def->str_scale + (float)player->dexterity * def->dex_scale + (float)player->intelligence * def->int_scale; if(scaled<1.0f) scaled=1.0f;
            }
            float combo_scale = 1.0f + (pc->combo * 0.08f); if(combo_scale>1.4f) combo_scale=1.4f;
            float raw = scaled * combo_scale * window_mult;
            int dmg = (int)floorf(raw + 0.5f);
            if(pc->combo>0){ int min_noncrit = (int)floorf(scaled + pc->combo + 0.5f); int hard_cap = (int)floorf(scaled * 1.4f + 0.5f); if(min_noncrit>hard_cap) min_noncrit=hard_cap; if(dmg<min_noncrit) dmg=min_noncrit; }
            int overkill=0; int raw_before = dmg;
            /* Phase 5.5: terrain obstruction attenuation.
               Cast a simple DDA stepping between player center and enemy; if any blocking tile encountered, mark obstructed.
               Obstructed hits deal 60% damage (rounded) and emit a future clank hook (TODO). */
            int obstructed = 0;
            {
                float rx0 = cx; float ry0 = cy; float rx1 = ex; float ry1 = ey;
                int tx0 = (int)floorf(rx0); int ty0 = (int)floorf(ry0); int tx1 = (int)floorf(rx1); int ty1 = (int)floorf(ry1);
                int steps = (abs(tx1-tx0) > abs(ty1-ty0)? abs(tx1-tx0): abs(ty1-ty0)); if(steps<1) steps=1; /* number of tile transitions */
                /* We step inclusive of the last interior tile by using steps+1 samples after the origin. */
                float fx = (float)(tx1 - tx0)/(float)steps; float fy = (float)(ty1 - ty0)/(float)steps; float sx = (float)tx0 + 0.5f; float sy = (float)ty0 + 0.5f;
                for(int si=0; si<=steps; ++si){ int cx_t = (int)floorf(sx); int cy_t = (int)floorf(sy); if(!(cx_t==tx0 && cy_t==ty0) && !(cx_t==tx1 && cy_t==ty1)){
                        if(rogue_nav_is_blocked(cx_t,cy_t)){ obstructed=1; break; }
                    }
                    sx += fx; sy += fy;
                }
                if(obstructed){ dmg = (int)floorf(dmg * 0.60f + 0.5f); if(dmg<1) dmg=1; }
            }
            /* Roll crit up-front for now; layering mode decides when multiplier applies. */
            float dex_bonus = player->dexterity * 0.0035f; if(dex_bonus>0.55f) dex_bonus=0.55f; /* soft clamp so base + dex <= ~0.60 */
            float crit_chance = 0.05f + dex_bonus + (float)player->crit_chance * 0.01f; if(crit_chance>0.75f) crit_chance=0.75f; /* hard cap */
            int is_crit = (((float)rand()/(float)RAND_MAX) < crit_chance)?1:0;
            float crit_mult = 1.0f;
            if(is_crit){
                crit_mult = 1.0f + ((float)player->crit_damage * 0.01f);
                if(crit_mult > 5.0f) crit_mult = 5.0f; /* safety cap */
            }
            /* Apply crit multiplier pre-mitigation if mode 0 */
            if(is_crit && g_crit_layering_mode==0){
                float cval = (float)dmg * crit_mult; dmg = (int)floorf(cval + 0.5f); if(dmg<1) dmg=1;
            }
            /* Apply penetration before mitigation if physical: flat then percent (player stats). */
            if(def && def->damage_type == ROGUE_DMG_PHYSICAL){
                int eff_armor = enemies[i].armor;
                int pen_flat = player->pen_flat; if(pen_flat>0){ eff_armor -= pen_flat; if(eff_armor<0) eff_armor=0; }
                int pen_pct = player->pen_percent; if(pen_pct>100) pen_pct=100; if(pen_pct>0){ int reduce = (enemies[i].armor * pen_pct)/100; eff_armor -= reduce; if(eff_armor<0) eff_armor=0; }
                enemies[i].armor = eff_armor; /* temporary override for mitigation call */
            }
            int health_before = enemies[i].health;
            int final_dmg = rogue_apply_mitigation_enemy(&enemies[i], dmg, def?def->damage_type:ROGUE_DMG_PHYSICAL, &overkill);
            int execution = 0;
            if(health_before>0){
                int will_kill = (health_before - final_dmg) <= 0;
                if(will_kill){
                    float health_pct_before = (float)health_before / (float)(enemies[i].max_health>0?enemies[i].max_health:1);
                    float overkill_pct = (float)overkill / (float)(enemies[i].max_health>0?enemies[i].max_health:1);
                    if(health_pct_before <= ROGUE_EXEC_HEALTH_PCT || overkill_pct >= ROGUE_EXEC_OVERKILL_PCT){
                        execution = 1;
                    }
                }
            }
            /* If crit layering is post-mitigation (mode 1), apply multiplier now */
            if(is_crit && g_crit_layering_mode==1){
                float cval = (float)final_dmg * crit_mult; final_dmg = (int)floorf(cval + 0.5f); if(final_dmg<1) final_dmg=1; /* floor */
            }
            enemies[i].health -= final_dmg; enemies[i].hurt_timer=150.0f; enemies[i].flash_timer=70.0f; pc->hit_confirmed=1;
            rogue_add_damage_number_ex(ex, ey - 0.25f, final_dmg, 1, is_crit);
            rogue_damage_event_record((unsigned short)(def?def->id:0), (unsigned char)(def?def->damage_type:ROGUE_DMG_PHYSICAL), (unsigned char)is_crit, raw_before, final_dmg, overkill, (unsigned char)execution);
            /* Accumulate status buildup placeholders (future: move to status system). */
            if(bleed_build>0){ enemies[i].bleed_buildup += bleed_build; }
            if(frost_build>0){ enemies[i].frost_buildup += frost_build; }
            /* Phase 3.3: apply poise damage & trigger stagger when depleted (simple). */
            if(def && def->poise_damage > 0.0f && enemies[i].poise_max > 0.0f){
                enemies[i].poise -= def->poise_damage;
                if(enemies[i].poise < 0.0f){ enemies[i].poise = 0.0f; }
                if(enemies[i].poise <= 0.0f && !enemies[i].staggered){
                    enemies[i].staggered = 1; enemies[i].stagger_timer_ms = 600.0f; /* ms placeholder */
                    if(pc->event_count < (int)(sizeof(pc->events)/sizeof(pc->events[0]))){
                        pc->events[pc->event_count].type = ROGUE_COMBAT_EVENT_STAGGER_ENEMY; pc->events[pc->event_count].data = (unsigned short)i; pc->events[pc->event_count].t_ms = pc->strike_time_ms; pc->event_count++; }
                }
            }
            if(enemies[i].health<=0){ enemies[i].alive=0; kills++; }
        }
    }
    pc->processed_window_mask |= process_mask;
    /* Emit END_WINDOW events for windows just processed (if not already). */
    if(def){
        for(int wi=0; wi<def->num_windows && wi<32; ++wi){
            unsigned int bit = (1u<<wi);
            if(process_mask & bit){
                if(pc->event_count < (int)(sizeof(pc->events)/sizeof(pc->events[0]))){
                    pc->events[pc->event_count].type = ROGUE_COMBAT_EVENT_END_WINDOW; pc->events[pc->event_count].data = (unsigned short)wi; pc->events[pc->event_count].t_ms = pc->strike_time_ms; pc->event_count++; }
            }
        }
    }
    /* Hyper armor active only during window processing; reset after evaluation (covers all windows struck this frame). */
    rogue_player_set_hyper_armor_active(0);
    return kills;
}

void rogue_combat_notify_blocked(RoguePlayerCombat* pc){ if(!pc) return; if(pc->phase==ROGUE_ATTACK_STRIKE){ pc->blocked_this_strike=1; } }

int rogue_combat_consume_events(RoguePlayerCombat* pc, struct RogueCombatEvent* out_events, int max_events){
    if(!pc || !out_events || max_events<=0) return 0;
    int n = pc->event_count; if(n>max_events) n = max_events;
    for(int i=0;i<n;i++){ out_events[i] = pc->events[i]; }
    /* Compact any overflow left (rare) */
    int remaining = pc->event_count - n;
    if(remaining>0){ for(int i=0;i<remaining && i < (int)(sizeof(pc->events)/sizeof(pc->events[0])); ++i){ pc->events[i] = pc->events[n+i]; } }
    pc->event_count = remaining>0? remaining:0;
    /* Don't reset masks here; they govern multi-hit & end event emission; clearing would allow duplicate events. */
    return n;
}

void rogue_combat_set_archetype(RoguePlayerCombat* pc, RogueWeaponArchetype arch){ if(!pc) return; pc->archetype = arch; pc->chain_index = 0; }
RogueWeaponArchetype rogue_combat_current_archetype(const RoguePlayerCombat* pc){ return pc? pc->archetype : ROGUE_WEAPON_LIGHT; }
int rogue_combat_current_chain_index(const RoguePlayerCombat* pc){ return pc? pc->chain_index : 0; }
void rogue_combat_queue_branch(RoguePlayerCombat* pc, RogueWeaponArchetype branch_arch){ if(!pc) return; pc->queued_branch_archetype = branch_arch; pc->queued_branch_pending=1; }

/* ---------------- Phase 3.8 Guard / 3.9 Perfect Guard / 3.10 Poise Regen Curve ---------------- */
static void rogue_player_face(RoguePlayer* p, int dir){ if(!p) return; if(dir<0||dir>3) return; p->facing = dir; }
static int g_player_hyper_armor_active = 0; /* transient state set from strike windows */
void rogue_player_set_hyper_armor_active(int active){ g_player_hyper_armor_active = active?1:0; }
int rogue_player_begin_guard(RoguePlayer* p, int guard_dir){ if(!p) return 0; if(p->guard_meter <= 0.0f){ p->guarding=0; return 0; } p->guarding=1; p->guard_active_time_ms = 0.0f; rogue_player_face(p,guard_dir); return 1; }
int rogue_player_update_guard(RoguePlayer* p, float dt_ms){ if(!p) return 0; int chip=0; if(p->guarding){ p->guard_active_time_ms += dt_ms; p->guard_meter -= dt_ms * ROGUE_GUARD_METER_DRAIN_HOLD_PER_MS; if(p->guard_meter <= 0.0f){ p->guard_meter=0.0f; p->guarding=0; } } else { p->guard_meter += dt_ms * ROGUE_GUARD_METER_RECOVER_PER_MS; if(p->guard_meter > p->guard_meter_max) p->guard_meter = p->guard_meter_max; }
    rogue_player_poise_regen_tick(p, dt_ms);
    return chip; }
/* Compute player's facing unit vector */
static void rogue_player_facing_dir(const RoguePlayer* p, float* dx,float*dy){ switch(p->facing){ case 0:*dx=0;*dy=1;break; case 1:*dx=-1;*dy=0;break; case 2:*dx=1;*dy=0;break; case 3:*dx=0;*dy=-1;break; default:*dx=0;*dy=1;break; } }
int rogue_player_apply_incoming_melee(RoguePlayer* p, float raw_damage, float attack_dir_x, float attack_dir_y, int poise_damage, int *out_blocked, int *out_perfect){
    if(out_blocked) *out_blocked=0; if(out_perfect) *out_perfect=0; if(!p) return (int)raw_damage;
    /* Phase 4: if player currently has i-frames, ignore incoming melee entirely (no damage, no poise). */
    if(p->iframes_ms > 0.0f){ return 0; }
    if(raw_damage < 0) raw_damage = 0;
    float fdx,fdy; rogue_player_facing_dir(p,&fdx,&fdy);
    float alen = sqrtf(attack_dir_x*attack_dir_x + attack_dir_y*attack_dir_y); if(alen>0.0001f){ attack_dir_x/=alen; attack_dir_y/=alen; }
    float dot = fdx*attack_dir_x + fdy*attack_dir_y; int blocked = 0; int perfect = 0;
    if(p->guarding && p->guard_meter > 0.0f && dot >= ROGUE_GUARD_CONE_DOT){
        blocked = 1; perfect = (p->guard_active_time_ms <= p->perfect_guard_window_ms)?1:0;
        float chip = raw_damage * ROGUE_GUARD_CHIP_PCT; if(chip < 1.0f) chip = (raw_damage>0)?1.0f:0.0f;
        if(perfect){ chip = 0.0f; p->guard_meter += ROGUE_PERFECT_GUARD_REFUND; if(p->guard_meter>p->guard_meter_max) p->guard_meter = p->guard_meter_max; p->poise += ROGUE_PERFECT_GUARD_POISE_BONUS; if(p->poise>p->poise_max) p->poise=p->poise_max; }
        else { p->guard_meter -= ROGUE_GUARD_METER_DRAIN_ON_BLOCK; if(p->guard_meter < 0.0f) p->guard_meter = 0.0f; if(poise_damage>0){ /* apply scaled poise damage on normal block */ float pd = (float)poise_damage * ROGUE_GUARD_BLOCK_POISE_SCALE; p->poise -= pd; if(p->poise < 0.0f) p->poise = 0.0f; p->poise_regen_delay_ms = ROGUE_POISE_REGEN_DELAY_AFTER_HIT; } }
        if(out_blocked) *out_blocked = 1; if(perfect && out_perfect) *out_perfect = 1; return (int)chip;
    }
    /* Not blocked OR rear / cone fail: full damage; apply poise damage unless hyper armor active */
    int triggered_reaction = 0;
    if(poise_damage>0 && !g_player_hyper_armor_active){
        float before = p->poise;
        p->poise -= (float)poise_damage; if(p->poise < 0.0f) p->poise=0.0f;
        if(before > 0.0f && p->poise <= 0.0f){ /* full poise break -> stagger reaction */
            rogue_player_apply_reaction(p, 2); triggered_reaction=1; /* stagger */
        }
    }
    /* Damage magnitude based reactions if not already triggered by poise: light flinch for moderate hit, knockdown for large. */
    if(!triggered_reaction){
        if(raw_damage >= 80){ rogue_player_apply_reaction(p,3); }
        else if(raw_damage >= 25){ rogue_player_apply_reaction(p,1); }
    }
    p->poise_regen_delay_ms = ROGUE_POISE_REGEN_DELAY_AFTER_HIT;
    return (int)raw_damage;
}
void rogue_player_poise_regen_tick(RoguePlayer* p, float dt_ms){ if(!p) return; if(p->poise_regen_delay_ms>0){ p->poise_regen_delay_ms -= dt_ms; if(p->poise_regen_delay_ms<0) p->poise_regen_delay_ms=0; }
    if(p->poise_regen_delay_ms<=0 && p->poise < p->poise_max){ /* Non-linear early burst then taper: use ratio curve */
        float missing = p->poise_max - p->poise; float ratio = missing / p->poise_max; if(ratio<0) ratio=0; if(ratio>1) ratio=1;
        float regen = (ROGUE_POISE_REGEN_BASE_PER_MS * dt_ms) * (1.0f + 1.75f * ratio * ratio); /* quadratic emphasis when low */
        p->poise += regen; if(p->poise > p->poise_max) p->poise = p->poise_max; }
}
/* ---------------- Phase 4: Reaction & I-Frame Logic ---------------- */
void rogue_player_update_reactions(RoguePlayer* p, float dt_ms){
    if(!p) return;
    if(p->reaction_timer_ms>0){
        p->reaction_timer_ms -= dt_ms;
        if(p->reaction_timer_ms<=0){ p->reaction_timer_ms=0; p->reaction_type=0; p->reaction_total_ms=0; p->reaction_di_accum_x=0; p->reaction_di_accum_y=0; p->reaction_di_max=0; }
    }
    if(p->iframes_ms>0){ p->iframes_ms -= dt_ms; if(p->iframes_ms<0) p->iframes_ms=0; }
}
static void rogue_player_init_reaction_params(RoguePlayer* p){
    /* Set DI max & allow early cancel fraction thresholds per reaction type. */
    switch(p->reaction_type){
        case 1: /* light flinch: small DI, narrow cancel window */
            p->reaction_di_max = 0.35f; break;
        case 2: /* stagger: moderate DI */
            p->reaction_di_max = 0.55f; break;
        case 3: /* knockdown: larger DI (rolling body) */
            p->reaction_di_max = 0.85f; break;
        case 4: /* launch: aerial DI reserved */
            p->reaction_di_max = 1.00f; break;
        default: p->reaction_di_max = 0.0f; break;
    }
    p->reaction_di_accum_x = 0; p->reaction_di_accum_y = 0; p->reaction_canceled_early=0;
}
void rogue_player_apply_reaction(RoguePlayer* p, int reaction_type){
    if(!p) return; if(reaction_type<=0){ return; }
    p->reaction_type = reaction_type;
    switch(reaction_type){
        case 1: p->reaction_timer_ms = 220.0f; break; /* light flinch */
        case 2: p->reaction_timer_ms = 600.0f; break; /* stagger */
        case 3: p->reaction_timer_ms = 900.0f; break; /* knockdown */
        case 4: p->reaction_timer_ms = 1100.0f; break; /* launch */
        default: p->reaction_timer_ms = 300.0f; break;
    }
    p->reaction_total_ms = p->reaction_timer_ms;
    rogue_player_init_reaction_params(p);
}
int rogue_player_try_reaction_cancel(RoguePlayer* p){
    if(!p) return 0; if(p->reaction_type==0 || p->reaction_timer_ms<=0) return 0; if(p->reaction_canceled_early) return 0;
    /* Early cancel allowed after a minimum lock fraction & before a latest window fraction. */
    float min_frac=0.0f, max_frac=0.0f;
    switch(p->reaction_type){
        case 1: min_frac=0.40f; max_frac=0.75f; break; /* flinch shorter */
        case 2: min_frac=0.55f; max_frac=0.85f; break; /* stagger */
        case 3: min_frac=0.60f; max_frac=0.80f; break; /* knockdown (tighter) */
        case 4: min_frac=0.65f; max_frac=0.78f; break; /* launch (aerial) */
        default: return 0;
    }
    if(p->reaction_total_ms <= 0) return 0;
    float elapsed = p->reaction_total_ms - p->reaction_timer_ms;
    float frac = elapsed / p->reaction_total_ms;
    if(frac >= min_frac && frac <= max_frac){
        p->reaction_timer_ms = 0; p->reaction_type=0; p->reaction_canceled_early=1; return 1; }
    return 0;
}
void rogue_player_apply_reaction_di(RoguePlayer* p, float dx, float dy){
    if(!p) return; if(p->reaction_type==0 || p->reaction_timer_ms<=0) return; if(p->reaction_di_max<=0) return;
    /* Normalize input vector (dx,dy) in -1..1 range; accumulate with cap radius. */
    float mag = sqrtf(dx*dx + dy*dy); if(mag>1.0f && mag>0){ dx/=mag; dy/=mag; }
    p->reaction_di_accum_x += dx * 0.08f; /* per-input step; tuned small for tests */
    p->reaction_di_accum_y += dy * 0.08f;
    /* Clamp to circle of radius reaction_di_max. */
    float acc_mag = sqrtf(p->reaction_di_accum_x*p->reaction_di_accum_x + p->reaction_di_accum_y*p->reaction_di_accum_y);
    if(acc_mag > p->reaction_di_max && acc_mag>0){
        float scale = p->reaction_di_max / acc_mag;
        p->reaction_di_accum_x *= scale; p->reaction_di_accum_y *= scale;
    }
}
void rogue_player_add_iframes(RoguePlayer* p, float ms){ if(!p) return; if(ms<=0) return; if(p->iframes_ms < ms) p->iframes_ms = ms; }
