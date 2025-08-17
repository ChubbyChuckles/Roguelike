#include "core/loot_generation.h"
#include "core/loot_rarity_adv.h"
#include "core/loot_tables.h"
#include "core/loot_item_defs.h"
#include "core/loot_instances.h"
#include "core/loot_affixes.h"
#include "core/loot_perf.h"
#include "core/metrics.h"
#include <string.h>

static float g_quality_scalar_min = 1.0f;
static float g_quality_scalar_max = 1.0f;

void rogue_generation_set_quality_scalar(float qs_min, float qs_max){
    if(qs_min < 0.1f) qs_min = 0.1f; if(qs_max < qs_min) qs_max = qs_min; g_quality_scalar_min = qs_min; g_quality_scalar_max = qs_max;
}

unsigned int rogue_generation_mix_seed(const RogueGenerationContext* ctx, unsigned int base_seed){
    unsigned int h = base_seed * 636413622u + 1442695043u;
    if(ctx){
        h ^= (unsigned)(ctx->enemy_level * 97 + 0x9e3779b9u);
        h = h * 1664525u + 1013904223u;
        h ^= (unsigned)(ctx->biome_id * 131 + 0x85ebca6bu);
        h = h * 22695477u + 1u;
        h ^= (unsigned)(ctx->enemy_archetype * 181 + 0xc2b2ae35u);
        h ^= (unsigned)(ctx->player_luck * 211 + 0x27d4eb2fu);
    }
    return h;
}

/* Externalized in split file (loot_generation_affix.c) for Cleanup 24.4 */
int rogue_generation_gated_affix_roll(RogueAffixType type, int rarity, unsigned int* rng_state,
                                     const RogueItemDef* base_def, int existing_prefix, int existing_suffix);

int rogue_generate_item(int loot_table_index, const RogueGenerationContext* ctx, unsigned int* rng_state, RogueGeneratedItem* out){
    if(!out || !rng_state) return -1; memset(out,0,sizeof *out); out->def_index=-1; out->rarity=-1; out->inst_index=-1;
    if(loot_table_index<0) return -1;
    unsigned int local = rogue_generation_mix_seed(ctx, *rng_state);
    /* Multi-pass: rarity -> base item -> affixes */
    /* For now sample table once producing a single item (reuse existing roll API) */
    int idef_arr[4]; int qty_arr[4]; int rar_arr[4];
    int drops = rogue_loot_roll_ex(loot_table_index, &local, 4, idef_arr, qty_arr, rar_arr);
    if(drops<=0) return -1;
    out->def_index = idef_arr[0];
    int rarity = rar_arr[0]; if(rarity<0){ const RogueItemDef* d = rogue_item_def_at(out->def_index); rarity = d? d->rarity : 0; }
    /* Apply rarity floor from context enemy level (simple mapping: every 10 levels raise floor by 1 up to 2) */
    if(ctx){ int level_floor = ctx->enemy_level / 10; if(level_floor>2) level_floor=2; if(level_floor>0){ if(rarity < level_floor) rarity = level_floor; } }
    /* Apply min floor global */
    int global_floor = rogue_rarity_get_min_floor(); if(global_floor>=0 && rarity < global_floor) rarity = global_floor;
    out->rarity = rarity;
    /* Spawn instance */
    int inst = rogue_items_spawn(out->def_index, qty_arr[0], 0.0f,0.0f); if(inst>=0){
        out->inst_index = inst; unsigned int affix_seed = local ^ 0xA5A5A5A5u;
        RogueItemInstance* it = (RogueItemInstance*)rogue_item_instance_at(inst);
        if(it){
            const RogueItemDef* base_def = rogue_item_def_at(out->def_index);
            int want_prefix=0,want_suffix=0; if(rarity>=2){ if(rarity>=3){ want_prefix=1; want_suffix=1; } else { want_prefix = ((affix_seed)&1)==0; want_suffix = !want_prefix; } }
            int prefix_index=-1,suffix_index=-1; int prefix_value=0,suffix_value=0;
            if(want_prefix || want_suffix){ rogue_loot_perf_affix_roll_begin(); }
            if(want_prefix){ prefix_index = rogue_generation_gated_affix_roll(ROGUE_AFFIX_PREFIX, rarity, &affix_seed, base_def, -1, -1); if(prefix_index>=0){ float qscalar=g_quality_scalar_min; if(ctx){ float luck=(float)ctx->player_luck; float t = luck / (5.0f + luck); qscalar = g_quality_scalar_min + (g_quality_scalar_max - g_quality_scalar_min)*t; } prefix_value = rogue_affix_roll_value_scaled(prefix_index,&affix_seed,qscalar); }}
            if(want_suffix){ suffix_index = rogue_generation_gated_affix_roll(ROGUE_AFFIX_SUFFIX, rarity, &affix_seed, base_def, prefix_index, -1); if(suffix_index>=0){ float qscalar=g_quality_scalar_min; if(ctx){ float luck=(float)ctx->player_luck; float t = luck / (5.0f + luck); qscalar = g_quality_scalar_min + (g_quality_scalar_max - g_quality_scalar_min)*t; } suffix_value = rogue_affix_roll_value_scaled(suffix_index,&affix_seed,qscalar); }}
            if(want_prefix || want_suffix){ rogue_loot_perf_affix_roll_end(); }
            it->rarity = rarity; it->prefix_index=prefix_index; it->prefix_value=prefix_value; it->suffix_index=suffix_index; it->suffix_value=suffix_value;
            /* Record session drop metrics */
            rogue_metrics_record_drop(rarity);
        }
    }
    *rng_state = local; /* propagate mixed seed forward */
    return 0;
}
