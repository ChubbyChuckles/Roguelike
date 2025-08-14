#include "core/loot_affixes.h"
#include "core/path_utils.h"
#include <stdio.h>

static int fail(const char* msg){ fprintf(stderr,"FAIL:%s\n", msg); return 1; }

int main(void){
    char path[256];
    if(!rogue_find_asset_path("affixes.cfg", path, sizeof path)) return fail("path");
    rogue_affixes_reset();
    int added = rogue_affixes_load_from_cfg(path);
    if(added < 4) return fail("added");
    int idx = rogue_affix_index("sharp");
    if(idx<0) return fail("index");
    const RogueAffixDef* a = rogue_affix_at(idx);
    if(!a) return fail("at_null");
    if(!(a->min_value==1 && a->max_value==3)) return fail("range");
    printf("AFFIX_PARSE_OK count=%d first=%s range=%d-%d\n", rogue_affix_count(), a->id, a->min_value, a->max_value);
    return 0;
}
