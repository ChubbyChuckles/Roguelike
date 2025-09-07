#include "../../src/world/world_gen_room_templates.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* base_json_fields[] = {"\"id\":101",
                                         "\"cls\":\"small\"",
                                         "\"biome_tags\":\"crypt\"",
                                         "\"encounter_slots\":1",
                                         "\"hazard_slots\":2",
                                         "\"puzzle_slot\":0",
                                         "\"grid\":[\"D.E\",\".L.\"]",
                                         "\"doors\":[{\"x\":0,\"y\":0,\"type\":\"secret\"}]",
                                         "\"deco\":[{\"x\":2,\"y\":0,\"kind\":\"banner\"}]"};

static void shuffle(int* idx, int n)
{
    for (int i = n - 1; i > 0; --i)
    {
        int j = rand() % (i + 1);
        int t = idx[i];
        idx[i] = idx[j];
        idx[j] = t;
    }
}

static int build_json_with_order(char* buf, size_t cap, const int* order, int n)
{
    size_t off = 0;
    off += (size_t) snprintf(buf + off, cap - off, "{");
    for (int i = 0; i < n; ++i)
    {
        if (i)
            off += (size_t) snprintf(buf + off, cap - off, ",");
        off += (size_t) snprintf(buf + off, cap - off, "%s", base_json_fields[order[i]]);
    }
    off += (size_t) snprintf(buf + off, cap - off, "}");
    return (int) off;
}

static int run_fuzz(void)
{
    int n = (int) (sizeof(base_json_fields) / sizeof(base_json_fields[0]));
    int idx[16];
    for (int i = 0; i < n; ++i)
        idx[i] = i;
    RogueRoomTemplate ref, tmp;
    char err[128];
    char jbuf[2048];
    /* Build reference with original order */
    build_json_with_order(jbuf, sizeof jbuf, idx, n);
    if (!rogue_room_template_load_json_text(jbuf, &ref, err, sizeof err))
    {
        fprintf(stderr, "ref json load failed: %s\n", err);
        return 1;
    }
    for (int iter = 0; iter < 32; ++iter)
    {
        shuffle(idx, n);
        build_json_with_order(jbuf, sizeof jbuf, idx, n);
        if (!rogue_room_template_load_json_text(jbuf, &tmp, err, sizeof err))
        {
            fprintf(stderr, "fuzz json load failed (iter %d): %s\n", iter, err);
            return 1;
        }
        if (tmp.id != ref.id || tmp.cls != ref.cls || tmp.width != ref.width ||
            tmp.height != ref.height || strcmp(tmp.biome_tags, ref.biome_tags) != 0 ||
            tmp.encounter_slots != ref.encounter_slots || tmp.hazard_slots != ref.hazard_slots ||
            tmp.puzzle_slot != ref.puzzle_slot || tmp.door_count != ref.door_count ||
            tmp.deco_count != ref.deco_count)
        {
            fprintf(stderr, "mismatch after shuffle iter %d\n", iter);
            return 1;
        }
        rogue_room_template_free(&tmp);
    }
    rogue_room_template_free(&ref);
    return 0;
}

int main(void)
{
    srand(12345);
    int rc = run_fuzz();
    if (rc == 0)
        printf("room templates json fuzz: ok\n");
    return rc;
}
