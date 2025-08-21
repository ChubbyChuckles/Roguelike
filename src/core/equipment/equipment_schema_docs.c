/* Phase 17.1 implementation: schema docs exporter */
#include "core/equipment/equipment_schema_docs.h"
#include <stdio.h>
#include <string.h>

int rogue_equipment_schema_docs_export(char* buf, int cap)
{
    if (!buf || cap < 16)
        return -1;
    int off = 0; /* Minimal static description; future expansion can introspect runtime tables */
    const char* header = "{\n  \"version\":1,\n";
    int n = (int) strlen(header);
    if (off + n >= cap)
        return -1;
    memcpy(buf + off, header, (size_t) n);
    off += n;
    const char* item =
        "  \"item_def\":{\n"
        "    \"fields\":[\n"
        "      {\"name\":\"id\",\"type\":\"int\"},\n"
        "      {\"name\":\"name\",\"type\":\"string\"},\n"
        "      {\"name\":\"category\",\"type\":\"enum(weapon,armor,consumable,gem,material)\"},\n"
        "      {\"name\":\"rarity\",\"type\":\"int(0-4)\"},\n"
        "      {\"name\":\"base_damage_min\",\"type\":\"int\"},\n"
        "      {\"name\":\"base_damage_max\",\"type\":\"int\"},\n"
        "      {\"name\":\"socket_min\",\"type\":\"int\"},\n"
        "      {\"name\":\"socket_max\",\"type\":\"int\"}\n"
        "    ]\n  },\n";
    n = (int) strlen(item);
    if (off + n >= cap)
        return -1;
    memcpy(buf + off, item, (size_t) n);
    off += n;
    const char* set_schema =
        "  \"set_def\":{\n    \"object\":{\"set_id\":\"int>0\",\"bonuses\":\"array<bonus> "
        "(1-4)\"},\n    \"bonus\":{\"pieces\":\"ascending "
        "int\",\"strength\":\"int\",\"dexterity\":\"int\",\"vitality\":\"int\",\"intelligence\":"
        "\"int\",\"armor_flat\":\"int\",\"resist_fire\":\"int\",\"resist_cold\":\"int\",\"resist_"
        "light\":\"int\",\"resist_poison\":\"int\",\"resist_status\":\"int\",\"resist_physical\":"
        "\"int\"}\n  },\n";
    n = (int) strlen(set_schema);
    if (off + n >= cap)
        return -1;
    memcpy(buf + off, set_schema, (size_t) n);
    off += n;
    const char* proc_schema =
        "  \"proc_def\":{\n    "
        "\"fields\":[{\"name\":\"name\",\"type\":\"string\"},{\"name\":\"trigger\",\"type\":\"enum("
        "on_hit,on_crit,on_kill,on_block,on_dodge,when_low_hp)\"},{\"name\":\"icd_ms\",\"type\":"
        "\"int>=0\"},{\"name\":\"duration_ms\",\"type\":\"int>=0\"},{\"name\":\"magnitude\","
        "\"type\":\"float\"},{\"name\":\"max_stacks\",\"type\":\"int>=1\"},{\"name\":\"stack_"
        "rule\",\"type\":\"enum(refresh,stack,ignore)\"},{\"name\":\"param\",\"type\":\"float("
        "optional)\"}]\n  },\n";
    n = (int) strlen(proc_schema);
    if (off + n >= cap)
        return -1;
    memcpy(buf + off, proc_schema, (size_t) n);
    off += n;
    const char* runeword =
        "  \"runeword_pattern\":{\n    \"rules\":\"lowercase alnum + single underscores, max 5 "
        "segments, max length 11, no double underscores\"\n  }\n";
    n = (int) strlen(runeword);
    if (off + n >= cap)
        return -1;
    memcpy(buf + off, runeword, (size_t) n);
    off += n;
    if (off + 3 >= cap)
        return -1;
    buf[off++] = '}';
    buf[off++] = '\n';
    buf[off] = '\0';
    return off;
}
