#include "progression_specialization.h"
#include "../app/app_state.h"
#include "../skills/skills.h"
#include "progression_attributes.h"
#include <stdlib.h>
#include <string.h>

static unsigned char* g_spec_paths = NULL; /* per-skill path id */
static int g_spec_cap = 0;

static void ensure_cap(int min_id)
{
    if (min_id < g_spec_cap)
        return;
    int need = min_id + 1;
    int nc = g_spec_cap ? g_spec_cap : 16;
    while (nc < need)
        nc *= 2;
    unsigned char* n = (unsigned char*) realloc(g_spec_paths, (size_t) nc);
    if (!n)
        return;
    /* zero init new tail */
    if (nc > g_spec_cap)
        memset(n + g_spec_cap, 0, (size_t) (nc - g_spec_cap));
    g_spec_paths = n;
    g_spec_cap = nc;
}

int rogue_specialization_init(int max_skills)
{
    (void) max_skills; /* dynamic */
    g_spec_paths = NULL;
    g_spec_cap = 0;
    return 0;
}

void rogue_specialization_shutdown(void)
{
    if (g_spec_paths)
    {
        memset(g_spec_paths, 0, (size_t) g_spec_cap);
        free(g_spec_paths);
        g_spec_paths = NULL;
    }
    g_spec_cap = 0;
}

int rogue_specialization_choose(int skill_id, int path_id)
{
    if (skill_id < 0 || path_id <= ROGUE_SPEC_NONE || path_id > ROGUE_SPEC_CONTROL)
        return -1;
    ensure_cap(skill_id);
    if (!g_spec_paths)
        return -3;
    if (g_spec_paths[skill_id] != ROGUE_SPEC_NONE)
        return -2; /* already chosen */
    g_spec_paths[skill_id] = (unsigned char) path_id;
    return 0;
}

int rogue_specialization_get(int skill_id)
{
    if (skill_id < 0)
        return ROGUE_SPEC_NONE;
    if (!g_spec_paths || skill_id >= g_spec_cap)
        return ROGUE_SPEC_NONE;
    return (int) g_spec_paths[skill_id];
}

int rogue_specialization_respec(int skill_id)
{
    if (skill_id < 0)
        return -1;
    if (!g_spec_paths || skill_id >= g_spec_cap || g_spec_paths[skill_id] == ROGUE_SPEC_NONE)
        return -1;
    /* consume respec token from progression attributes */
    if (g_attr_state.respec_tokens <= 0)
        return -2;
    g_attr_state.respec_tokens--;
    g_spec_paths[skill_id] = ROGUE_SPEC_NONE;
    return 0;
}

float rogue_specialization_damage_scalar(int skill_id)
{
    int p = rogue_specialization_get(skill_id);
    if (p == ROGUE_SPEC_POWER)
        return 1.10f; /* +10% damage */
    return 1.00f;
}

float rogue_specialization_cooldown_scalar(int skill_id)
{
    int p = rogue_specialization_get(skill_id);
    if (p == ROGUE_SPEC_CONTROL)
        return 0.90f; /* -10% cooldown */
    return 1.00f;
}
