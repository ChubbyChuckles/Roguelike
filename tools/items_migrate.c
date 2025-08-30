#include "../src/content/json_envelope.h"
#include "../src/content/json_io.h"
#include "../src/core/integration/json_schema.h"
#include "../src/core/loot/loot_item_defs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Simple CLI: exports current items registry to a versioned JSON envelope.
   Usage: items_migrate <out_path.json>
   Exit code 0 on success, non-zero on error. */
int main(int argc, char** argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <out.json>\n", argv[0]);
        return 2;
    }
    const char* out_path = argv[1];
    /* Ensure registry has content; try loading default assets if empty */
    if (rogue_item_defs_count() == 0)
    {
        int loaded = rogue_item_defs_load_directory("assets/items");
        if (loaded <= 0)
        {
            loaded = rogue_item_defs_load_from_cfg("assets/test_items.cfg");
        }
        if (loaded <= 0)
        {
            fprintf(stderr, "No item definitions loaded; nothing to export.\n");
            return 3;
        }
    }

    /* Export registry to JSON array */
    int cap = 64 * 1024;
    const int cap_max = 16 * 1024 * 1024;
    char* entries = NULL;
    int n = -1;
    for (;;)
    {
        free(entries);
        entries = (char*) malloc((size_t) cap);
        if (!entries)
        {
            fprintf(stderr, "Out of memory allocating %d bytes\n", cap);
            return 4;
        }
        n = rogue_item_defs_export_json(entries, cap);
        if (n >= 0)
            break;
        cap *= 2;
        if (cap > cap_max)
        {
            fprintf(stderr, "Export too large (> %d bytes)\n", cap_max);
            free(entries);
            return 5;
        }
    }

    /* Wrap in envelope */
    char* wrapped = NULL;
    char err[256];
    if (json_envelope_create("items", ROGUE_SCHEMA_VERSION_CURRENT, entries, &wrapped, err,
                             (int) sizeof err) != 0)
    {
        fprintf(stderr, "Envelope create failed: %s\n", err);
        free(entries);
        return 6;
    }
    free(entries);

    /* Write atomically */
    int wrc = json_io_write_atomic(out_path, wrapped, strlen(wrapped), err, (int) sizeof err);
    free(wrapped);
    if (wrc != 0)
    {
        fprintf(stderr, "Write failed: %s\n", err);
        return 7;
    }
    printf("Exported %d item defs to %s\n", rogue_item_defs_count(), out_path);
    return 0;
}
