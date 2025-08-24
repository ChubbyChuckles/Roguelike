/* test_kv_schema.c - Unit test for schema validation (Phase M3.2) */
#include "../../src/util/kv_schema.h"
#include <stdio.h>
#include <string.h>

static const char* content = "hp=100\n"
                             "speed=1.5\n"
                             "name=Hero\n"
                             "unknown=42\n";

int main(void)
{
    RogueKVFile f = {content, (int) strlen(content)};
    RogueKVFieldDef defs[] = {
        {"hp", ROGUE_KV_INT, 1}, {"speed", ROGUE_KV_FLOAT, 1}, {"name", ROGUE_KV_STRING, 0}};
    RogueKVSchema schema = {defs, 3};
    RogueKVFieldValue values[8];
    char err[256];
    int errs = rogue_kv_validate(&f, &schema, values, 8, err, sizeof err);
    if (errs < 1)
    {
        printf("FAIL expected at least 1 error (unknown key)\n");
        return 1;
    }
    int found_hp = 0, found_speed = 0;
    for (int i = 0; i < 3; i++)
    {
        if (values[i].present && values[i].def == &defs[0])
            found_hp = 1;
        if (values[i].present && values[i].def == &defs[1])
            found_speed = 1;
    }
    if (!found_hp || !found_speed)
    {
        printf("FAIL required fields missing in values array\n");
        return 1;
    }
    printf("OK test_kv_schema (%d errors)\n", errs);
    return 0;
}
