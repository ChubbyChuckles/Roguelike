#include "core/localization.h"
#include <string.h>

static RogueLocalePair g_default_pairs[] = {
    {"menu_continue", "Continue"},
    {"menu_new_game", "New Game"},
    {"menu_load", "Load Game"},
    {"menu_settings", "Settings"},
    {"menu_credits", "Credits"},
    {"menu_quit", "Quit"},
    {"menu_seed", "Seed:"},
    {"tip_settings", "Settings coming soon"},
    {"tip_credits", "Credits coming soon"},
    {"hint_accept_cancel", "Enter: select, Esc: back"},
};

static const RogueLocalePair* g_pairs = g_default_pairs;
static int g_pair_count = (int) (sizeof(g_default_pairs) / sizeof(g_default_pairs[0]));

void rogue_locale_set_table(const RogueLocalePair* pairs, int count)
{
    if (pairs && count > 0)
    {
        g_pairs = pairs;
        g_pair_count = count;
    }
}

void rogue_locale_reset(void)
{
    g_pairs = g_default_pairs;
    g_pair_count = (int) (sizeof(g_default_pairs) / sizeof(g_default_pairs[0]));
}

const char* rogue_locale_get(const char* key)
{
    if (!key)
        return "";
    for (int i = 0; i < g_pair_count; ++i)
    {
        if (g_pairs[i].key && 0 == strcmp(g_pairs[i].key, key))
            return g_pairs[i].value ? g_pairs[i].value : "";
    }
    return key; /* fall back to key for visibility */
}
