/* Test v7 integrity: per-section CRC + overall SHA256 */
#include "core/save_manager.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/* Buff stub (deduplicated) */
/* Use real buff system */

static int section_counter=0; static int iter_cb(const struct RogueSaveDescriptor* d,uint32_t id,const void* data,uint32_t size,void* user){ (void)d;(void)id;(void)data;(void)size;(void)user; section_counter++; return 0; }

int main(void){
    if(ROGUE_SAVE_FORMAT_VERSION < 7u){ printf("INTEG_SKIP v=%u\n", (unsigned)ROGUE_SAVE_FORMAT_VERSION); return 0; }
    rogue_save_manager_reset_for_tests();
    rogue_save_manager_init();
    rogue_register_core_save_components();
    /* Add some interned strings for a section with payload */
    rogue_save_intern_string("alpha");
    rogue_save_intern_string("beta");
    int save_rc = rogue_save_manager_save_slot(0);
    printf("save rc=%d\n", save_rc);
    if(save_rc!=0){ printf("INTEG_FAIL save\n"); return 1; }
    unsigned char digest[32]; memcpy(digest, rogue_save_last_sha256(), 32);
    /* Load triggers verification and populates last sha again */
    int lrc = rogue_save_manager_load_slot(0);
    printf("load rc=%d\n", lrc);
    if(lrc!=0){ printf("INTEG_FAIL load rc=%d\n", lrc); return 1; }
    unsigned char digest2[32]; memcpy(digest2, rogue_save_last_sha256(), 32);
        if(memcmp(digest,digest2,32)!=0){ printf("INTEG_FAIL digest mismatch\n"); return 1; }
    /* Iterate sections to ensure enumeration still works under v7 layout */
    int rc = rogue_save_for_each_section(0, iter_cb, NULL);
    printf("iter rc=%d section_counter=%d\n", rc, section_counter);
    if(rc!=0 || section_counter==0){ printf("INTEG_FAIL iter rc=%d cnt=%d\n", rc, section_counter); return 1; }
    char hex[65]; rogue_save_last_sha256_hex(hex); if(strlen(hex)!=64){ printf("INTEG_FAIL hex len\n"); return 1; }
    printf("INTEG_OK sections=%d sha=%s\n", section_counter, hex);
    return 0; }
