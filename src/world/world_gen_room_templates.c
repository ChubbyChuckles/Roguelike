#include "world_gen_room_templates.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char map_char_to_tile(char c)
{
    switch (c)
    {
    case '#':
        return ROGUE_TILE_DUNGEON_WALL;
    case '.':
        return ROGUE_TILE_DUNGEON_FLOOR;
    case 'D':
        return ROGUE_TILE_DUNGEON_DOOR;
    case 'L':
        return ROGUE_TILE_DUNGEON_LOCKED_DOOR;
    case 'S':
        return ROGUE_TILE_DUNGEON_SECRET_DOOR;
    case 'T':
        return ROGUE_TILE_DUNGEON_TRAP;
    case 'K':
        return ROGUE_TILE_DUNGEON_KEY;
    default:
        return ROGUE_TILE_DUNGEON_FLOOR;
    }
}

int rogue_room_template_from_ascii(const char** rows, int h, int id, RogueRoomSizeClass cls,
                                   RogueRoomTemplate* out)
{
    if (!rows || h <= 0 || !out)
        return -1;
    int w = (int) strlen(rows[0]);
    if (w <= 0)
        return -2;
    for (int i = 1; i < h; ++i)
    {
        if ((int) strlen(rows[i]) != w)
            return -3; /* ragged rows */
    }
    size_t len = (size_t) w * (size_t) h;
    char* grid = (char*) malloc(len);
    if (!grid)
        return -4;
    int exit_count = 0;
    int door_count = 0;
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            char c = rows[y][x];
            grid[y * w + x] = c;
            if (c == 'E' && exit_count < 16)
            {
                out->exits[exit_count++] = (RogueRoomExit){x, y, -1};
            }
            else if ((c == 'D' || c == 'L' || c == 'S') && door_count < 32)
            {
                RogueDoorType tp = (c == 'D')   ? ROGUE_DOOR_NORMAL
                                   : (c == 'L') ? ROGUE_DOOR_LOCKED
                                                : ROGUE_DOOR_SECRET;
                out->doors[door_count++] = (RogueRoomDoor){x, y, tp, -1};
            }
        }
    }
    out->id = id;
    out->cls = cls;
    out->width = w;
    out->height = h;
    out->grid = grid;
    out->exit_count = exit_count;
    out->door_count = door_count;
    out->biome_tags[0] = '\0';
    out->encounter_slots = 0;
    out->hazard_slots = 0;
    out->puzzle_slot = 0;
    out->deco_count = 0;
    return 0;
}

void rogue_room_template_free(RogueRoomTemplate* t)
{
    if (!t)
        return;
    free(t->grid);
    t->grid = NULL;
    t->exit_count = 0;
    t->door_count = 0;
    t->deco_count = 0;
}

static void xform_point(int x, int y, int w, int h, int rot_deg, int reflect_x, int* ox, int* oy)
{
    /* Apply reflection first in local space */
    if (reflect_x)
        x = (w - 1) - x;
    int rx = x, ry = y;
    switch (rot_deg)
    {
    case 0:
        rx = x;
        ry = y;
        break;
    case 90:
        rx = h - 1 - y;
        ry = x;
        break;
    case 180:
        rx = w - 1 - x;
        ry = h - 1 - y;
        break;
    case 270:
        rx = y;
        ry = w - 1 - x;
        break;
    default:
        rx = x;
        ry = y;
        break;
    }
    *ox = rx;
    *oy = ry;
}

void rogue_room_template_dims_after_xform(const RogueRoomTemplate* t, int rot_deg, int reflect_x,
                                          int* out_w, int* out_h)
{
    (void) reflect_x; /* reflection doesn't change dims */
    if (!t)
    {
        if (out_w)
            *out_w = 0;
        if (out_h)
            *out_h = 0;
        return;
    }
    if (rot_deg == 90 || rot_deg == 270)
    {
        if (out_w)
            *out_w = t->height;
        if (out_h)
            *out_h = t->width;
    }
    else
    {
        if (out_w)
            *out_w = t->width;
        if (out_h)
            *out_h = t->height;
    }
}

int rogue_room_template_compute_exits(const RogueRoomTemplate* t, int rot_deg, int reflect_x,
                                      RogueRoomExit* out_exits, int max)
{
    if (!t || !out_exits || max <= 0)
        return 0;
    int tw, th;
    rogue_room_template_dims_after_xform(t, rot_deg, reflect_x, &tw, &th);
    int count = 0;
    for (int i = 0; i < t->exit_count && count < max; ++i)
    {
        int x = t->exits[i].x, y = t->exits[i].y;
        int rx, ry;
        xform_point(x, y, t->width, t->height, rot_deg, reflect_x, &rx, &ry);
        out_exits[count++] = (RogueRoomExit){rx, ry, -1};
    }
    (void) tw;
    (void) th;
    return count;
}

int rogue_room_template_compute_doors(const RogueRoomTemplate* t, int rot_deg, int reflect_x,
                                      RogueRoomDoor* out_doors, int max)
{
    if (!t || !out_doors || max <= 0)
        return 0;
    int count = 0;
    for (int i = 0; i < t->door_count && count < max; ++i)
    {
        int rx, ry;
        xform_point(t->doors[i].x, t->doors[i].y, t->width, t->height, rot_deg, reflect_x, &rx,
                    &ry);
        out_doors[count++] = (RogueRoomDoor){rx, ry, t->doors[i].type, t->doors[i].key_id};
    }
    return count;
}

int rogue_dungeon_stamp_template(RogueTileMap* io_map, int ox, int oy, const RogueRoomTemplate* t,
                                 int rot_deg, int reflect_x)
{
    if (!io_map || !io_map->tiles || !t || !t->grid)
        return 0;
    int tw, th;
    rogue_room_template_dims_after_xform(t, rot_deg, reflect_x, &tw, &th);
    int written = 0;
    for (int y = 0; y < t->height; ++y)
    {
        for (int x = 0; x < t->width; ++x)
        {
            char ch = t->grid[y * t->width + x];
            if (ch == 'E')
                ch = '.'; /* exits are also floors for stamping */
            int rx, ry;
            xform_point(x, y, t->width, t->height, rot_deg, reflect_x, &rx, &ry);
            int gx = ox + rx;
            int gy = oy + ry;
            if (gx < 0 || gy < 0 || gx >= io_map->width || gy >= io_map->height)
                continue;
            unsigned char tile = map_char_to_tile(ch);
            io_map->tiles[gy * io_map->width + gx] = tile;
            written++;
        }
    }
    (void) tw;
    (void) th;
    return written;
}

/* --- Minimal JSON loader (Phase 2.1) ---
   Expected JSON object fields (all optional except id, cls, grid):
   {
     "id": 101,
     "cls": "small|medium|large",
     "biome_tags": "crypt,undead",
     "encounter_slots": 1,
     "hazard_slots": 0,
     "puzzle_slot": 0,
     "grid": ["#####","E...D","#...#","L...S","#####"],
    "exits": [{"x":0,"y":1},{"x":4,"y":1}], (optional; if absent 'E' in grid used)
     "doors": [{"x":4,"y":1,"type":"normal|locked|secret","key_id":12}],
     "deco": [{"x":2,"y":2,"kind":"pillar"}]
   }
*/

static RogueRoomSizeClass parse_cls(const char* s)
{
    if (!s)
        return ROGUE_ROOM_SMALL;
    if (strcmp(s, "small") == 0)
        return ROGUE_ROOM_SMALL;
    if (strcmp(s, "medium") == 0)
        return ROGUE_ROOM_MEDIUM;
    if (strcmp(s, "large") == 0)
        return ROGUE_ROOM_LARGE;
    return ROGUE_ROOM_SMALL;
}

static RogueDoorType parse_door_type(const char* s)
{
    if (!s)
        return ROGUE_DOOR_NORMAL;
    if (strcmp(s, "locked") == 0)
        return ROGUE_DOOR_LOCKED;
    if (strcmp(s, "secret") == 0)
        return ROGUE_DOOR_SECRET;
    return ROGUE_DOOR_NORMAL;
}

/* local JSON helpers (very small subset) */
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

static void skip_ws(const char** ps)
{
    const char* s = *ps;
    while (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t')
        s++;
    *ps = s;
}

static int parse_json_string(const char** ps, char* out, size_t cap)
{
    const char* s = *ps;
    if (*s != '"')
        return 0;
    s++;
    size_t i = 0;
    while (*s && *s != '"')
    {
        if (i + 1 < cap)
            out[i++] = *s;
        s++;
    }
    if (*s != '"')
        return 0;
    out[i] = '\0';
    s++;
    *ps = s;
    return 1;
}

static int parse_json_number_i(const char** ps, int* out)
{
    char* e = NULL;
    long v = strtol(*ps, &e, 10);
    if (e == *ps)
        return 0;
    *out = (int) v;
    *ps = e;
    return 1;
}

static int expect(const char** ps, char c)
{
    skip_ws(ps);
    if (**ps != c)
        return 0;
    (*ps)++;
    return 1;
}

int rogue_room_template_load_json_text(const char* json_text, RogueRoomTemplate* out, char* err,
                                       size_t err_cap)
{
    if (!json_text || !out)
    {
        set_err(err, err_cap, "invalid args");
        return 0;
    }
    const char* s = json_text;
    skip_ws(&s);
    if (!expect(&s, '{'))
    {
        set_err(err, err_cap, "expected object");
        return 0;
    }
    memset(out, 0, sizeof *out);
    out->encounter_slots = out->hazard_slots = out->puzzle_slot = 0;
    while (1)
    {
        skip_ws(&s);
        if (*s == '}')
        {
            s++;
            break;
        }
        char key[32];
        if (!parse_json_string(&s, key, sizeof key))
        {
            set_err(err, err_cap, "bad key");
            return 0;
        }
        if (!expect(&s, ':'))
        {
            set_err(err, err_cap, "expected colon");
            return 0;
        }
        skip_ws(&s);
        if (strcmp(key, "id") == 0)
        {
            if (!parse_json_number_i(&s, &out->id))
            {
                set_err(err, err_cap, "bad id");
                return 0;
            }
        }
        else if (strcmp(key, "cls") == 0)
        {
            char tmp[16];
            if (!parse_json_string(&s, tmp, sizeof tmp))
                return 0;
            out->cls = parse_cls(tmp);
        }
        else if (strcmp(key, "biome_tags") == 0)
        {
            char tmp[120];
            if (!parse_json_string(&s, tmp, sizeof tmp))
                return 0;
#ifdef _MSC_VER
            strncpy_s(out->biome_tags, sizeof out->biome_tags, tmp, _TRUNCATE);
#else
            strncpy(out->biome_tags, tmp, sizeof out->biome_tags - 1);
#endif
        }
        else if (strcmp(key, "encounter_slots") == 0)
        {
            if (!parse_json_number_i(&s, &out->encounter_slots))
                return 0;
        }
        else if (strcmp(key, "hazard_slots") == 0)
        {
            if (!parse_json_number_i(&s, &out->hazard_slots))
                return 0;
        }
        else if (strcmp(key, "puzzle_slot") == 0)
        {
            if (!parse_json_number_i(&s, &out->puzzle_slot))
                return 0;
        }
        else if (strcmp(key, "grid") == 0)
        {
            if (!expect(&s, '['))
            {
                set_err(err, err_cap, "expected grid array");
                return 0;
            }
            const char* lines[128];
            int h = 0;
            char cache[128][128];
            while (1)
            {
                skip_ws(&s);
                if (*s == ']')
                {
                    s++;
                    break;
                }
                if (h >= 128)
                {
                    set_err(err, err_cap, "grid too tall");
                    return 0;
                }
                if (!parse_json_string(&s, cache[h], sizeof cache[h]))
                {
                    set_err(err, err_cap, "bad grid line");
                    return 0;
                }
                lines[h] = cache[h];
                h++;
                skip_ws(&s);
                if (*s == ',')
                    s++;
            }
            /* Preserve any fields parsed prior to grid (key order independence) */
            char saved_biome[sizeof out->biome_tags];
#ifdef _MSC_VER
            strncpy_s(saved_biome, sizeof saved_biome, out->biome_tags, _TRUNCATE);
#else
            strncpy(saved_biome, out->biome_tags, sizeof saved_biome - 1);
            saved_biome[sizeof saved_biome - 1] = '\0';
#endif
            int saved_encounter = out->encounter_slots;
            int saved_hazard = out->hazard_slots;
            int saved_puzzle = out->puzzle_slot;
            if (rogue_room_template_from_ascii(lines, h, out->id, out->cls, out) != 0)
            {
                set_err(err, err_cap, "ascii build failed");
                return 0;
            }
            /* Restore saved non-grid fields */
#ifdef _MSC_VER
            strncpy_s(out->biome_tags, sizeof out->biome_tags, saved_biome, _TRUNCATE);
#else
            strncpy(out->biome_tags, saved_biome, sizeof out->biome_tags - 1);
#endif
            out->encounter_slots = saved_encounter;
            out->hazard_slots = saved_hazard;
            out->puzzle_slot = saved_puzzle;
        }
        else if (strcmp(key, "exits") == 0)
        {
            if (!expect(&s, '['))
                return 0;
            out->exit_count = 0;
            while (1)
            {
                skip_ws(&s);
                if (*s == ']')
                {
                    s++;
                    break;
                }
                if (!expect(&s, '{'))
                    return 0;
                int x = -1, y = -1;
                while (1)
                {
                    skip_ws(&s);
                    if (*s == '}')
                    {
                        s++;
                        break;
                    }
                    char k2[8];
                    if (!parse_json_string(&s, k2, sizeof k2))
                        return 0;
                    if (!expect(&s, ':'))
                        return 0;
                    if (strcmp(k2, "x") == 0)
                    {
                        if (!parse_json_number_i(&s, &x))
                            return 0;
                    }
                    else if (strcmp(k2, "y") == 0)
                    {
                        if (!parse_json_number_i(&s, &y))
                            return 0;
                    }
                    skip_ws(&s);
                    if (*s == ',')
                        s++;
                }
                if (x >= 0 && y >= 0 && out->exit_count < 16)
                    out->exits[out->exit_count++] = (RogueRoomExit){x, y, -1};
                skip_ws(&s);
                if (*s == ',')
                    s++;
            }
        }
        else if (strcmp(key, "doors") == 0)
        {
            if (!expect(&s, '['))
                return 0;
            out->door_count = 0;
            while (1)
            {
                skip_ws(&s);
                if (*s == ']')
                {
                    s++;
                    break;
                }
                if (!expect(&s, '{'))
                    return 0;
                int x = -1, y = -1, key_id = -1;
                char tp[12] = "normal";
                while (1)
                {
                    skip_ws(&s);
                    if (*s == '}')
                    {
                        s++;
                        break;
                    }
                    char k2[8];
                    if (!parse_json_string(&s, k2, sizeof k2))
                        return 0;
                    if (!expect(&s, ':'))
                        return 0;
                    if (strcmp(k2, "x") == 0)
                    {
                        if (!parse_json_number_i(&s, &x))
                            return 0;
                    }
                    else if (strcmp(k2, "y") == 0)
                    {
                        if (!parse_json_number_i(&s, &y))
                            return 0;
                    }
                    else if (strcmp(k2, "key_id") == 0)
                    {
                        if (!parse_json_number_i(&s, &key_id))
                            return 0;
                    }
                    else if (strcmp(k2, "type") == 0)
                    {
                        if (!parse_json_string(&s, tp, sizeof tp))
                            return 0;
                    }
                    skip_ws(&s);
                    if (*s == ',')
                        s++;
                }
                if (x >= 0 && y >= 0 && out->door_count < 32)
                    out->doors[out->door_count++] =
                        (RogueRoomDoor){x, y, parse_door_type(tp), key_id};
                skip_ws(&s);
                if (*s == ',')
                    s++;
            }
        }
        else if (strcmp(key, "deco") == 0)
        {
            if (!expect(&s, '['))
                return 0;
            out->deco_count = 0;
            while (1)
            {
                skip_ws(&s);
                if (*s == ']')
                {
                    s++;
                    break;
                }
                if (!expect(&s, '{'))
                    return 0;
                int x = -1, y = -1;
                char kind[24] = "";
                while (1)
                {
                    skip_ws(&s);
                    if (*s == '}')
                    {
                        s++;
                        break;
                    }
                    char k2[8];
                    if (!parse_json_string(&s, k2, sizeof k2))
                        return 0;
                    if (!expect(&s, ':'))
                        return 0;
                    if (strcmp(k2, "x") == 0)
                    {
                        if (!parse_json_number_i(&s, &x))
                            return 0;
                    }
                    else if (strcmp(k2, "y") == 0)
                    {
                        if (!parse_json_number_i(&s, &y))
                            return 0;
                    }
                    else if (strcmp(k2, "kind") == 0)
                    {
                        if (!parse_json_string(&s, kind, sizeof kind))
                            return 0;
                    }
                    skip_ws(&s);
                    if (*s == ',')
                        s++;
                }
                if (x >= 0 && y >= 0 && out->deco_count < 32)
                {
                    RogueDecoMarker* m = &out->deco[out->deco_count++];
                    m->x = x;
                    m->y = y;
#ifdef _MSC_VER
                    strncpy_s(m->kind, sizeof m->kind, kind, _TRUNCATE);
#else
                    strncpy(m->kind, kind, sizeof m->kind - 1);
#endif
                }
                skip_ws(&s);
                if (*s == ',')
                    s++;
            }
        }
        else
        { /* skip unknown value: naive skip by consuming literals/strings/arrays/objects shallowly
           */
            if (*s == '"')
            {
                char tmp[64];
                (void) parse_json_string(&s, tmp, sizeof tmp);
            }
            else if (*s == '{')
            {
                int depth = 1;
                s++;
                while (*s && depth)
                {
                    if (*s == '{')
                        depth++;
                    else if (*s == '}')
                        depth--;
                    s++;
                }
            }
            else if (*s == '[')
            {
                int depth = 1;
                s++;
                while (*s && depth)
                {
                    if (*s == '[')
                        depth++;
                    else if (*s == ']')
                        depth--;
                    s++;
                }
            }
            else
            {
                int dummy;
                (void) parse_json_number_i(&s, &dummy);
            }
        }
        skip_ws(&s);
        if (*s == ',')
            s++;
    }
    (void) err;
    (void) err_cap;
    return 1;
}
