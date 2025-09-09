#include "vendor_ui_filters.h"
#include <string.h>

static RogueVendorUiFilters g_filters;
static int g_inited = 0;

static void clamp_filters(RogueVendorUiFilters* f)
{
    if (!f)
        return;
    if (f->min_rarity < 0)
        f->min_rarity = 0;
    if (f->min_rarity > 4)
        f->min_rarity = 4;
    if (f->max_rarity < 0)
        f->max_rarity = 0;
    if (f->max_rarity > 4)
        f->max_rarity = 4;
    if (f->min_rarity > f->max_rarity)
        f->min_rarity = f->max_rarity;
    if (f->stat_min > f->stat_max)
        f->stat_min = f->stat_max;
}

static void ensure_init(void)
{
    if (g_inited)
        return;
    memset(&g_filters, 0, sizeof g_filters);
    g_filters.category_mask = -1; /* all categories visible by default */
    g_filters.min_rarity = 0;
    g_filters.max_rarity = 4;
    g_filters.stat_min = 0;
    g_filters.stat_max = 999999;
    g_inited = 1;
}

RogueVendorUiFilters rogue_vendor_ui_get_filters(void)
{
    ensure_init();
    return g_filters;
}

void rogue_vendor_ui_set_filters(const RogueVendorUiFilters* f)
{
    ensure_init();
    if (!f)
        return;
    g_filters = *f;
    clamp_filters(&g_filters);
    /* TODO (Phase 16.7+): persist to save if desired; for now, static session store. */
}

void rogue_vendor_ui_reset_filters(void)
{
    g_inited = 0;
    ensure_init();
}
