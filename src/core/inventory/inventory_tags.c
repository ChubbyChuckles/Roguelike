#include "inventory_tags.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct InvTagRec
{
    unsigned flags;
    unsigned char tag_count;
    char tags[ROGUE_INV_TAG_MAX_TAGS_PER_DEF][ROGUE_INV_TAG_SHORT_LEN];
} InvTagRec;
static InvTagRec* g_tag_table = NULL; /* allocated on demand size ROGUE_INV_TAG_MAX_DEFS */

int rogue_inv_tags_init(void)
{
    if (!g_tag_table)
    {
        g_tag_table = (InvTagRec*) calloc(ROGUE_INV_TAG_MAX_DEFS, sizeof(InvTagRec));
        if (!g_tag_table)
            return -1;
    }
    else
    {
        memset(g_tag_table, 0, sizeof(InvTagRec) * ROGUE_INV_TAG_MAX_DEFS);
    }
    return 0;
}
static int valid_def(int d) { return d >= 0 && d < ROGUE_INV_TAG_MAX_DEFS; }
int rogue_inv_tags_set_flags(int def_index, unsigned flags)
{
    if (!valid_def(def_index) || !g_tag_table)
        return -1;
    g_tag_table[def_index].flags = flags;
    return 0;
}
unsigned rogue_inv_tags_get_flags(int def_index)
{
    if (!valid_def(def_index) || !g_tag_table)
        return 0;
    return g_tag_table[def_index].flags;
}
static int find_tag(int def_index, const char* tag)
{
    if (!valid_def(def_index) || !g_tag_table || !tag)
        return -1;
    InvTagRec* r = &g_tag_table[def_index];
    for (int i = 0; i < r->tag_count; i++)
    {
        if (strncmp(r->tags[i], tag, ROGUE_INV_TAG_SHORT_LEN) == 0)
            return i;
    }
    return -1;
}
int rogue_inv_tags_add_tag(int def_index, const char* tag)
{
    if (!valid_def(def_index) || !g_tag_table || !tag || !*tag)
        return -1;
    InvTagRec* r = &g_tag_table[def_index];
    if (r->tag_count >= ROGUE_INV_TAG_MAX_TAGS_PER_DEF)
        return -1;
    if (find_tag(def_index, tag) >= 0)
        return 0;
    size_t len = strlen(tag);
    if (len >= ROGUE_INV_TAG_SHORT_LEN)
        len = ROGUE_INV_TAG_SHORT_LEN - 1;
    memset(r->tags[r->tag_count], 0, ROGUE_INV_TAG_SHORT_LEN);
    memcpy(r->tags[r->tag_count], tag, len);
    r->tag_count++;
    return 0;
}
int rogue_inv_tags_remove_tag(int def_index, const char* tag)
{
    int idx = find_tag(def_index, tag);
    if (idx < 0)
        return -1;
    InvTagRec* r = &g_tag_table[def_index];
    int last = r->tag_count - 1;
    if (idx != last)
    {
        memcpy(r->tags[idx], r->tags[last], ROGUE_INV_TAG_SHORT_LEN);
    }
    r->tag_count--;
    return 0;
}
int rogue_inv_tags_list(int def_index, const char** out_tags, int cap)
{
    if (!valid_def(def_index) || !g_tag_table || !out_tags || cap <= 0)
        return 0;
    InvTagRec* r = &g_tag_table[def_index];
    int n = r->tag_count;
    if (n > cap)
        n = cap;
    for (int i = 0; i < n; i++)
    {
        out_tags[i] = r->tags[i];
    }
    return r->tag_count;
}
int rogue_inv_tags_has(int def_index, const char* tag) { return find_tag(def_index, tag) >= 0; }
int rogue_inv_tags_can_salvage(int def_index)
{
    unsigned f = rogue_inv_tags_get_flags(def_index);
    if (f & (ROGUE_INV_FLAG_LOCKED | ROGUE_INV_FLAG_FAVORITE))
        return 0;
    return 1;
}
int rogue_inv_tags_serialize(FILE* f)
{
    if (!f || !g_tag_table)
        return -1;
    for (int i = 0; i < ROGUE_INV_TAG_MAX_DEFS; i++)
    {
        InvTagRec* r = &g_tag_table[i];
        if (r->flags || r->tag_count)
        {
            fprintf(f, "IT%d=%u", i, r->flags);
            for (int t = 0; t < r->tag_count; t++)
            {
                fprintf(f, ",%s", r->tags[t]);
            }
            fputc('\n', f);
        }
    }
    return 0;
}
