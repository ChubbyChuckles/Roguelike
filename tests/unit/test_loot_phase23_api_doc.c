#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "core/loot_api_doc.h"

/* Unit test for Itemsystem Phase 23.5 API doc generator */
int main(void){
    char buf[4096];
    int n = rogue_loot_generate_api_doc(buf, (int)sizeof buf);
    assert(n > 0);
    /* Basic invariants */
    assert(strstr(buf, "LOOT API REFERENCE")!=NULL);
    assert(strstr(buf, "rogue_loot_roll_hash")!=NULL);
    assert(strstr(buf, "rogue_loot_tables_roll")!=NULL);
    /* Stable ordering: anomaly_flag should appear before anomaly_record (since same prefix then alphabetical) */
    const char* flag_pos = strstr(buf, "rogue_loot_anomaly_flag");
    const char* rec_pos = strstr(buf, "rogue_loot_anomaly_record");
    assert(flag_pos && rec_pos && flag_pos < rec_pos);
    /* Truncation guard: buffer large enough so doc ends with newline */
    size_t len = strlen(buf);
    assert(len>0 && buf[len-1]=='\n');
    /* Small buffer failure path */
    char tiny[8];
    int r = rogue_loot_generate_api_doc(tiny, (int)sizeof tiny);
    assert(r==-1); /* too small */
    return 0;
}
