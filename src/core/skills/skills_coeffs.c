#include "skills_coeffs.h"
#include "../../game/stat_cache.h"
#include "skills.h"
#include <string.h>

/* Simple static registry; small cap is fine for tests and early content. */
#define ROGUE_MAX_SKILL_COEFFS 256
typedef struct CoeffEntry
{
    int skill_id;
    RogueSkillCoeffParams params;
    int in_use;
} CoeffEntry;

static CoeffEntry g_coeffs[ROGUE_MAX_SKILL_COEFFS];

int rogue_skill_coeff_register(int skill_id, const RogueSkillCoeffParams* params)
{
    if (!params)
        return -1;
    /* Update if existing */
    for (int i = 0; i < ROGUE_MAX_SKILL_COEFFS; ++i)
    {
        if (g_coeffs[i].in_use && g_coeffs[i].skill_id == skill_id)
        {
            g_coeffs[i].params = *params;
            return 0;
        }
    }
    /* Insert new */
    for (int i = 0; i < ROGUE_MAX_SKILL_COEFFS; ++i)
    {
        if (!g_coeffs[i].in_use)
        {
            g_coeffs[i].in_use = 1;
            g_coeffs[i].skill_id = skill_id;
            g_coeffs[i].params = *params;
            return 0;
        }
    }
    return -2;
}

/* Internal: fetch rank for a skill id; returns 0 if unknown. */
static int get_skill_rank(int skill_id)
{
    const struct RogueSkillState* st = rogue_skill_get_state(skill_id);
    return st ? st->rank : 0;
}

/* Compute stat-based percentage using soft-cap helper; returns e.g., 0.12 for +12%. */
static float stat_contrib_pct(float pct_per10, int stat_total, float cap, float softness)
{
    if (pct_per10 <= 0.0f || stat_total <= 0)
        return 0.0f;
    /* Raw linear before cap: (stat/10)*pct_per10 */
    float raw_pct = ((float) stat_total / 10.0f) * pct_per10;
    float adj_pct = raw_pct;
    if (cap > 0.0f && softness > 0.0f)
        adj_pct = rogue_soft_cap_apply(raw_pct, cap, softness);
    /* Ensure contribution never exceeds the configured cap (tests expect <= cap). */
    if (cap > 0.0f && adj_pct > cap)
        adj_pct = cap;
    return adj_pct / 100.0f; /* convert to scalar fraction */
}

float rogue_skill_coeff_get_scalar(int skill_id)
{
    const CoeffEntry* entry = NULL;
    for (int i = 0; i < ROGUE_MAX_SKILL_COEFFS; ++i)
    {
        if (g_coeffs[i].in_use && g_coeffs[i].skill_id == skill_id)
        {
            entry = &g_coeffs[i];
            break;
        }
    }
    if (!entry)
        return 1.0f;

    const RogueSkillCoeffParams* p = &entry->params;
    int rank = get_skill_rank(skill_id);
    if (rank <= 0)
        return 1.0f; /* locked -> baseline */

    float scalar = p->base_scalar;
    if (rank > 1)
        scalar += p->per_rank_scalar * (float) (rank - 1);

    /* Stat contributions from global stat cache totals. */
    /* Ensure cache is reasonably up to date; callers typically keep it refreshed. */
    int STR = g_player_stat_cache.total_strength;
    int INT = g_player_stat_cache.total_intelligence;
    int DEX = g_player_stat_cache.total_dexterity;

    float add_pct = 0.0f;
    add_pct += stat_contrib_pct(p->str_pct_per10, STR, p->stat_cap_pct, p->stat_softness);
    add_pct += stat_contrib_pct(p->int_pct_per10, INT, p->stat_cap_pct, p->stat_softness);
    add_pct += stat_contrib_pct(p->dex_pct_per10, DEX, p->stat_cap_pct, p->stat_softness);

    if (add_pct < -0.9f)
        add_pct = -0.9f; /* avoid negative runaway; keep >=10% of base */

    return scalar * (1.0f + add_pct);
}
