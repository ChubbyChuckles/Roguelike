#include "infusions.h"

static const RogueInfusionDef g_infusions[] = {
    {0, "None", 1.0f, 0, 0, 0, 0, 0, 1.0f, 1.0f, 1.0f},
    {1, "Fire", 0.85f, 0.25f, 0, 0, 0, 0, 0.95f, 0.95f, 1.05f},
    {2, "Frost", 0.90f, 0, 0.25f, 0, 0, 0, 0.95f, 1.00f, 1.05f},
    {3, "Arcane", 0.80f, 0, 0, 0.33f, 0, 0, 0.90f, 0.90f, 1.20f},
    {4, "Bleed", 0.95f, 0.10f, 0, 0, 15.0f, 0, 1.00f, 1.05f, 0.95f},
    {5, "Poison", 0.95f, 0.05f, 0, 0, 0, 12.0f, 1.00f, 1.00f, 1.00f},
};
static const int g_infusion_count = (int) (sizeof(g_infusions) / sizeof(g_infusions[0]));

const RogueInfusionDef* rogue_infusion_get(int id)
{
    if (id < 0)
        return g_infusions; /* none */
    for (int i = 0; i < g_infusion_count; i++)
    {
        if (g_infusions[i].id == id)
            return &g_infusions[i];
    }
    return g_infusions; /* default none */
}
