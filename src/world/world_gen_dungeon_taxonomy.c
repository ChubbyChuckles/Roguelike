#include "world_gen.h" /* brings in RogueBiomeId and includes taxonomy header */
#include <stdio.h>     /* FILE, fopen, fputs, fprintf, fclose */
#include <string.h>
#if defined(_WIN32)
#include <direct.h> /* _mkdir */
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

const char* rogue_dungeon_archetype_name(RogueDungeonArchetype a)
{
    switch (a)
    {
    case ROGUE_DUNGEON_ARCH_LINEAR:
        return "Linear";
    case ROGUE_DUNGEON_ARCH_BRANCHING:
        return "Branching";
    case ROGUE_DUNGEON_ARCH_LOOPING:
        return "Looping";
    case ROGUE_DUNGEON_ARCH_HUB:
        return "Hub";
    case ROGUE_DUNGEON_ARCH_GAUNTLET:
        return "Gauntlet";
    case ROGUE_DUNGEON_ARCH_PUZZLE:
        return "Puzzle";
    case ROGUE_DUNGEON_ARCH_ARENA:
        return "Arena";
    default:
        return "UNKNOWN";
    }
}

int rogue_dungeon_is_valid_template_id(int id)
{
    return (id >= ROGUE_DUNGEON_TEMPLATE_ID_BASE && id <= ROGUE_DUNGEON_TEMPLATE_ID_MAX);
}

int rogue_dungeon_is_valid_event_id(int id)
{
    return (id >= ROGUE_DUNGEON_EVENT_ID_BASE && id <= ROGUE_DUNGEON_EVENT_ID_MAX);
}

int rogue_dungeon_is_valid_objective_id(int id)
{
    return (id >= ROGUE_DUNGEON_OBJECTIVE_ID_BASE && id <= ROGUE_DUNGEON_OBJECTIVE_ID_MAX);
}

/* Simple monotonic, saturating mapping for early phases: every 3 depths adds +1 until +8. */
int rogue_dungeon_target_level_delta(int depth)
{
    if (depth <= 0)
        return 0;
    int delta = depth / 3; /* 0,0,0,1,1,1,2,2,2, ... */
    if (delta > 8)
        delta = 8;
    return delta;
}

/* Export depth -> target level delta table as JSON for analytics. */
static void rogue__mkdir_p(const char* path)
{
    if (!path || !*path)
        return;
    /* Make a mutable copy and create each segment */
    char buf[512];
#if defined(_MSC_VER)
    strncpy_s(buf, sizeof buf, path, _TRUNCATE);
#else
    strncpy(buf, path, sizeof buf - 1);
    buf[sizeof buf - 1] = '\0';
#endif
    size_t len = strlen(buf);
    /* Replace final filename with terminator at last sep to get parent dir */
    size_t last_sep = 0;
    for (size_t i = 0; i < len; ++i)
    {
        if (buf[i] == '/' || buf[i] == '\\')
            last_sep = i;
    }
    if (last_sep == 0)
        return; /* no directory component */
    buf[last_sep] = '\0';
    /* Iterate and create */
    for (size_t i = 1; i <= last_sep; ++i)
    {
        if (buf[i] == '/' || buf[i] == '\\' || buf[i] == '\0')
        {
            char save = buf[i];
            buf[i] = '\0';
            if (buf[0] != '\0')
            {
#if defined(_WIN32)
                _mkdir(buf);
#else
                mkdir(buf, 0755);
#endif
            }
            buf[i] = save;
        }
    }
}

int rogue_dungeon_export_depth_profile(const char* path, int max_depth)
{
    if (!path || max_depth <= 0)
        return 0;
    /* Ensure parent directories exist for the given path */
    rogue__mkdir_p(path);
    FILE* f = NULL;
#if defined(_MSC_VER)
    if (fopen_s(&f, path, "wb") != 0 || !f)
        return 0;
#else
    f = fopen(path, "wb");
    if (!f)
        return 0;
#endif
    fputs("{\n  \"depth_target_level_table\": [\n", f);
    for (int d = 0; d <= max_depth; ++d)
    {
        int dl = rogue_dungeon_target_level_delta(d);
        if (fprintf(f, "    {\"depth\": %d, \"delta\": %d}%s\n", d, dl,
                    (d < max_depth ? "," : "")) < 0)
        {
            fclose(f);
            return 0;
        }
    }
    fputs("  ]\n}\n", f);
    fclose(f);
    return 1;
}

/* Export a simple gating manifest summarizing which capability tags are required/used by the
 * generated dungeon graph. For the current deterministic slice, we approximate capabilities from
 * room thematic tags: PUZZLE -> capability "puzzle", TREASURE -> capability "timed_door",
 * SECRET rooms imply capability "secret_passage". This is intentionally minimal and will be
 * extended when puzzle templates carry explicit min_skill_req/capability tags. */
int rogue_dungeon_export_gating_manifest(const char* path, const struct RogueDungeonGraph* graph)
{
    if (!path || !graph || graph->room_count <= 0 || !graph->rooms)
        return 0;
    rogue__mkdir_p(path);
    FILE* f = NULL;
#if defined(_MSC_VER)
    if (fopen_s(&f, path, "wb") != 0 || !f)
        return 0;
#else
    f = fopen(path, "wb");
    if (!f)
        return 0;
#endif
    /* Compute booleans and counts deterministically */
    int has_puzzle = 0, has_timed = 0, has_secret = 0;
    int count_puzzle = 0, count_timed = 0, count_secret = 0;
    for (int i = 0; i < graph->room_count; ++i)
    {
        const struct RogueDungeonRoom* r = &graph->rooms[i];
        if (!r)
            continue;
        if (r->tag & ROGUE_DUNGEON_ROOM_PUZZLE)
        {
            has_puzzle = 1;
            ++count_puzzle;
        }
        if (r->tag & ROGUE_DUNGEON_ROOM_TREASURE)
        {
            has_timed = 1; /* treasure rooms currently imply timed door traversal marker */
            ++count_timed;
        }
        if (r->secret)
        {
            has_secret = 1;
            ++count_secret;
        }
    }
    /* Emit compact JSON with deterministic key order */
    fprintf(f, "{\n");
    fprintf(f, "  \"capabilities\": [");
    int first = 1;
    if (has_puzzle)
    {
        fprintf(f, "%s{\"id\": \"puzzle\", \"count\": %d}", first ? "" : ", ", count_puzzle);
        first = 0;
    }
    if (has_timed)
    {
        fprintf(f, "%s{\"id\": \"timed_door\", \"count\": %d}", first ? "" : ", ", count_timed);
        first = 0;
    }
    if (has_secret)
    {
        fprintf(f, "%s{\"id\": \"secret_passage\", \"count\": %d}", first ? "" : ", ",
                count_secret);
        first = 0;
    }
    fprintf(f, "],\n");
    fprintf(f, "  \"rooms\": %d\n", graph->room_count);
    fprintf(f, "}\n");
    fclose(f);
    return 1;
}

unsigned int rogue_dungeon_biome_theme_tags(enum RogueBiomeId biome)
{
    switch (biome)
    {
    case ROGUE_BIOME_OCEAN:
        return ROGUE_DUNGEON_THEME_DAMP;
    case ROGUE_BIOME_PLAINS:
        return ROGUE_DUNGEON_THEME_RUIN;
    case ROGUE_BIOME_FOREST_BIOME:
        return ROGUE_DUNGEON_THEME_FORESTED;
    case ROGUE_BIOME_MOUNTAIN_BIOME:
        return ROGUE_DUNGEON_THEME_MOUNTAIN;
    case ROGUE_BIOME_SNOW_BIOME:
        return ROGUE_DUNGEON_THEME_ICY;
    case ROGUE_BIOME_SWAMP_BIOME:
        return ROGUE_DUNGEON_THEME_SWAMP | ROGUE_DUNGEON_THEME_DAMP;
    default:
        return ROGUE_DUNGEON_THEME_DEFAULT;
    }
}
