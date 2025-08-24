/* Definition loading & lifecycle (init/shutdown) for vegetation system */
#include "../../util/log.h"
#include "vegetation_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

RogueVegetationDef g_defs[ROGUE_MAX_VEG_DEFS];
int g_def_count = 0;
RogueVegetationInstance g_instances[ROGUE_MAX_VEG_INSTANCES];
int g_instance_count = 0;
int g_trunk_collision_enabled = 1;      /* default on */
int g_canopy_tile_blocking_enabled = 1; /* default on */
float g_target_tree_cover = 0.12f;      /* fraction of grass tiles covered by tree canopy */
unsigned int g_last_seed = 0;
unsigned int vrng_state = 1u; /* rng state (xorshift) */

void rogue_vegetation_init(void)
{
    g_def_count = 0;
    g_instance_count = 0;
}
void rogue_vegetation_clear_instances(void) { g_instance_count = 0; }
void rogue_vegetation_shutdown(void)
{
    g_def_count = 0;
    g_instance_count = 0;
}

/* Internal registration helper used by JSON ingestion to append a definition directly.
   Returns 1 on success, 0 on failure (e.g., capacity reached or null input). */
int rogue__vegetation_register_def(const RogueVegetationDef* def)
{
    if (!def)
        return 0;
    if (g_def_count >= ROGUE_MAX_VEG_DEFS)
        return 0;
    g_defs[g_def_count] = *def;
    g_def_count++;
    return 1;
}

static int parse_line(char* line, int is_tree)
{
    char* context = NULL;
    char* tok = strtok_s(line, ",\r\n", &context);
    if (!tok)
        return 0;
    if ((is_tree && strcmp(tok, "TREE") != 0) || (!is_tree && strcmp(tok, "PLANT") != 0))
        return 0;
    if (g_def_count >= ROGUE_MAX_VEG_DEFS)
        return 0;
    RogueVegetationDef* d = &g_defs[g_def_count];
    memset(d, 0, sizeof *d);
    d->is_tree = (unsigned char) is_tree;
    tok = strtok_s(NULL, ",\r\n", &context);
    if (!tok)
        return 0;
    strncpy_s(d->id, sizeof d->id, tok, _TRUNCATE);
    tok = strtok_s(NULL, ",\r\n", &context);
    if (!tok)
        return 0;
    strncpy_s(d->image, sizeof d->image, tok, _TRUNCATE);
    if (strncmp(d->image, "../assets/", 10) == 0)
    {
        size_t len = strlen(d->image);
        memmove(d->image, d->image + 3, len - 2);
    }
    tok = strtok_s(NULL, ",\r\n", &context);
    if (!tok)
        return 0;
    d->tile_x = (unsigned short) atoi(tok);
    tok = strtok_s(NULL, ",\r\n", &context);
    if (!tok)
        return 0;
    d->tile_y = (unsigned short) atoi(tok);
    char* save_ptr = context;
    char* look = strtok_s(NULL, ",\r\n", &context);
    if (!look)
        return 0;
    int have_rect = 0;
    int v1 = atoi(look);
    char* look2_ctx = context;
    char* look2 = strtok_s(NULL, ",\r\n", &look2_ctx);
    if (look2)
    {
        int v2 = atoi(look2);
        char* look3_ctx = look2_ctx;
        char* look3 = strtok_s(NULL, ",\r\n", &look3_ctx);
        if (look3)
        {
            d->tile_x2 = (unsigned short) v1;
            d->tile_y2 = (unsigned short) v2;
            d->rarity = (unsigned short) atoi(look3);
            context = look3_ctx;
            have_rect = 1;
        }
    }
    if (!have_rect)
    {
        d->tile_x2 = d->tile_x;
        d->tile_y2 = d->tile_y;
        d->rarity = (unsigned short) v1;
        context = save_ptr;
    }
    if (d->rarity == 0)
        d->rarity = 1;
    if (is_tree)
    {
        char* cr = strtok_s(NULL, ",\r\n", &context);
        if (!cr)
            return 0;
        d->canopy_radius = (unsigned char) atoi(cr);
        if (d->canopy_radius == 0)
            d->canopy_radius = 1;
    }
    else
        d->canopy_radius = 0;
    if (!have_rect)
    {
        d->tile_x2 = d->tile_x;
        d->tile_y2 = d->tile_y;
    }
    g_def_count++;
    return 1;
}

static int open_with_fallback(const char* base, FILE** out)
{
    const char* variants[3] = {base, NULL, NULL};
    static char buf1[256];
    static char buf2[256];
    snprintf(buf1, sizeof buf1, "../%s", base);
    variants[1] = buf1;
    snprintf(buf2, sizeof buf2, "../../%s", base);
    variants[2] = buf2;
    for (int i = 0; i < 3; i++)
    {
        FILE* f = NULL;
        fopen_s(&f, variants[i], "r");
        if (f)
        {
            *out = f;
            return i;
        }
    }
    return -1;
}

int rogue_vegetation_load_defs(const char* plants_cfg, const char* trees_cfg)
{
    FILE* f = NULL;
    (void) open_with_fallback(plants_cfg, &f);
    if (f)
    {
        char line[512];
        while (fgets(line, sizeof line, f))
        {
            if (line[0] == '#' || line[0] == '\n')
                continue;
            char tmp[512];
            strncpy_s(tmp, sizeof tmp, line, _TRUNCATE);
            parse_line(tmp, 0);
        }
        fclose(f);
    }
    else
    {
        ROGUE_LOG_WARN("plants.cfg not found (tried %s, ../%s, ../../%s)", plants_cfg, plants_cfg,
                       plants_cfg);
    }
    f = NULL;
    (void) open_with_fallback(trees_cfg, &f);
    if (f)
    {
        char line[512];
        while (fgets(line, sizeof line, f))
        {
            if (line[0] == '#' || line[0] == '\n')
                continue;
            char tmp[512];
            strncpy_s(tmp, sizeof tmp, line, _TRUNCATE);
            parse_line(tmp, 1);
        }
        fclose(f);
    }
    else
    {
        ROGUE_LOG_WARN("trees.cfg not found (tried %s, ../%s, ../../%s)", trees_cfg, trees_cfg,
                       trees_cfg);
    }
    ROGUE_LOG_INFO("Vegetation defs loaded: %d", g_def_count);
    return g_def_count;
}
