#include "../src/content/json_envelope.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_create_and_parse_basic(void)
{
    char err[128];
    char* json = NULL;
    const char* schema = "rogue://schemas/items";
    const unsigned version = 3;
    const char* entries = "[{\n  \"id\": 1, \"name\": \"Sword\"\n}]";
    int rc = json_envelope_create(schema, version, entries, &json, err, (int) sizeof err);
    assert(rc == 0);
    assert(json && strstr(json, schema) && strstr(json, "\"version\": 3"));

    RogueJsonEnvelope env;
    rc = json_envelope_parse(json, &env, err, (int) sizeof err);
    assert(rc == 0);
    assert(env.version == version);
    assert(strcmp(env.schema, schema) == 0);
    assert(env.entries && strstr(env.entries, "\"id\": 1"));

    json_envelope_free(&env);
    free(json);
}

static void test_parse_errors(void)
{
    char err[128];
    RogueJsonEnvelope env;
    const char* bad1 = "{}";
    int rc = json_envelope_parse(bad1, &env, err, (int) sizeof err);
    assert(rc != 0);

    const char* bad2 = "{ \"$schema\": 1, \"version\": 1, \"entries\": [] }";
    rc = json_envelope_parse(bad2, &env, err, (int) sizeof err);
    assert(rc != 0);

    const char* bad3 = "{ \"$schema\": \"s\", \"version\": 1, \"entries\": 123 }";
    rc = json_envelope_parse(bad3, &env, err, (int) sizeof err);
    assert(rc != 0);
}

int main(void)
{
    test_create_and_parse_basic();
    test_parse_errors();
    return 0;
}
