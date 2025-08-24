/* Skill activation & ticking logic */
#include "../../game/buffs.h"
#include "../app/app_state.h"
#include "skills_internal.h"
#include <stdlib.h>
#include <string.h>

void rogue_effect_apply(int effect_spec_id, double now_ms);

int rogue_skill_try_activate(int id, const RogueSkillCtx* ctx)
{
    extern unsigned int g_skill_defs_canary;
    extern unsigned int g_skill_states_canary;
    if (g_skill_defs_canary != 0xABCD1234u || g_skill_states_canary != 0xBEEF5678u)
    {
        fprintf(stderr, "SKILL CANARY CORRUPTION BEFORE ACTIVATE id=%d\n", id);
        abort();
    }
    if (id < 0 || id >= g_skill_count_internal)
        return 0;
    RogueSkillState* st = &g_skill_states_internal[id];
    const RogueSkillDef* def = &g_skill_defs_internal[id];
    if (st->rank <= 0)
        return 0;
    if (def->is_passive)
        return 0;
    double now = ctx ? ctx->now_ms : 0.0;
    if (now < st->cooldown_end_ms)
        return 0;
    if (def->max_charges > 0)
    {
        if (st->charges_cur < def->max_charges && st->next_charge_ready_ms > 0 &&
            now >= st->next_charge_ready_ms)
        {
            st->charges_cur++;
            if (st->charges_cur < def->max_charges)
            {
                st->next_charge_ready_ms = now + def->charge_recharge_ms;
            }
            else
            {
                st->next_charge_ready_ms = 0;
            }
        }
        if (st->charges_cur <= 0)
            return 0;
    }
    if (def->resource_cost_mana > 0 && g_app.player.mana < def->resource_cost_mana)
        return 0;
    if (def->action_point_cost > 0 && g_app.player.action_points < def->action_point_cost)
        return 0;
    if (def->min_weave_ms > 0 && def->cast_type == 1 && def->cast_time_ms > 0)
    {
        /* Haste override: if a temporary haste buff is active, bypass the min weave gate. */
        int haste = rogue_buffs_get_total(ROGUE_BUFF_POWER_STRIKE);
        int bypass_weave = (haste >= 10); /* threshold: >=10 magnitude grants weave bypass */
        if (!bypass_weave)
        {
            if (st->last_cast_ms > 0 && (now - st->last_cast_ms) < (double) def->min_weave_ms)
            {
                fprintf(stderr,
                        "SKILL DEBUG: block weave id=%d haste=%d now=%.2f last=%.2f delta=%.2f "
                        "min=%u\n",
                        id, haste, now, st->last_cast_ms, now - st->last_cast_ms,
                        def->min_weave_ms);
                return 0;
            }
        }
        else
        {
            fprintf(
                stderr,
                "SKILL DEBUG: bypass weave id=%d haste=%d now=%.2f last=%.2f delta=%.2f min=%u\n",
                id, haste, now, st->last_cast_ms, now - st->last_cast_ms, def->min_weave_ms);
        }
    }
    RogueSkillCtx local_ctx = ctx ? *ctx : (RogueSkillCtx){0};
    local_ctx.rng_state = (unsigned int) (id * 2654435761u) ^ (unsigned int) st->uses * 2246822519u;
    int consumed = 1;
    if ((def->cast_type == 1 && def->cast_time_ms > 0) ||
        (def->cast_type == 0 && def->input_buffer_ms > 0))
    {
        if (st->casting_active)
        {
            return 0;
        }
        for (int i2 = 0; i2 < g_skill_count_internal; i2++)
        {
            if (i2 == id)
                continue;
            RogueSkillState* other = &g_skill_states_internal[i2];
            const RogueSkillDef* odef = &g_skill_defs_internal[i2];
            if (other->casting_active && odef->cast_type == 1 && odef->cast_time_ms > 0)
            {
                double other_remaining = odef->cast_time_ms - other->cast_progress_ms;
                double projected_finish = now + (other_remaining > 0 ? other_remaining : 0);
                unsigned short buf = def->input_buffer_ms;
                if (buf > 0)
                {
                    st->queued_until_ms = projected_finish + (double) buf;
                    st->queued_trigger_ms = projected_finish;
                    return 1;
                }
            }
        }
    }
    if (def->cast_type == 1 && def->cast_time_ms > 0)
    {
        st->casting_active = 1;
        st->cast_progress_ms = 0;
        st->channel_active = 0;
        /* Snapshot haste for cast if flag set */
        int haste = rogue_buffs_get_total(ROGUE_BUFF_POWER_STRIKE);
        double hf = 1.0 - (haste * 0.02);
        if (hf < 0.5)
            hf = 0.5;
        st->haste_factor_cast = (def->haste_mode_flags & 0x1) ? hf : 0.0;
    }
    else if (def->cast_type == 2 && def->cast_time_ms > 0)
    {
        st->channel_active = 1;
        st->casting_active = 0;
        st->channel_start_ms = now;
        st->channel_end_ms = now + def->cast_time_ms;
        /* snapshot or dynamic haste for channel */
        int haste = rogue_buffs_get_total(ROGUE_BUFF_POWER_STRIKE);
        double hf = 1.0 - (haste * 0.02);
        if (hf < 0.5)
            hf = 0.5;
        double tick_interval_base = 250.0;
        if (def->haste_mode_flags & 0x2)
        {
            st->haste_factor_channel = hf;
            st->channel_tick_interval_ms = tick_interval_base * hf;
        }
        else
        {
            st->haste_factor_channel = 0.0;
            st->channel_tick_interval_ms = 0.0;
        }
        /* schedule first tick at exact grid to minimize drift */
        double tick_interval = (st->channel_tick_interval_ms > 0.0) ? st->channel_tick_interval_ms
                                                                    : tick_interval_base;
        st->channel_next_tick_ms = now + tick_interval;
        if (def->on_activate)
        {
            def->on_activate(def, st, &local_ctx);
        }
        /* keep next tick as scheduled above */
    }
    else
    {
        if (def->on_activate)
        {
            consumed = def->on_activate(def, st, &local_ctx);
        }
    }
    if (consumed)
    {
        if (def->resource_cost_mana > 0)
        {
            g_app.player.mana -= def->resource_cost_mana;
            if (g_app.player.mana < 0)
                g_app.player.mana = 0;
        }
        if (def->action_point_cost > 0)
        {
            g_app.player.action_points -= def->action_point_cost;
            if (g_app.player.action_points < 0)
                g_app.player.action_points = 0;
            st->action_points_spent_session += def->action_point_cost;
            if (def->action_point_cost >= 25)
            {
                float extend = 1500.0f + def->action_point_cost * 10.0f;
                if (g_app.ap_throttle_timer_ms < extend)
                    g_app.ap_throttle_timer_ms = extend;
            }
        }
        if (def->max_charges > 0)
        {
            st->charges_cur--;
            if (st->charges_cur < def->max_charges && st->next_charge_ready_ms == 0)
            {
                st->next_charge_ready_ms = now + def->charge_recharge_ms;
            }
        }
        float cd;
#ifdef ROGUE_TEST_SHORT_COOLDOWNS
        cd = 1000.0f;
#else
        cd = def->base_cooldown_ms - (st->rank - 1) * def->cooldown_reduction_ms_per_rank;
        if (cd < 100)
            cd = 100;
#endif
        st->cooldown_end_ms = now + cd;
        st->uses++;
        st->last_cast_ms = now;
        if (def->effect_spec_id >= 0 && !(def->cast_type == 1 && def->cast_time_ms > 0))
        {
            rogue_effect_apply(def->effect_spec_id, now);
        }
        /* Combo flags: apply builder/spender semantics on successful activation. */
        if (def->combo_builder)
        {
            g_app.player_combat.combo++;
            if (g_app.player_combat.combo > 5)
                g_app.player_combat.combo = 5;
        }
        if (def->combo_spender)
        {
            g_app.player_combat.combo = 0;
        }
    }
    return consumed;
}

int rogue_skill_try_cancel(int id, const RogueSkillCtx* ctx)
{
    if (id < 0 || id >= g_skill_count_internal)
        return 0;
    (void) ctx;
    RogueSkillState* st = &g_skill_states_internal[id];
    const RogueSkillDef* def = &g_skill_defs_internal[id];
    if (!st->casting_active || def->cast_type != 1 || def->cast_time_ms <= 0)
        return 0;
    double progress_pct = (st->cast_progress_ms / def->cast_time_ms) * 100.0;
    if (def->early_cancel_min_pct > 0 && progress_pct < def->early_cancel_min_pct)
        return 0;
    float scalar = (float) (st->cast_progress_ms / def->cast_time_ms);
    double effective_now = (ctx ? ctx->now_ms : 0.0) + st->cast_progress_ms;
    st->casting_active = 0;
    st->cast_progress_ms = 0;
    RogueSkillCtx c2 = ctx ? *ctx : (RogueSkillCtx){0};
    c2.now_ms = effective_now;
    c2.partial_scalar = scalar;
    c2.rng_state = (unsigned int) (id * 2654435761u) ^ (unsigned int) st->uses * 2246822519u;
    if (def->on_activate)
    {
        def->on_activate(def, st, &c2);
    }
    if (def->effect_spec_id >= 0)
    {
        rogue_effect_apply(def->effect_spec_id, c2.now_ms);
    }
    st->last_cast_ms = effective_now;
    return 1;
}

void rogue_skills_update(double now_ms)
{
    extern unsigned int g_skill_defs_canary;
    extern unsigned int g_skill_states_canary;
    if (g_skill_defs_canary != 0xABCD1234u || g_skill_states_canary != 0xBEEF5678u)
    {
        fprintf(stderr, "SKILL CANARY CORRUPTION BEFORE UPDATE\n");
        abort();
    }
    for (int i = 0; i < g_skill_count_internal; i++)
    {
        RogueSkillState* st = &g_skill_states_internal[i];
        const RogueSkillDef* def = &g_skill_defs_internal[i];
        if (def->max_charges > 0 && st->charges_cur < def->max_charges &&
            st->next_charge_ready_ms > 0 && now_ms >= st->next_charge_ready_ms)
        {
            st->charges_cur++;
            if (st->charges_cur < def->max_charges)
            {
                st->next_charge_ready_ms = now_ms + def->charge_recharge_ms;
            }
            else
            {
                st->next_charge_ready_ms = 0;
            }
        }
        if (st->casting_active && def->cast_type == 1 && def->cast_time_ms > 0)
        {
            double haste_factor;
            if (st->haste_factor_cast > 0.0)
                haste_factor = st->haste_factor_cast;
            else
            {
                int haste = rogue_buffs_get_total(ROGUE_BUFF_POWER_STRIKE);
                haste_factor = 1.0 - (haste * 0.02);
                if (haste_factor < 0.5)
                    haste_factor = 0.5;
            }
            st->cast_progress_ms += 16.0 / haste_factor;
            if (st->cast_progress_ms >= def->cast_time_ms)
            {
                st->casting_active = 0;
                st->cast_progress_ms = def->cast_time_ms;
                RogueSkillCtx ctx = {0};
                ctx.now_ms = now_ms;
                ctx.rng_state =
                    (unsigned int) (i * 2654435761u) ^ (unsigned int) st->uses * 2246822519u;
                if (def->on_activate)
                {
                    def->on_activate(def, st, &ctx);
                }
                if (def->effect_spec_id >= 0)
                {
                    rogue_effect_apply(def->effect_spec_id, now_ms);
                }
                /* Combo flags on cast completion */
                if (def->combo_builder)
                {
                    g_app.player_combat.combo++;
                    if (g_app.player_combat.combo > 5)
                        g_app.player_combat.combo = 5;
                }
                if (def->combo_spender)
                {
                    g_app.player_combat.combo = 0;
                }
                for (int qi = 0; qi < g_skill_count_internal; ++qi)
                {
                    RogueSkillState* qst = &g_skill_states_internal[qi];
                    if (qst->queued_trigger_ms > 0 && now_ms >= qst->queued_trigger_ms &&
                        now_ms <= qst->queued_until_ms)
                    {
                        qst->queued_trigger_ms = 0;
                        qst->queued_until_ms = 0;
                        RogueSkillCtx qctx = {0};
                        qctx.now_ms = now_ms;
                        rogue_skill_try_activate(qi, &qctx);
                    }
                }
            }
        }
        if (st->channel_active && def->cast_type == 2 && def->cast_time_ms > 0)
        {
            /* determine tick interval (snapshot vs dynamic) */
            double tick_interval;
            if (st->channel_tick_interval_ms > 0.0)
                tick_interval = st->channel_tick_interval_ms;
            else
            {
                int haste = rogue_buffs_get_total(ROGUE_BUFF_POWER_STRIKE);
                double haste_factor = 1.0 - (haste * 0.02);
                if (haste_factor < 0.5)
                    haste_factor = 0.5;
                tick_interval = 250.0 * haste_factor;
            }
            while (st->channel_active && st->channel_next_tick_ms > 0 &&
                   now_ms >= st->channel_next_tick_ms)
            {
                RogueSkillCtx ctx = {0};
                ctx.now_ms = st->channel_next_tick_ms;
                ctx.rng_state = (unsigned int) (i * 2654435761u) ^
                                (unsigned int) st->uses * 2246822519u +
                                    (unsigned int) (st->channel_next_tick_ms);
                if (def->on_activate)
                {
                    def->on_activate(def, st, &ctx);
                }
                if (def->effect_spec_id >= 0)
                {
                    rogue_effect_apply(def->effect_spec_id, st->channel_next_tick_ms);
                }
                if (def->combo_builder)
                {
                    g_app.player_combat.combo++;
                    if (g_app.player_combat.combo > 5)
                        g_app.player_combat.combo = 5;
                }
                if (def->combo_spender)
                {
                    g_app.player_combat.combo = 0;
                }
                /* drift-correct: compute next tick by counting intervals from channel_start */
                if (st->channel_start_ms <= 0.0)
                    st->channel_start_ms = now_ms;
                double elapsed = (st->channel_next_tick_ms - st->channel_start_ms) + tick_interval;
                int tick_index = (int) (elapsed / tick_interval + 0.5);
                double ideal_next = st->channel_start_ms + tick_interval * (double) tick_index;
                st->channel_next_tick_ms = ideal_next;
                if (st->channel_next_tick_ms > st->channel_end_ms)
                {
                    st->channel_next_tick_ms = 0;
                }
            }
            if (now_ms >= st->channel_end_ms)
            {
                st->channel_active = 0;
            }
        }
    }
}
