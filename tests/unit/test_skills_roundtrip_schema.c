#include "../../src/content/json_envelope.h"
#include "../../src/content/json_io.h"
#include "../../src/content/schema_skills.h"
#include "../../src/core/app/app_state.h"
#include "../../src/core/skills/skills.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int export_first_n_skills_json(char* out, int cap, int n)
{
    if (!out || cap <= 0)
        return -1;
    int count = g_app.skill_count;
    if (count <= 0)
        return -1;
    if (n > count)
        n = count;
    int w = 0;
    int r = snprintf(out + w, cap - w, "[");
    if (r < 0 || w + r >= cap)
        return -1;
    w += r;
    for (int i = 0; i < n; ++i)
    {
        const RogueSkillDef* d = rogue_skill_get_def(i);
        if (!d)
            continue;
        r = snprintf(out + w, cap - w,
                     "%s{\"name\":\"%s\",\"icon\":\"%s\",\"max_rank\":%d,\"skill_strength\":%d,"
                     "\"base_cooldown_ms\":%.3f,\"cooldown_reduction_ms_per_rank\":%.3f,\"is_"
                     "passive\":%d,\"tags\":%d,"
                     "\"synergy_id\":%d,\"synergy_value_per_rank\":%d,\"resource_cost_mana\":%d,"
                     "\"action_point_cost\":%d,"
                     "\"max_charges\":%d,\"charge_recharge_ms\":%.3f,\"cast_time_ms\":%.3f,\"input_"
                     "buffer_ms\":%u,"
                     "\"min_weave_ms\":%u,\"early_cancel_min_pct\":%u,\"cast_type\":%u,\"combo_"
                     "builder\":%u,\"combo_spender\":%u,\"effect_spec_id\":%d}",
                     (i ? "," : ""), d->name ? d->name : "", d->icon ? d->icon : "", d->max_rank,
                     d->skill_strength, d->base_cooldown_ms, d->cooldown_reduction_ms_per_rank,
                     d->is_passive, d->tags, d->synergy_id, d->synergy_value_per_rank,
                     d->resource_cost_mana, d->action_point_cost, d->max_charges,
                     d->charge_recharge_ms, d->cast_time_ms, d->input_buffer_ms, d->min_weave_ms,
                     d->early_cancel_min_pct, d->cast_type, d->combo_builder, d->combo_spender,
                     d->effect_spec_id);
        if (r < 0 || w + r >= cap)
            return -1;
        w += r;
    }
    if (w + 2 >= cap)
        return -1;
    out[w++] = ']';
    out[w] = '\0';
    return w;
}

int main(void)
{
    /* Keep test fast: skip icon textures and seed registry with a tiny JSON set. */
    rogue_skills_set_skip_icon_loads(1);
    const char* seed_path = "skills_rt_seed.json";
    const char* seed_json =
        "[\n"
        "  {\"name\":\"T_Fire\",\"icon\":\"../assets/skills/01_Fireball.png\",\"max_"
        "rank\":2,\"skill_strength\":1,\"base_cooldown_ms\":100.0,\"cooldown_reduction_ms_"
        "per_rank\":5.0,\"is_passive\":0,\"tags\":0,\"synergy_id\":-1,\"synergy_value_"
        "per_rank\":0,\"resource_cost_mana\":10,\"action_point_cost\":1,\"max_charges\":0,"
        "\"charge_recharge_ms\":0.0,\"cast_time_ms\":50.0,\"input_buffer_ms\":10,\"min_"
        "weave_ms\":10,\"early_cancel_min_pct\":0,\"cast_type\":0,\"combo_builder\":0,\""
        "combo_spender\":0,\"effect_spec_id\":1},\n"
        "  {\"name\":\"T_Ice\",\"icon\":\"../assets/skills/03_IceShard.png\",\"max_"
        "rank\":1,\"skill_strength\":2,\"base_cooldown_ms\":0.0,\"cooldown_reduction_ms_"
        "per_rank\":0.0,\"is_passive\":1,\"tags\":0,\"synergy_id\":-1,\"synergy_value_"
        "per_rank\":0,\"resource_cost_mana\":0,\"action_point_cost\":0,\"max_charges\":0,"
        "\"charge_recharge_ms\":0.0,\"cast_time_ms\":0.0,\"input_buffer_ms\":0,\"min_"
        "weave_ms\":0,\"early_cancel_min_pct\":0,\"cast_type\":0,\"combo_builder\":0,\""
        "combo_spender\":0,\"effect_spec_id\":2},\n"
        "  {\"name\":\"T_Blink\",\"icon\":\"../assets/skills/07_Blink.png\",\"max_"
        "rank\":3,\"skill_strength\":1,\"base_cooldown_ms\":250.0,\"cooldown_reduction_"
        "ms_per_rank\":10.0,\"is_passive\":0,\"tags\":0,\"synergy_id\":-1,\"synergy_"
        "value_per_rank\":0,\"resource_cost_mana\":5,\"action_point_cost\":1,\"max_"
        "charges\":1,\"charge_recharge_ms\":5000.0,\"cast_time_ms\":0.0,\"input_buffer_"
        "ms\":5,\"min_weave_ms\":0,\"early_cancel_min_pct\":0,\"cast_type\":0,\"combo_"
        "builder\":0,\"combo_spender\":0,\"effect_spec_id\":3}\n"
        "]\n";
    FILE* sf = fopen(seed_path, "wb");
    if (!sf)
    {
        fprintf(stderr, "failed to create seed json\n");
        return 1;
    }
    fwrite(seed_json, 1, strlen(seed_json), sf);
    fclose(sf);
    int loaded = rogue_skills_load_from_cfg(seed_path);
    if (loaded <= 0)
    {
        fprintf(stderr, "Failed to seed skills registry\n");
        return 1;
    }

    /* Export a small subset to keep test fast */
    char entries[16384];
    int wn = export_first_n_skills_json(entries, (int) sizeof entries, 3);
    if (wn < 0)
    {
        fprintf(stderr, "Export failed\n");
        return 2;
    }

    /* Wrap in envelope */
    char* wrapped = NULL;
    char err[256];
    if (json_envelope_create("skills", ROGUE_SCHEMA_VERSION_CURRENT, entries, &wrapped, err,
                             (int) sizeof err) != 0)
    {
        fprintf(stderr, "Envelope create failed: %s\n", err);
        return 3;
    }

    /* Validate via schema (object-by-object) */
    RogueSchemaValidationResult vr = {0};
    if (!rogue_skills_validate_defs(g_app.skill_defs, g_app.skill_count, &vr))
    {
        fprintf(stderr, "Schema validate failed: %s\n",
                (vr.error_count ? vr.errors[0].message : "unknown"));
        free(wrapped);
        return 4;
    }

    /* Write to cwd (ctest runs from build/) */
    const char* out_path = "skills_rt_enveloped.json";
    if (json_io_write_atomic(out_path, wrapped, (int) strlen(wrapped), err, (int) sizeof err) != 0)
    {
        fprintf(stderr, "Write failed: %s\n", err);
        free(wrapped);
        return 5;
    }
    free(wrapped);

    /* Now reload via the skills loader (must accept enveloped file) */
    int reloaded = rogue_skills_load_from_cfg(out_path);
    if (reloaded <= 0)
    {
        fprintf(stderr, "Reload failed from envelope (%d)\n", reloaded);
        return 6;
    }

    printf(
        "OK test_skills_roundtrip_schema: exported+validated+reloaded (%d initial, %d reloaded)\n",
        loaded, reloaded);
    return 0;
}
