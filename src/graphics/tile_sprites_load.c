/* Loading & initialization for tile sprites */
#include "graphics/tile_sprites_internal.h"
#include "util/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct RogueTileSpritesGlobal g_tile_sprites;

void rogue_tile_bucket_add_variant(TileBucket* b, const char* path, int col, int row)
{
    if (b->count == b->cap)
    {
        int ncap = b->cap ? b->cap * 2 : 2;
        TileVariant* nv = (TileVariant*) realloc(b->variants, ncap * sizeof(TileVariant));
        if (!nv)
            return;
        b->variants = nv;
        b->cap = ncap;
    }
    TileVariant* v = &b->variants[b->count++];
    memset(v, 0, sizeof *v);
#if defined(_MSC_VER)
    strncpy_s(v->path, sizeof v->path, path, _TRUNCATE);
#else
    strncpy(v->path, path, sizeof v->path - 1);
    v->path[sizeof v->path - 1] = '\0';
#endif
    v->col = col;
    v->row = row;
}

void rogue_tile_normalize_path(char* p)
{
    while (*p)
    {
        if (*p == '\\')
            *p = '/';
        p++;
    }
}

bool rogue_tile_sprites_init(int tile_size)
{
    memset(&g_tile_sprites, 0, sizeof g_tile_sprites);
    if (tile_size <= 0)
        tile_size = 64;
    g_tile_sprites.tile_size = tile_size;
    g_tile_sprites.initialized = 1;
    return true;
}

static RogueTileType name_to_type(const char* name)
{
    if (!name)
        return ROGUE_TILE_EMPTY;
    struct
    {
        const char* n;
        RogueTileType t;
    } map[] = {{"EMPTY", ROGUE_TILE_EMPTY},
               {"WATER", ROGUE_TILE_WATER},
               {"GRASS", ROGUE_TILE_GRASS},
               {"FOREST", ROGUE_TILE_FOREST},
               {"MOUNTAIN", ROGUE_TILE_MOUNTAIN},
               {"CAVE_WALL", ROGUE_TILE_CAVE_WALL},
               {"CAVE_FLOOR", ROGUE_TILE_CAVE_FLOOR},
               {"RIVER", ROGUE_TILE_RIVER},
               {"SWAMP", ROGUE_TILE_SWAMP},
               {"SNOW", ROGUE_TILE_SNOW},
               {"RIVER_DELTA", ROGUE_TILE_RIVER_DELTA},
               {"RIVER_WIDE", ROGUE_TILE_RIVER_WIDE}};
    for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++)
        if (strcmp(map[i].n, name) == 0)
            return map[i].t;
    return ROGUE_TILE_EMPTY;
}

void rogue_tile_sprite_define(RogueTileType type, const char* path, int col, int row)
{
    if (!g_tile_sprites.initialized || type < 0 || type >= ROGUE_TILE_MAX || !path)
        return;
    rogue_tile_bucket_add_variant(&g_tile_sprites.buckets[type], path, col, row);
}

bool rogue_tile_sprites_load_config(const char* path)
{
    if (!g_tile_sprites.initialized)
        return false;
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, path, "rb");
#else
    f = fopen(path, "rb");
#endif
    if (!f)
    {
        const char* prefixes[] = {"../", "../../", "../../../"};
        char attempt[512];
        for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]) && !f; i++)
        {
            snprintf(attempt, sizeof attempt, "%s%s", prefixes[i], path);
#if defined(_MSC_VER)
            fopen_s(&f, attempt, "rb");
#else
            f = fopen(attempt, "rb");
#endif
            if (f)
            {
                ROGUE_LOG_INFO("Opened tile config via fallback path: %s", attempt);
                break;
            }
        }
    }
    if (!f)
    {
        ROGUE_LOG_WARN("tile config open failed: %s", path);
        return false;
    }
    char line[1024];
    int lineno = 0;
    int added = 0;
    while (fgets(line, sizeof line, f))
    {
        lineno++;
        char* segment = line;
        while (1)
        {
            while (*segment == ' ' || *segment == '\t')
                segment++;
            if (*segment == '#' || *segment == '\n' || *segment == '\0')
                break;
            if (strncmp(segment, "TILE", 4) != 0)
                break;
            segment += 4;
            while (*segment == ' ' || *segment == '\t')
                segment++;
            if (*segment == ',')
                segment++;
            while (*segment == ' ' || *segment == '\t')
                segment++;
            char name_tok[64];
            int name_len = 0;
            while (segment[name_len] && segment[name_len] != ',' && segment[name_len] != '\n' &&
                   name_len < 63)
                name_len++;
            if (segment[name_len] != ',')
            {
                break;
            }
            memcpy(name_tok, segment, name_len);
            name_tok[name_len] = '\0';
            segment += name_len + 1;
            while (*segment == ' ' || *segment == '\t')
                segment++;
            char sheet_path[256];
            int path_len = 0;
            while (segment[path_len] && segment[path_len] != ',' && segment[path_len] != '\n' &&
                   path_len < 255)
                path_len++;
            if (segment[path_len] != ',')
            {
                break;
            }
            memcpy(sheet_path, segment, path_len);
            sheet_path[path_len] = '\0';
            segment += path_len + 1;
            while (*segment == ' ' || *segment == '\t')
                segment++;
            int col = atoi(segment);
            while (*segment && *segment != ',' && *segment != '\n')
                segment++;
            if (*segment != ',')
                break;
            segment++;
            while (*segment == ' ' || *segment == '\t')
                segment++;
            int row = atoi(segment);
            while (*segment && *segment != ' ' && *segment != '\t' && *segment != '\n')
                segment++;
            rogue_tile_normalize_path(sheet_path);
            if (name_len > 0 && path_len > 0)
            {
                RogueTileType t = name_to_type(name_tok);
                rogue_tile_sprite_define(t, sheet_path, col, row);
                added++;
            }
            char* next = strstr(segment, "TILE");
            if (!next)
                break;
            else
                segment = next;
        }
    }
    fclose(f);
    ROGUE_LOG_INFO("tile config loaded %d variants", added);
    return added > 0;
}

bool rogue_tile_sprites_finalize(void)
{
    if (g_tile_sprites.finalized)
        return true;
    int loaded_any = 0;
    int failed = 0;
    for (int t = 0; t < ROGUE_TILE_MAX; t++)
    {
        TileBucket* b = &g_tile_sprites.buckets[t];
        for (int i = 0; i < b->count; i++)
        {
            TileVariant* v = &b->variants[i];
            if (!v->loaded)
            {
                if (!rogue_texture_load(&v->texture, v->path))
                {
                    ROGUE_LOG_WARN("tile texture load fail: %s (tile=%d variant=%d)", v->path, t,
                                   i);
                    failed++;
                    continue;
                }
                v->sprite.tex = &v->texture;
                v->sprite.sx = v->col * g_tile_sprites.tile_size;
                v->sprite.sy = v->row * g_tile_sprites.tile_size;
                v->sprite.sw = g_tile_sprites.tile_size;
                v->sprite.sh = g_tile_sprites.tile_size;
                v->loaded = 1;
                loaded_any++;
            }
        }
    }
    if (loaded_any > 0 && failed > 0)
    {
        ROGUE_LOG_INFO("Tile sprites finalize: %d variants loaded, %d failed (partial success)",
                       loaded_any, failed);
    }
    else if (loaded_any > 0)
    {
        ROGUE_LOG_INFO("Tile sprites finalize: %d variants loaded (all successful)", loaded_any);
    }
    else
    {
        ROGUE_LOG_WARN("Tile sprites finalize: all %d variants failed to load", failed);
    }
    g_tile_sprites.finalized = 1;
    return loaded_any > 0;
}

void rogue_tile_sprites_shutdown(void)
{
    for (int t = 0; t < ROGUE_TILE_MAX; t++)
    {
        TileBucket* b = &g_tile_sprites.buckets[t];
        for (int i = 0; i < b->count; i++)
            if (b->variants[i].loaded)
            {
                rogue_texture_destroy(&b->variants[i].texture);
                b->variants[i].loaded = 0;
            }
        free(b->variants);
        b->variants = NULL;
        b->count = b->cap = 0;
    }
    memset(&g_tile_sprites, 0, sizeof g_tile_sprites);
}
