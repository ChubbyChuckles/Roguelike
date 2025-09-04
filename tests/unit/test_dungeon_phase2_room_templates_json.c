#include "world/tilemap.h"
#include "world/world_gen_room_templates.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void make_empty_map(RogueTileMap* m, int w, int h)
{
    m->width = w;
    m->height = h;
    m->tiles = (unsigned char*) malloc((size_t) w * h);
    memset(m->tiles, ROGUE_TILE_EMPTY, (size_t) w * h);
}

static int test_json_loader_ascii_doors_and_exits(void)
{
    const char* json = "{\n"
                       "  \"id\": 201,\n"
                       "  \"cls\": \"medium\",\n"
                       "  \"biome_tags\": \"crypt,undead\",\n"
                       "  \"encounter_slots\": 2,\n"
                       "  \"hazard_slots\": 1,\n"
                       "  \"puzzle_slot\": 0,\n"
                       "  \"grid\": [\n"
                       "    \"S..#\",\n"
                       "    \"E.D#\",\n"
                       "    \".L.#\"\n"
                       "  ]\n"
                       "}";
    RogueRoomTemplate t;
    char err[128];
    int ok = rogue_room_template_load_json_text(json, &t, err, sizeof err);
    if (!ok)
    {
        fprintf(stderr, "json load failed: %s\n", err);
        return 1;
    }
    if (t.id != 201 || t.cls != ROGUE_ROOM_MEDIUM)
    {
        fprintf(stderr, "id/cls mismatch\n");
        return 1;
    }
    if (t.width != 4 || t.height != 3)
    {
        fprintf(stderr, "dims mismatch %dx%d\n", t.width, t.height);
        return 1;
    }
    if (strcmp(t.biome_tags, "crypt,undead") != 0)
    {
        fprintf(stderr, "biome_tags mismatch: '%s'\n", t.biome_tags);
        return 1;
    }
    if (t.encounter_slots != 2 || t.hazard_slots != 1 || t.puzzle_slot != 0)
    {
        fprintf(stderr, "slot fields mismatch\n");
        return 1;
    }
    /* Exits from ASCII: expect one at (0,1) */
    if (t.exit_count != 1 || t.exits[0].x != 0 || t.exits[0].y != 1)
    {
        fprintf(stderr, "exit parse mismatch\n");
        return 1;
    }
    /* Doors from ASCII: S(0,0)=secret, D(2,1)=normal, L(1,2)=locked */
    if (t.door_count != 3)
    {
        fprintf(stderr, "door_count mismatch (got %d)\n", t.door_count);
        return 1;
    }
    int found_secret = 0, found_normal = 0, found_locked = 0;
    for (int i = 0; i < t.door_count; ++i)
    {
        if (t.doors[i].x == 0 && t.doors[i].y == 0 && t.doors[i].type == ROGUE_DOOR_SECRET)
            found_secret = 1;
        if (t.doors[i].x == 2 && t.doors[i].y == 1 && t.doors[i].type == ROGUE_DOOR_NORMAL)
            found_normal = 1;
        if (t.doors[i].x == 1 && t.doors[i].y == 2 && t.doors[i].type == ROGUE_DOOR_LOCKED)
            found_locked = 1;
    }
    if (!(found_secret && found_normal && found_locked))
    {
        fprintf(stderr, "door parse mismatch\n");
        return 1;
    }

    /* Stamp and verify tile kinds (E becomes floor) */
    RogueTileMap map;
    make_empty_map(&map, 8, 8);
    (void) rogue_dungeon_stamp_template(&map, 2, 2, &t, 0, 0);
    if (map.tiles[(2 + 1) * 8 + (2 + 0)] != ROGUE_TILE_DUNGEON_FLOOR)
    {
        fprintf(stderr, "E not floor on stamp\n");
        return 1;
    }
    if (map.tiles[(2 + 0) * 8 + (2 + 0)] != ROGUE_TILE_DUNGEON_SECRET_DOOR)
    {
        fprintf(stderr, "S not secret door on stamp\n");
        return 1;
    }
    if (map.tiles[(2 + 1) * 8 + (2 + 2)] != ROGUE_TILE_DUNGEON_DOOR)
    {
        fprintf(stderr, "D not door on stamp\n");
        return 1;
    }
    if (map.tiles[(2 + 2) * 8 + (2 + 1)] != ROGUE_TILE_DUNGEON_LOCKED_DOOR)
    {
        fprintf(stderr, "L not locked door on stamp\n");
        return 1;
    }

    /* Transform door positions must align with stamped tiles after rotation */
    RogueRoomDoor d[8];
    int dc = rogue_room_template_compute_doors(&t, 90, 0, d, 8);
    if (dc != 3)
    {
        fprintf(stderr, "door compute count mismatch (90)\n");
        return 1;
    }
    RogueTileMap map90;
    make_empty_map(&map90, 8, 8);
    (void) rogue_dungeon_stamp_template(&map90, 0, 0, &t, 90, 0);
    for (int i = 0; i < dc; ++i)
    {
        unsigned char tt = map90.tiles[d[i].y * map90.width + d[i].x];
        if (d[i].type == ROGUE_DOOR_NORMAL && tt != ROGUE_TILE_DUNGEON_DOOR)
        {
            fprintf(stderr, "door align fail normal@%d,%d\n", d[i].x, d[i].y);
            return 1;
        }
        if (d[i].type == ROGUE_DOOR_LOCKED && tt != ROGUE_TILE_DUNGEON_LOCKED_DOOR)
        {
            fprintf(stderr, "door align fail locked\n");
            return 1;
        }
        if (d[i].type == ROGUE_DOOR_SECRET && tt != ROGUE_TILE_DUNGEON_SECRET_DOOR)
        {
            fprintf(stderr, "door align fail secret\n");
            return 1;
        }
    }

    rogue_room_template_free(&t);
    free(map.tiles);
    free(map90.tiles);
    return 0;
}

static int test_json_loader_explicit_doors_and_deco(void)
{
    const char* json = "{\n"
                       "  \"id\": 202,\n"
                       "  \"cls\": \"small\",\n"
                       "  \"grid\": [\n"
                       "    \"S..#\",\n"
                       "    \"E.D#\",\n"
                       "    \".L.#\"\n"
                       "  ],\n"
                       "  \"doors\": [\n"
                       "    {\"x\":2,\"y\":1,\"type\":\"normal\"},\n"
                       "    {\"x\":1,\"y\":2,\"type\":\"locked\",\"key_id\":42},\n"
                       "    {\"x\":0,\"y\":0,\"type\":\"secret\"}\n"
                       "  ],\n"
                       "  \"deco\": [\n"
                       "    {\"x\":1,\"y\":0,\"kind\":\"banner\"},\n"
                       "    {\"x\":3,\"y\":2,\"kind\":\"pillar\"}\n"
                       "  ]\n"
                       "}";
    RogueRoomTemplate t;
    char err[128];
    int ok = rogue_room_template_load_json_text(json, &t, err, sizeof err);
    if (!ok)
    {
        fprintf(stderr, "json load failed: %s\n", err);
        return 1;
    }
    if (t.door_count != 3)
    {
        fprintf(stderr, "door_count mismatch explicit\n");
        return 1;
    }
    int saw_key = 0;
    for (int i = 0; i < t.door_count; ++i)
    {
        if (t.doors[i].x == 1 && t.doors[i].y == 2)
        {
            if (t.doors[i].type != ROGUE_DOOR_LOCKED || t.doors[i].key_id != 42)
            {
                fprintf(stderr, "locked door metadata mismatch\n");
                return 1;
            }
            saw_key = 1;
        }
    }
    if (!saw_key)
    {
        fprintf(stderr, "explicit locked door not found\n");
        return 1;
    }
    if (t.deco_count != 2)
    {
        fprintf(stderr, "deco count mismatch\n");
        return 1;
    }
    if (strcmp(t.deco[0].kind, "banner") != 0 && strcmp(t.deco[1].kind, "banner") != 0)
    {
        fprintf(stderr, "deco kind banner missing\n");
        return 1;
    }
    if (strcmp(t.deco[0].kind, "pillar") != 0 && strcmp(t.deco[1].kind, "pillar") != 0)
    {
        fprintf(stderr, "deco kind pillar missing\n");
        return 1;
    }

    /* Transform and check key_id persists */
    RogueRoomDoor d[8];
    int dc = rogue_room_template_compute_doors(&t, 180, 1, d, 8);
    if (dc != 3)
    {
        fprintf(stderr, "door compute count mismatch (180,reflect)\n");
        return 1;
    }
    int found_key = 0;
    for (int i = 0; i < dc; ++i)
        if (d[i].type == ROGUE_DOOR_LOCKED && d[i].key_id == 42)
            found_key = 1;
    if (!found_key)
    {
        fprintf(stderr, "key_id not preserved across transform\n");
        return 1;
    }

    /* Stamping still respects grid markers for door tiles */
    RogueTileMap map;
    make_empty_map(&map, 8, 8);
    (void) rogue_dungeon_stamp_template(&map, 0, 0, &t, 0, 0);
    if (map.tiles[1 * 8 + 2] != ROGUE_TILE_DUNGEON_DOOR)
    {
        fprintf(stderr, "stamp door mismatch at grid D pos\n");
        return 1;
    }
    if (map.tiles[2 * 8 + 1] != ROGUE_TILE_DUNGEON_LOCKED_DOOR)
    {
        fprintf(stderr, "stamp locked door mismatch at grid L pos\n");
        return 1;
    }
    if (map.tiles[0 * 8 + 0] != ROGUE_TILE_DUNGEON_SECRET_DOOR)
    {
        fprintf(stderr, "stamp secret door mismatch at grid S pos\n");
        return 1;
    }

    rogue_room_template_free(&t);
    free(map.tiles);
    return 0;
}

int main(void)
{
    int rc = 0;
    rc = test_json_loader_ascii_doors_and_exits();
    if (rc)
        return rc;
    rc = test_json_loader_explicit_doors_and_deco();
    if (rc)
        return rc;
    printf("room templates json: ok\n");
    return 0;
}
