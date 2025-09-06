/**
 * @file world_gen_dungeon_traps.c
 * @brief Dungeon trap generation and management utilities for the roguelike game.
 *
 * This module provides functionality for loading trap definitions from JSON text, computing trap
 * damage based on level differences and player avoidance, resolving trap overlaps in tile maps to
 * enforce density limits, and determining the success probability of disarming traps using player
 * skill. Trap definitions include fields like ID, trigger type (pressure, proximity, timed),
 * telegraph time, base damage, cooldown, and disarm difficulty. Damage scaling uses a linear factor
 * with avoidance reduction. Overlap resolution employs an iterative thinning algorithm with
 * deterministic priority ordering. Disarm success uses a piecewise linear approximation of a
 * sigmoid curve for probability.
 *
 * @author [Developer Name or Auto-generated]
 * @date [Current Date or Build Time]
 * @version 1.0
 */

#include "world_gen_dungeon_traps.h"
#include <stdlib.h>
#include <string.h>

/* local */
/**
 * @brief Safely sets an error message in a character buffer, handling platform-specific string
 * copying.
 *
 * Copies the message into the error buffer, truncating if necessary, and ensures null-termination.
 * This helper is used for error reporting in parsing functions.
 * @param err Pointer to the destination error buffer.
 * @param cap Capacity of the error buffer (including space for null terminator).
 * @param msg The source error message to copy.
 * @note On Microsoft Visual C++ (_MSC_VER), uses strncpy_s with _TRUNCATE flag.
 *       On other platforms, uses strncpy and manually sets the last byte to null terminator.
 * @warning Buffer overflow is prevented, but truncation may occur if msg is longer than cap-1.
 */
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

/**
 * @brief Parses an integer value that appears immediately after a specified key in a string.
 *
 * Searches for the key using strstr, then uses strtol to extract and convert the following
 * characters to an integer. This is a simple JSON-like field extractor for numeric values after a
 * key.
 * @param s The input string to search within (e.g., JSON-like text).
 * @param key The key substring to locate (e.g., "damage_base\":").
 * @param out Pointer to the integer where the parsed value will be stored.
 * @return 1 if the key was found and parsing succeeded (out is set), 0 otherwise (key not found or
 * invalid number).
 * @note strtol may fail silently if no digits follow; assumes well-formed input after key.
 * @warning Does not skip whitespace or validate JSON structure; assumes immediate numeric after
 * key.
 */
static int parse_int_after(const char* s, const char* key, int* out)
{
    const char* p = strstr(s, key);
    if (!p)
        return 0;
    *out = (int) strtol(p + strlen(key), NULL, 10);
    return 1;
}

/**
 * @brief Parses a quoted string value associated with a key in a string (e.g., JSON field
 * extraction).
 *
 * Locates the key, finds the following colon ':', skips whitespace, expects a double-quoted string,
 * extracts the content between the quotes, and copies it to the output buffer with truncation if
 * needed. This is used for extracting string fields like "id" or "trigger" from JSON text.
 * @param s The input string to parse.
 * @param key The field key to search for (e.g., "\"id\"").
 * @param out Pointer to the destination buffer for the extracted string.
 * @param cap Capacity of the output buffer (including null terminator).
 * @return 1 if the key, colon, and quoted string were successfully found and extracted, 0
 * otherwise.
 * @details
 * - Finds the key occurrence using strstr.
 * - Locates the colon following the key using strchr.
 * - Skips whitespace (space, tab, newline, carriage return) after the colon.
 * - Validates the start of a quoted string ('"').
 * - Finds the end quote using strchr.
 * - Copies up to (cap - 1) characters and null-terminates the output.
 * @note Assumes simple JSON without escaped quotes inside strings.
 * @warning Truncates if extracted string exceeds cap-1; no error set for truncation.
 */
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

/**
 * @brief Loads a RogueTrapDef structure from a JSON-formatted text string.
 *
 * Parses essential fields from the JSON text and populates the output structure.
 * Requires a valid "id" string field; other fields are optional with defaults.
 * This function is used during world generation to load trap configurations from data files.
 * @param json_text The input JSON string containing the trap definition (e.g.,
 * {"id":"spike","trigger":"pressure",...}).
 * @param out Pointer to the RogueTrapDef to fill with parsed data.
 * @param err Buffer to store error messages on failure (e.g., "invalid args" or "missing id").
 * @param err_cap Maximum length of the error buffer (including null terminator).
 * @return 1 on successful parsing and population, 0 on failure (invalid args or missing id).
 * @details
 * - Validates input pointers; sets "invalid args" error if null.
 * - Zero-initializes the output structure using memset.
 * - Parses "id" as a quoted string using parse_string_field; fails with "missing id" if not found.
 * - Parses "trigger" as a quoted string enum ("pressure", "proximity", "timed"); defaults to
 * ROGUE_TRAP_TRIGGER_PRESSURE_PLATE if missing or invalid.
 * - Parses integer fields after keys: "telegraph_ms\":", "damage_base\":", "cooldown_ms\":",
 * "disarm_diff\":" using parse_int_after.
 * - Ignores parsing failures for optional fields (no error set).
 * @note Trigger enum mapping: "pressure" -> ROGUE_TRAP_TRIGGER_PRESSURE_PLATE, "proximity" ->
 * ROGUE_TRAP_TRIGGER_PROXIMITY, "timed" -> ROGUE_TRAP_TRIGGER_TIMED.
 * @warning Assumes JSON keys are quoted and values are immediate; no full JSON validation.
 */
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

/**
 * @brief Calculates the effective damage from a trap, factoring in dungeon level difference and
 * player avoidance.
 *
 * Applies scaling based on level delta and reduces damage based on avoidance percentage, with
 * safeguards. This is used during gameplay when a player triggers a trap to compute the actual
 * damage dealt.
 * @param def The trap definition containing base damage and other params (non-null required).
 * @param delta_level The level difference (player's current level minus trap's generation level;
 * positive for deeper traps).
 * @param player_avoid The player's avoidance rating from skills/gear (percentage, 0-100).
 * @return The computed integer damage value (rounded to nearest, minimum 0).
 * @details
 * - Returns 0 if def is null.
 * - Clamps player_avoid to [0, 100] to prevent invalid inputs.
 * - Clamps delta_level to >=0 for safety (no negative scaling).
 * - Scales damage: base_damage * (1 + 0.08 * delta_level) for progression.
 * - Applies avoidance factor: 1.0 - (0.6 * (avoid / 100.0)), clamped to minimum 0.2 (allows 20%
 * chip damage even at 100 avoid).
 * - Multiplies scaled damage by avoidance factor.
 * - Ensures result is non-negative, rounds using +0.5 before casting to int.
 * @note The scaling factor (0.08) provides 8% increase per level difference.
 * @warning Avoidance caps at 60% reduction to ensure traps remain threatening.
 */
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

/**
 * @brief Resolves excessive trap density in a dungeon tile map by removing overlapping traps.
 *
 * Iteratively thins traps in 3x3 windows exceeding the maximum density until stable, then performs
 * a final pass to enforce the limit exactly. Modifies the map in-place, replacing excess traps with
 * floor tiles. This is called during dungeon generation after initial trap placement to balance
 * difficulty.
 * @param io_map Pointer to the RogueTileMap to modify (in-out parameter).
 * @param max_density The maximum number of ROGUE_TILE_DUNGEON_TRAP tiles allowed in any 3x3 window
 * (e.g., 2 for sparse).
 * @return The total number of traps removed during the process.
 * @details
 * - Returns 0 if io_map is null or max_density < 0 (invalid input).
 * - Uses fixed priority arrays for deterministic removal order: prio_dx/dy define order (center
 * first, then N, E, S, W, NW, NE, SW, SE).
 * - Iterative phase: While changes occur (guarded by pass limit = map area to prevent infinite
 * loops), scan inner tiles (excluding borders), count traps in 3x3 neighborhood; if > max_density,
 * remove the highest-priority trap in that window, increment removed_total, set changed=1.
 * - Final enforcement phase: Single pass over inner tiles, recount; if over limit, remove exactly
 * (count - max_density) traps using the same priority order.
 * - Trap tiles (ROGUE_TILE_DUNGEON_TRAP) are replaced with ROGUE_TILE_DUNGEON_FLOOR.
 * - Borders (y=0 or height-1, x=0 or width-1) are not scanned to avoid edge artifacts.
 * @note The algorithm ensures stability without oscillations due to the single-removal per
 * iteration and guard.
 * @warning Modifies the map in-place; assumes tiles array is large enough (width * height).
 */
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

/**
 * @brief Determines if a player successfully disarms a trap based on skill level and trap
 * difficulty.
 *
 * Computes success probability using a piecewise linear sigmoid-like curve based on skill-diff
 * delta, then samples a normalized RNG value from the worldgen context. This is used in gameplay
 * when a player attempts to disarm a discovered trap.
 * @param ctx The RogueWorldGenContext providing the micro_rng for deterministic pseudo-randomness.
 * @param def The trap definition containing the disarm_difficulty.
 * @param player_disarm_skill The player's disarm skill rating (0-100).
 * @return 1 if disarm succeeds (RNG < probability), 0 otherwise (failure).
 * @details
 * - Returns 0 if ctx or def is null.
 * - Clamps player_disarm_skill to [0, 100].
 * - Clamps def->disarm_diff to [0, 100].
 * - Computes delta = skill - diff (-100 to 100).
 * - Maps delta to probability p in [0.1, 0.95]:
 *   - If delta <= -50, p = 0.10 (10% base failure floor).
 *   - If delta >= 50, p = 0.95 (95% success ceiling).
 *   - Else, p = 0.10 + (delta + 50) * (0.85 / 100.0) (linear ramp).
 * - Samples r = rogue_worldgen_rand_norm(&ctx->micro_rng) (assumed uniform [0,1)).
 * - Succeeds if r < p.
 * @note The micro_rng ensures reproducible randomness for the same context state.
 * @warning Probability curve is soft; full success requires ~50+ skill advantage.
 */
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
