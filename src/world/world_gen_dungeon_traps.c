#include "world_gen_dungeon_traps.h"
#include <stdlib.h>
#include <string.h>

/* local */
static void set_err(char* err, size_t cap, const char* msg)
{
    if (err && cap)
    {
#ifdef _MSC_VER
        strncpy_s(err, cap, msg, _TRUNCATE);
#else
        strncpy(err, msg, cap - 1);
        err[cap - 1] = '\0';
#endif
    }
}

static int parse_int_after(const char* s, const char* key, int* out)
{
    const char* p = strstr(s, key);
    if (!p)
        return 0;
    *out = (int) strtol(p + strlen(key), NULL, 10);
    return 1;
}

static int parse_string_field(const char* s, const char* key, char* out, size_t cap)
{
    /* Find the key occurrence, then locate the ':' and the quoted string after it */
    const char* p = strstr(s, key);
    if (!p)
        return 0;
    /* Find colon following the key */
    const char* colon = strchr(p, ':');
    if (!colon)
        return 0;
    /* Skip whitespace after ':' */
    const char* cur = colon + 1;
    while (*cur == ' ' || *cur == '\t' || *cur == '\n' || *cur == '\r')
        ++cur;
    if (*cur != '"')
        return 0;
    const char* start = cur + 1;
    const char* end = strchr(start, '"');
    if (!end)
        return 0;
    size_t n = (size_t) (end - start);
    if (n >= cap)
        n = cap - 1;
    memcpy(out, start, n);
    out[n] = '\0';
    return 1;
}

int rogue_trap_def_load_json_text(const char* json_text, RogueTrapDef* out, char* err,
                                  size_t err_cap)
{
    if (!json_text || !out)
    {
        set_err(err, err_cap, "invalid args");
        return 0;
    }
    memset(out, 0, sizeof *out);
    if (!parse_string_field(json_text, "\"id\"", out->id, sizeof out->id))
    {
        set_err(err, err_cap, "missing id");
        return 0;
    }
    /* trigger: string enum */
    char trig[24] = {0};
    if (parse_string_field(json_text, "\"trigger\"", trig, sizeof trig))
    {
        if (strcmp(trig, "pressure") == 0)
            out->trigger = ROGUE_TRAP_TRIGGER_PRESSURE_PLATE;
        else if (strcmp(trig, "proximity") == 0)
            out->trigger = ROGUE_TRAP_TRIGGER_PROXIMITY;
        else if (strcmp(trig, "timed") == 0)
            out->trigger = ROGUE_TRAP_TRIGGER_TIMED;
        else
            out->trigger = ROGUE_TRAP_TRIGGER_PRESSURE_PLATE;
    }
    else
    {
        out->trigger = ROGUE_TRAP_TRIGGER_PRESSURE_PLATE;
    }
    (void) parse_int_after(json_text, "telegraph_ms\":", &out->telegraph_ms);
    (void) parse_int_after(json_text, "damage_base\":", &out->damage_base);
    (void) parse_int_after(json_text, "cooldown_ms\":", &out->cooldown_ms);
    (void) parse_int_after(json_text, "disarm_diff\":", &out->disarm_diff);
    return 1;
}

int rogue_trap_compute_damage(const RogueTrapDef* def, int delta_level, int player_avoid)
{
    if (!def)
        return 0;
    if (player_avoid < 0)
        player_avoid = 0;
    if (player_avoid > 100)
        player_avoid = 100;
    /* Scale base damage by (1 + 0.08*ΔL), clamp ΔL>=0 for safety */
    if (delta_level < 0)
        delta_level = 0;
    double scale = 1.0 + 0.08 * (double) delta_level;
    double dmg = (double) def->damage_base * scale;
    /* avoidance reduces damage linearly up to 60% max */
    double avoid_factor = 1.0 - (0.6 * ((double) player_avoid / 100.0));
    if (avoid_factor < 0.2)
        avoid_factor = 0.2; /* leave some chip */
    dmg *= avoid_factor;
    if (dmg < 0)
        dmg = 0;
    int idmg = (int) (dmg + 0.5);
    return idmg;
}

int rogue_trap_resolve_overlap(RogueTileMap* io_map, int max_density)
{
    if (!io_map || max_density < 0)
        return 0;
    /* Iterate, removing one trap from any 3x3 window that exceeds the cap, until stable. */
    static const int prio_dx[9] = {0, 0, 1, 0, -1, -1, 1, 1, -1};
    static const int prio_dy[9] = {0, -1, 0, 1, 0, -1, -1, 1, 1};
    int removed_total = 0;
    int changed = 1;
    int pass_guard = io_map->width * io_map->height; /* hard cap to avoid pathological loops */
    while (changed && pass_guard-- > 0)
    {
        changed = 0;
        for (int y = 1; y < io_map->height - 1; ++y)
        {
            for (int x = 1; x < io_map->width - 1; ++x)
            {
                int count = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        if (io_map->tiles[(y + dy) * io_map->width + (x + dx)] ==
                            ROGUE_TILE_DUNGEON_TRAP)
                            count++;
                    }
                if (count > max_density)
                {
                    /* Remove one trap from this window using a deterministic priority. */
                    for (int k = 0; k < 9; ++k)
                    {
                        int rx = x + prio_dx[k];
                        int ry = y + prio_dy[k];
                        int idx = ry * io_map->width + rx;
                        if (io_map->tiles[idx] == ROGUE_TILE_DUNGEON_TRAP)
                        {
                            io_map->tiles[idx] = ROGUE_TILE_DUNGEON_FLOOR;
                            removed_total++;
                            changed = 1;
                            break;
                        }
                    }
                }
            }
        }
    }
    /* Final enforcement: for any remaining over-cap window, remove exactly the excess now. */
    for (int y = 1; y < io_map->height - 1; ++y)
    {
        for (int x = 1; x < io_map->width - 1; ++x)
        {
            int count = 0;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                    if (io_map->tiles[(y + dy) * io_map->width + (x + dx)] ==
                        ROGUE_TILE_DUNGEON_TRAP)
                        count++;
            if (count > max_density)
            {
                int excess = count - max_density;
                for (int k = 0; k < 9 && excess > 0; ++k)
                {
                    int rx = x + prio_dx[k];
                    int ry = y + prio_dy[k];
                    int idx = ry * io_map->width + rx;
                    if (io_map->tiles[idx] == ROGUE_TILE_DUNGEON_TRAP)
                    {
                        io_map->tiles[idx] = ROGUE_TILE_DUNGEON_FLOOR;
                        removed_total++;
                        excess--;
                    }
                }
            }
        }
    }
    return removed_total;
}

int rogue_trap_disarm_success(RogueWorldGenContext* ctx, const RogueTrapDef* def,
                              int player_disarm_skill)
{
    if (!ctx || !def)
        return 0;
    if (player_disarm_skill < 0)
        player_disarm_skill = 0;
    if (player_disarm_skill > 100)
        player_disarm_skill = 100;
    /* success probability grows with (skill - difficulty), with a soft curve */
    int diff = def->disarm_diff;
    if (diff < 0)
        diff = 0;
    if (diff > 100)
        diff = 100;
    int delta = player_disarm_skill - diff; /* -100..100 */
    /* map delta to [0.1 .. 0.95] using a sigmoid-like piecewise linear */
    double p;
    if (delta <= -50)
        p = 0.10;
    else if (delta >= 50)
        p = 0.95;
    else
        p = 0.10 + (delta + 50) * (0.85 / 100.0);
    /* sample deterministic micro RNG */
    double r = rogue_worldgen_rand_norm(&ctx->micro_rng);
    if (r < p)
        return 1;
    return 0;
}
