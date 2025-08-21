#include "core/loot/loot_vfx.h"
#include "core/app_state.h"
#include "core/loot/loot_instances.h"
#include <string.h>

/* Simple parallel array indexed by instance slot */
static RogueLootVFXState g_vfx[ROGUE_ITEM_INSTANCE_CAP];

void rogue_loot_vfx_reset(void) { memset(g_vfx, 0, sizeof g_vfx); }

void rogue_loot_vfx_on_spawn(int inst_index, int rarity)
{
    if (inst_index < 0 || inst_index >= ROGUE_ITEM_INSTANCE_CAP)
        return;
    RogueLootVFXState* s = &g_vfx[inst_index];
    memset(s, 0, sizeof *s);
    if (rarity >= 3)
        s->beam_active = 1; /* epic+ pillar */
}

void rogue_loot_vfx_on_despawn(int inst_index)
{
    if (inst_index < 0 || inst_index >= ROGUE_ITEM_INSTANCE_CAP)
        return;
    memset(&g_vfx[inst_index], 0, sizeof g_vfx[inst_index]);
}

int rogue_loot_vfx_get(int inst_index, RogueLootVFXState* out)
{
    if (inst_index < 0 || inst_index >= ROGUE_ITEM_INSTANCE_CAP)
        return 0;
    const RogueItemInstance* it = rogue_item_instance_at(inst_index);
    if (!it)
    {
        return 0;
    }
    if (out)
        *out = g_vfx[inst_index];
    return 1;
}

int rogue_loot_vfx_edge_notifiers(void)
{
    int count = 0;
    for (int i = 0; i < ROGUE_ITEM_INSTANCE_CAP; i++)
        if (g_app.item_instances && g_app.item_instances[i].active)
        {
            float dx = g_app.item_instances[i].x - g_app.player.base.pos.x;
            float dy = g_app.item_instances[i].y - g_app.player.base.pos.y;
            float r2 = dx * dx + dy * dy;
            if (r2 > ROGUE_LOOT_VFX_VIEW_RADIUS * ROGUE_LOOT_VFX_VIEW_RADIUS)
                count++;
        }
    return count;
}

void rogue_loot_vfx_update(float dt_ms)
{
    for (int i = 0; i < ROGUE_ITEM_INSTANCE_CAP; i++)
        if (g_app.item_instances && g_app.item_instances[i].active)
        {
            RogueLootVFXState* s = &g_vfx[i];
            s->sparkle_t_ms += dt_ms;
            if (s->sparkle_t_ms >= ROGUE_LOOT_VFX_SPARKLE_PERIOD_MS)
                s->sparkle_t_ms -= ROGUE_LOOT_VFX_SPARKLE_PERIOD_MS;
            /* Pulse near despawn */
            float life = g_app.item_instances[i].life_ms;
            float limit = (float) ROGUE_ITEM_DESPAWN_MS;
            /* Future: rarity override. For now assume base. */
            float remaining = limit - life;
            if (remaining <= ROGUE_LOOT_VFX_PULSE_WINDOW_MS)
            {
                s->pulse_active = 1;
                float frac = 1.0f - (remaining / ROGUE_LOOT_VFX_PULSE_WINDOW_MS);
                if (frac < 0)
                    frac = 0;
                if (frac > 1)
                    frac = 1;
                s->pulse_alpha = frac;
            }
            else
            {
                s->pulse_active = 0;
                s->pulse_alpha = 0;
            }
        }
}
