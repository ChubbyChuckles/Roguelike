#include "save_intern.h"
#include <stdlib.h>
#include <string.h>

#define ROGUE_SAVE_MAX_STRINGS 256
static char* g_intern_strings[ROGUE_SAVE_MAX_STRINGS];
static int g_intern_count = 0;

int rogue_save_intern_string(const char* s)
{
    if (!s)
        return -1;
    for (int i = 0; i < g_intern_count; i++)
        if (strcmp(g_intern_strings[i], s) == 0)
            return i;
    if (g_intern_count >= ROGUE_SAVE_MAX_STRINGS)
        return -1;
    size_t len = strlen(s);
    char* dup = (char*) malloc(len + 1);
    if (!dup)
        return -1;
    memcpy(dup, s, len + 1);
    g_intern_strings[g_intern_count] = dup;
    return g_intern_count++;
}

const char* rogue_save_intern_get(int index)
{
    if (index < 0 || index >= g_intern_count)
        return NULL;
    return g_intern_strings[index];
}

int rogue_save_intern_count(void) { return g_intern_count; }

void rogue_save_intern_reset_and_reserve(int count)
{
    for (int i = 0; i < g_intern_count; i++)
        free(g_intern_strings[i]);
    g_intern_count = 0;
    if (count < 0)
        count = 0;
    if (count > ROGUE_SAVE_MAX_STRINGS)
        count = ROGUE_SAVE_MAX_STRINGS;
}

void rogue_save_intern_set_loaded(int index, char* owned_string)
{
    if (index < 0)
        return;
    if (index >= ROGUE_SAVE_MAX_STRINGS)
        return;
    if (index >= g_intern_count)
        g_intern_count = index + 1;
    g_intern_strings[index] = owned_string;
}
