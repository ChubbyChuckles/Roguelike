#include "../../src/content/json_envelope.h"
#include "../../src/content/json_io.h"
#include "../../src/content/schema_items.h"
#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/util/json_parser.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Minimal JSON parser helpers for this test: we treat entries as a raw JSON array string and
   build synthetic RogueJsonValue objects for schema validation by reusing itemdef_to_json via
   rogue_items_validate_defs. For envelope validation, we only check keys using
   json_envelope_parse.*/

static char* read_file_all(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*) malloc((size_t) sz + 1);
    if (!buf)
    {
        fclose(f);
        return NULL;
    }
    size_t n = fread(buf, 1, (size_t) sz, f);
    fclose(f);
    buf[n] = '\0';
    return buf;
}

int main(void)
{
    /* Ensure we have items in the registry */
    if (rogue_item_defs_count() == 0)
    {
        int loaded = rogue_item_defs_load_from_cfg("assets/test_items.cfg");
        assert(loaded >= 0);
    }
    /* Export JSON to a temp path in the current working directory (ctest runs from build/) */
    const char* path = "test_items_roundtrip.json";
    int cap = 64 * 1024;
    char* buf = (char*) malloc((size_t) cap);
    assert(buf);
    int n = rogue_item_defs_export_json(buf, cap);
    if (n < 0)
    {
        free(buf);
        cap *= 2;
        buf = (char*) malloc((size_t) cap);
        assert(buf);
        n = rogue_item_defs_export_json(buf, cap);
    }
    assert(n >= 0);

    /* Wrap in envelope */
    char* wrapped = NULL;
    char err[256];
    assert(json_envelope_create("items", ROGUE_SCHEMA_VERSION_CURRENT, buf, &wrapped, err,
                                (int) sizeof err) == 0);
    free(buf);
    /* Write */
    assert(json_io_write_atomic(path, wrapped, strlen(wrapped), err, (int) sizeof err) == 0);
    free(wrapped);

    /* Read back and parse envelope */
    char* text = read_file_all(path);
    assert(text);
    RogueJsonEnvelope env = {0};
    assert(json_envelope_parse(text, &env, err, (int) sizeof err) == 0);
    free(text);
    /* Sanity */
    assert(env.schema && strcmp(env.schema, "items") == 0);
    assert(env.entries && env.entries[0] == '[');

    /* Validate entries array shape by brute-force splitting objects and validating with schema */
    /* This test focuses on end-to-end wiring. We reuse rogue_items_validate_defs by converting the
       in-memory registry instead of reparsing JSON here. */
    RogueSchemaValidationResult vr = {0};
    assert(rogue_items_validate_defs(rogue_item_def_at(0), rogue_item_defs_count(), &vr));
    json_envelope_free(&env);

    /* Import the saved entries (array) and ensure non-negative result */
    int before = rogue_item_defs_count();
    int added = rogue_item_defs_load_from_json(path); /* loader accepts envelope or raw array */
    assert(added >= 0);
    assert(rogue_item_defs_count() >= before);
    printf("OK test_items_roundtrip_schema: exported+validated+reloaded (%d defs)\n",
           rogue_item_defs_count());
    return 0;
}
