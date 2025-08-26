#include "localization.h"
#include <string.h>

static RogueLocalePair g_default_pairs[] = {
    {"menu_continue", "Continue"},
    {"menu_new_game", "New Game"},
    {"menu_load", "Load Game"},
    {"menu_settings", "Settings"},
    {"menu_credits", "Credits"},
    {"menu_quit", "Quit"},
    {"menu_seed", "Seed:"},
    {"prompt_start", "Press Enter to start"},
    {"tip_settings", "Settings coming soon"},
    {"tip_credits", "Credits coming soon"},
    {"tip_continue", "Load your latest save"},
    {"tip_load", "Choose a save slot to load"},
    {"hint_accept_cancel", "Enter: select, Esc: back"},
    {"confirm_delete_title", "Delete Save?"},
    {"confirm_delete_body", "This will permanently remove the selected slot."},
    {"confirm_delete_hint", "Enter: Yes, Esc: No"},
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
