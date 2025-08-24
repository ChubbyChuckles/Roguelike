#include "effect_spec.h"
#include "buffs.h"
#include <stdlib.h>
#include <string.h>

static RogueEffectSpec* g_effect_specs = NULL;
static int g_effect_spec_count = 0;
static int g_effect_spec_cap = 0;

void rogue_effect_reset(void)
{
    free(g_effect_specs);
    g_effect_specs = NULL;
    g_effect_spec_count = 0;
    g_effect_spec_cap = 0;
}

int rogue_effect_register(const RogueEffectSpec* spec)
{
    if (!spec)
        return -1;
    if (g_effect_spec_count == g_effect_spec_cap)
    {
        int nc = g_effect_spec_cap ? g_effect_spec_cap * 2 : 8;
        RogueEffectSpec* ns = (RogueEffectSpec*) realloc(g_effect_specs, (size_t) nc * sizeof *ns);
        if (!ns)
            return -1;
        g_effect_specs = ns;
        g_effect_spec_cap = nc;
    }
    g_effect_specs[g_effect_spec_count] = *spec;
    g_effect_specs[g_effect_spec_count].id = g_effect_spec_count;
    return g_effect_spec_count++;
}

const RogueEffectSpec* rogue_effect_get(int id)
{
    if (id < 0 || id >= g_effect_spec_count)
        return NULL;
    return &g_effect_specs[id];
}

void rogue_effect_apply(int id, double now_ms)
{
    (void) now_ms; /* now_ms kept for future time-based scaling */
    const RogueEffectSpec* s = rogue_effect_get(id);
    if (!s)
        return;
    switch (s->kind)
    {
    case ROGUE_EFFECT_STAT_BUFF:
        rogue_buffs_apply((RogueBuffType) s->buff_type, s->magnitude, s->duration_ms, now_ms,
                          ROGUE_BUFF_STACK_ADD, 0);
        break;
    default: /* future kinds */
        break;
    }
}
