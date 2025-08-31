/* dialogue_locale.c - Localization storage for dialogue */
#include "dialogue.h"
#include <string.h>

typedef struct RogueLocEntry
{
    char locale[8];
    char key[64];
    char value[256];
} RogueLocEntry;

#ifndef ROGUE_DIALOGUE_MAX_LOC_ENTRIES
#define ROGUE_DIALOGUE_MAX_LOC_ENTRIES 256
#endif

static RogueLocEntry g_loc_entries[ROGUE_DIALOGUE_MAX_LOC_ENTRIES];
static int g_loc_entry_count = 0;
static char g_active_locale[8] = "en"; /* default */

int rogue_dialogue_locale_register(const char* locale, const char* key, const char* value)
{
    if (!locale || !*locale || !key || !*key || !value)
        return -1;
    for (int i = 0; i < g_loc_entry_count; i++)
    {
        if (strcmp(g_loc_entries[i].locale, locale) == 0 && strcmp(g_loc_entries[i].key, key) == 0)
        {
            size_t vl = strlen(value);
            if (vl > sizeof(g_loc_entries[i].value) - 1)
                vl = sizeof(g_loc_entries[i].value) - 1;
            memcpy(g_loc_entries[i].value, value, vl);
            g_loc_entries[i].value[vl] = '\0';
            return 0;
        }
    }
    if (g_loc_entry_count >= ROGUE_DIALOGUE_MAX_LOC_ENTRIES)
        return -2;
    RogueLocEntry* e = &g_loc_entries[g_loc_entry_count++];
    size_t ll = strlen(locale);
    if (ll > sizeof(e->locale) - 1)
        ll = sizeof(e->locale) - 1;
    memcpy(e->locale, locale, ll);
    e->locale[ll] = '\0';
    size_t kl = strlen(key);
    if (kl > sizeof(e->key) - 1)
        kl = sizeof(e->key) - 1;
    memcpy(e->key, key, kl);
    e->key[kl] = '\0';
    size_t vl = strlen(value);
    if (vl > sizeof(e->value) - 1)
        vl = sizeof(e->value) - 1;
    memcpy(e->value, value, vl);
    e->value[vl] = '\0';
    return 0;
}

int rogue_dialogue_locale_set(const char* locale)
{
    if (!locale || !*locale)
        return -1;
    size_t ll = strlen(locale);
    if (ll > sizeof(g_active_locale) - 1)
        ll = sizeof(g_active_locale) - 1;
    memcpy(g_active_locale, locale, ll);
    g_active_locale[ll] = '\0';
    return 0;
}

const char* rogue_dialogue_locale_active(void) { return g_active_locale; }

void rogue_dialogue_locale_reset(void)
{
    g_loc_entry_count = 0;
    g_active_locale[0] = 'e';
    g_active_locale[1] = 'n';
    g_active_locale[2] = '\0';
}

/* Internal helpers used by dialogue.c */
const char* rogue_dialogue__loc_lookup(const char* key)
{
    if (!key)
        return NULL;
    for (int i = 0; i < g_loc_entry_count; i++)
    {
        if (strcmp(g_loc_entries[i].locale, g_active_locale) == 0 &&
            strcmp(g_loc_entries[i].key, key) == 0)
            return g_loc_entries[i].value;
    }
    return NULL;
}

/* For IS_KEY lines we store key\0fallback_text\0 in text field region */
const char* rogue_dialogue__localized_fallback(const RogueDialogueLine* ln)
{
    if (!ln)
        return NULL;
    if (!(ln->token_flags & ROGUE_DIALOGUE_LINE_IS_KEY))
        return NULL;
    const char* key = ln->text;
    size_t klen = strlen(key);
    const char* fb = key + klen + 1;
    if (!*fb)
        return NULL;
    return fb;
}
