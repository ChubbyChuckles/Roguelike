#include "../../src/content/schema_tilesets.h"
#include "../../src/util/log.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    RogueSchemaValidationResult res = {0};
    int count = 0;
    if (!rogue_tilesets_validate_assets_default(&res, &count))
    {
        fprintf(stderr, "tilesets schema validation failed: errors=%u\n", res.error_count);
        for (unsigned i = 0; i < res.error_count; ++i)
        {
            fprintf(stderr, " - [%u] %s: %s\n", i, res.errors[i].field_path, res.errors[i].message);
        }
        return 1;
    }
    printf("OK test_tilesets_schema (tiles=%d)\n", count);
    return 0;
}
