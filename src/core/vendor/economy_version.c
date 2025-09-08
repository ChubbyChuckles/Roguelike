#include "economy_version.h"

static RogueEconomyHeader g_econ_header = {0, 0, 0};

void rogue_economy_version_reset(void)
{
    g_econ_header.curve_version = 0;
    g_econ_header.margin_policy_version = 0;
    g_econ_header.reserved0 = 0;
}

void rogue_economy_version_set(uint32_t curve_ver, uint32_t margin_ver)
{
    g_econ_header.curve_version = curve_ver;
    g_econ_header.margin_policy_version = margin_ver;
}

RogueEconomyHeader rogue_economy_version_get(void) { return g_econ_header; }
