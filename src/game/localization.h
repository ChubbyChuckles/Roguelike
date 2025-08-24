#ifndef ROGUE_CORE_LOCALIZATION_H
#define ROGUE_CORE_LOCALIZATION_H

typedef struct RogueLocalePair
{
    const char* key;
    const char* value;
} RogueLocalePair;

/* Replace the active locale table (shallow pointer to caller-owned static data is fine). */
void rogue_locale_set_table(const RogueLocalePair* pairs, int count);
/* Reset to built-in defaults (en-US). */
void rogue_locale_reset(void);
/* Lookup a localized string; returns key if not found to make missing keys visible. */
const char* rogue_locale_get(const char* key);

#endif
