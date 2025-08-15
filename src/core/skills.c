#include "core/skills.h"
#include "core/app_state.h"
#include <stdlib.h>
#include <string.h>

/* Forward (defined in effect_spec.c) - placed at file scope to avoid C4210 warning under MSVC */
void rogue_effect_apply(int effect_spec_id, double now_ms);

/* Forward declaration (implemented in persistence.c) */
void rogue_persistence_save_player_stats(void);

/* Simple dynamic array for skill defs */
static RogueSkillDef* g_defs = NULL;
static RogueSkillState* g_states = NULL;
static int g_capacity = 0;
static int g_count = 0;
/* Simple synergy buckets (fixed small number for now) */
#define ROGUE_MAX_SYNERGIES 16
static int g_synergy_totals[ROGUE_MAX_SYNERGIES];

static void recompute_synergies(void){
    for(int i=0;i<ROGUE_MAX_SYNERGIES;i++) g_synergy_totals[i]=0;
    for(int i=0;i<g_count;i++){
        const RogueSkillDef* d=&g_defs[i]; const RogueSkillState* st=&g_states[i];
        if(d->is_passive && d->synergy_id>=0 && d->synergy_id<ROGUE_MAX_SYNERGIES){
            g_synergy_totals[d->synergy_id] += st->rank * d->synergy_value_per_rank;
        }
    }
}

static void ensure_capacity(int min_cap){
    if(g_capacity >= min_cap) return;
    int new_cap = g_capacity? g_capacity*2:8; if(new_cap<min_cap) new_cap=min_cap;
    RogueSkillDef* nd = (RogueSkillDef*)realloc(g_defs, (size_t)new_cap*sizeof(RogueSkillDef));
    if(!nd) return; g_defs=nd;
    RogueSkillState* ns = (RogueSkillState*)realloc(g_states, (size_t)new_cap*sizeof(RogueSkillState));
    if(!ns) return; g_states=ns;
    for(int i=g_capacity;i<new_cap;i++){ g_states[i].rank=0; g_states[i].cooldown_end_ms=0; g_states[i].uses=0; g_states[i].charges_cur=0; g_states[i].next_charge_ready_ms=0; g_states[i].last_cast_ms=0; g_states[i].cast_progress_ms=0; g_states[i].channel_end_ms=0; g_states[i].queued_until_ms=0; g_states[i].queued_trigger_ms=0; g_states[i].channel_next_tick_ms=0; g_states[i].action_points_spent_session=0; g_states[i].combo_points_accum=0; g_states[i].casting_active=0; g_states[i].channel_active=0; }
    g_capacity = new_cap;
}

void rogue_skills_init(void){
    g_defs=NULL; g_states=NULL; g_capacity=0; g_count=0;
    g_app.skill_defs = NULL; g_app.skill_states=NULL; g_app.skill_count=0;
    for(int i=0;i<10;i++) g_app.skill_bar[i]=-1;
    g_app.talent_points = 0;
    for(int i=0;i<ROGUE_MAX_SYNERGIES;i++) g_synergy_totals[i]=0;
}

void rogue_skills_shutdown(void){
    free(g_defs); g_defs=NULL;
    free(g_states); g_states=NULL;
    g_capacity=0; g_count=0;
    g_app.skill_defs=NULL; g_app.skill_states=NULL; g_app.skill_count=0;
}

int rogue_skill_register(const RogueSkillDef* def){
    if(!def) return -1;
    ensure_capacity(g_count+1);
    RogueSkillDef* d = &g_defs[g_count];
    *d = *def; /* shallow copy (icon/name assumed static) */
    d->id = g_count;
    g_states[g_count].rank=0; g_states[g_count].cooldown_end_ms=0; g_states[g_count].uses=0; g_states[g_count].charges_cur=d->max_charges>0?d->max_charges:0; g_states[g_count].next_charge_ready_ms=0; g_states[g_count].last_cast_ms=0; g_states[g_count].cast_progress_ms=0; g_states[g_count].channel_end_ms=0; g_states[g_count].queued_until_ms=0; g_states[g_count].queued_trigger_ms=0; g_states[g_count].channel_next_tick_ms=0; g_states[g_count].action_points_spent_session=0; g_states[g_count].combo_points_accum=0; g_states[g_count].casting_active=0; g_states[g_count].channel_active=0;
    g_count++;
    g_app.skill_defs = g_defs; g_app.skill_states = g_states; g_app.skill_count=g_count;
    return d->id;
}

int rogue_skill_rank_up(int id){
    if(id<0 || id>=g_count) return -1;
    RogueSkillState* st = &g_states[id];
    const RogueSkillDef* def = &g_defs[id];
    if(st->rank >= def->max_rank) return st->rank;
    if(g_app.talent_points<=0) return -1;
    st->rank++; g_app.talent_points--; g_app.stats_dirty=1; recompute_synergies();
    /* Immediate save to persist talent point spend and new rank */
    rogue_persistence_save_player_stats();
    return st->rank;
}

int rogue_skill_try_activate(int id, const RogueSkillCtx* ctx){
    if(id<0 || id>=g_count) return 0;
    RogueSkillState* st = &g_states[id];
    const RogueSkillDef* def = &g_defs[id];
    if(st->rank<=0) return 0; /* locked */
    if(def->is_passive) return 0; /* passives cannot be 'activated' */
    double now = ctx? ctx->now_ms : 0.0;
    /* Cooldown */
    if(now < st->cooldown_end_ms) return 0;
    /* Charges */
    if(def->max_charges>0){
        /* passive recharge check */
        if(st->charges_cur < def->max_charges && st->next_charge_ready_ms>0 && now >= st->next_charge_ready_ms){
            st->charges_cur++; if(st->charges_cur < def->max_charges){ st->next_charge_ready_ms = now + def->charge_recharge_ms; } else { st->next_charge_ready_ms = 0; }
        }
        if(st->charges_cur<=0) return 0;
    }
    /* Resource checks */
    if(def->resource_cost_mana>0){ if(g_app.player.mana < def->resource_cost_mana) return 0; }
    if(def->action_point_cost>0){ if(g_app.player.action_points < def->action_point_cost) return 0; }
    /* Deterministic RNG seed (id + uses) */
    RogueSkillCtx local_ctx = ctx? *ctx : (RogueSkillCtx){0};
    local_ctx.rng_state = (unsigned int)(id * 2654435761u) ^ (unsigned int)st->uses * 2246822519u;
    int consumed = 1;
    /* If casting/channeling, possibly queue if already busy with another cast and within buffer window */
    if((def->cast_type==1 && def->cast_time_ms>0) || (def->cast_type==0 && def->input_buffer_ms>0)){
        /* If a cast is already active for this skill, ignore duplicate */
        if(st->casting_active){ return 0; }
        /* Simple global scan for an active cast to allow buffering */
        for(int i2=0;i2<g_count;i2++){
            if(i2==id) continue;
            RogueSkillState* other=&g_states[i2]; const RogueSkillDef* odef=&g_defs[i2];
            if(other->casting_active && odef->cast_type==1 && odef->cast_time_ms>0){
                /* check if within buffer window from other cast end estimate */
                double other_remaining = odef->cast_time_ms - other->cast_progress_ms;
                double projected_finish = now + (other_remaining>0? other_remaining:0);
                unsigned short buf = def->input_buffer_ms;
                if(buf>0){
                    st->queued_until_ms = projected_finish + (double)buf;
                    st->queued_trigger_ms = projected_finish; /* attempt exactly when other finishes */
                    return 1; /* accept queue (do not consume resources yet) */
                }
            }
        }
    }
    /* If skill has cast time or is a channel, start casting instead of immediate effect */
    if(def->cast_type==1 && def->cast_time_ms > 0){
        st->casting_active = 1; st->cast_progress_ms = 0; st->channel_active = 0; consumed = 1; /* resources spent upfront */
    } else if(def->cast_type==2 && def->cast_time_ms > 0){
        st->channel_active = 1; st->casting_active = 0; st->channel_end_ms = now + def->cast_time_ms; consumed = 1; /* spend upfront */
        /* schedule first tick immediately and subsequent ticks every 250ms (placeholder) */
        double tick_interval = 250.0;
        st->channel_next_tick_ms = now; /* immediate */
        if(def->on_activate){ /* initial tick for channel start */ def->on_activate(def, st, &local_ctx); }
        st->channel_next_tick_ms = now + tick_interval; /* next tick */
    } else {
        if(def->on_activate){ consumed = def->on_activate(def, st, &local_ctx); }
    }
    if(consumed){
    /* Spend mana */
        if(def->resource_cost_mana>0){ g_app.player.mana -= def->resource_cost_mana; if(g_app.player.mana<0) g_app.player.mana=0; }
    /* Spend AP */
    if(def->action_point_cost>0){
        g_app.player.action_points -= def->action_point_cost; if(g_app.player.action_points<0) g_app.player.action_points=0; st->action_points_spent_session += def->action_point_cost;
        /* Trigger/extend soft throttle when large AP chunk is spent */
        if(def->action_point_cost >= 25){
            float extend = 1500.0f + def->action_point_cost * 10.0f; /* scale throttle duration modestly */
            if(g_app.ap_throttle_timer_ms < extend) g_app.ap_throttle_timer_ms = extend;
        }
    }
        /* Spend charge */
        if(def->max_charges>0){ st->charges_cur--; if(st->charges_cur < def->max_charges && st->next_charge_ready_ms==0){ st->next_charge_ready_ms = now + def->charge_recharge_ms; } }
        /* Cooldown compute */
        float cd;
#ifdef ROGUE_TEST_SHORT_COOLDOWNS
        cd = 1000.0f;
#else
        cd = def->base_cooldown_ms - (st->rank-1)*def->cooldown_reduction_ms_per_rank; if(cd<100) cd=100;
#endif
        st->cooldown_end_ms = now + cd;
        st->uses++;
    st->last_cast_ms = now;
    /* Apply EffectSpec immediately only for instant or channel start (not delayed cast) */
    if(def->effect_spec_id >=0 && !(def->cast_type==1 && def->cast_time_ms>0)){ rogue_effect_apply(def->effect_spec_id, now); }
    }
    return consumed;
}

void rogue_skills_update(double now_ms){
    /* Charge regeneration idle path */
    for(int i=0;i<g_count;i++){
        RogueSkillState* st=&g_states[i]; const RogueSkillDef* def=&g_defs[i];
        if(def->max_charges>0 && st->charges_cur < def->max_charges && st->next_charge_ready_ms>0 && now_ms >= st->next_charge_ready_ms){
            st->charges_cur++; if(st->charges_cur < def->max_charges){ st->next_charge_ready_ms = now_ms + def->charge_recharge_ms; } else { st->next_charge_ready_ms = 0; }
        }
        /* Casting progression */
        if(st->casting_active && def->cast_type==1 && def->cast_time_ms>0){
            st->cast_progress_ms += 16.0; /* assume ~60fps step (improved later with dt) */
            if(st->cast_progress_ms >= def->cast_time_ms){
                st->casting_active = 0; st->cast_progress_ms = def->cast_time_ms;
                /* On cast complete apply effect & EffectSpec */
                RogueSkillCtx ctx = {0}; ctx.now_ms = now_ms; ctx.rng_state = (unsigned int)(i * 2654435761u) ^ (unsigned int)st->uses * 2246822519u;
                if(def->on_activate){ def->on_activate(def, st, &ctx); }
                if(def->effect_spec_id >=0){ rogue_effect_apply(def->effect_spec_id, now_ms); }
                /* After this cast completes, attempt to fire any buffered cast skill whose trigger matches now */
                for(int qi=0; qi<g_count; ++qi){
                    RogueSkillState* qst=&g_states[qi];
                    if(qst->queued_trigger_ms>0 && now_ms >= qst->queued_trigger_ms && now_ms <= qst->queued_until_ms){
                        /* Clear queue markers before activation to avoid recursion */
                        qst->queued_trigger_ms=0; qst->queued_until_ms=0;
                        RogueSkillCtx qctx={0}; qctx.now_ms = now_ms; qctx.player_level=0; qctx.talent_points=0;
                        rogue_skill_try_activate(qi, &qctx);
                    }
                }
            }
        }
        /* Channel periodic ticks (1A.5 basic scheduler) */
        if(st->channel_active && def->cast_type==2 && def->cast_time_ms>0){
            double tick_interval = 250.0; /* fixed quantum for now */
            while(st->channel_active && st->channel_next_tick_ms>0 && now_ms >= st->channel_next_tick_ms){
                RogueSkillCtx ctx={0}; ctx.now_ms=st->channel_next_tick_ms; ctx.rng_state = (unsigned int)(i * 2654435761u) ^ (unsigned int)st->uses * 2246822519u + (unsigned int)(st->channel_next_tick_ms);
                if(def->on_activate){ def->on_activate(def, st, &ctx); }
                /* EffectSpec per tick */
                if(def->effect_spec_id>=0){ rogue_effect_apply(def->effect_spec_id, st->channel_next_tick_ms); }
                st->channel_next_tick_ms += tick_interval;
                if(st->channel_next_tick_ms > st->channel_end_ms){ st->channel_next_tick_ms = 0; }
            }
            if(now_ms >= st->channel_end_ms){
                st->channel_active = 0; /* end channel */
            }
        }
    }
}

const RogueSkillDef* rogue_skill_get_def(int id){ if(id<0 || id>=g_count) return NULL; return &g_defs[id]; }
const RogueSkillState* rogue_skill_get_state(int id){ if(id<0 || id>=g_count) return NULL; return &g_states[id]; }

int rogue_skill_synergy_total(int synergy_id){ if(synergy_id<0 || synergy_id>=ROGUE_MAX_SYNERGIES) return 0; return g_synergy_totals[synergy_id]; }
