/* Test v5 string interning section presence and roundtrip */
#include "core/save_manager.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/* Buff stub (deduplicated) */
/* Use real buff system */

static int section_cb(const struct RogueSaveDescriptor* d,uint32_t id,const void* data,uint32_t size,void* user){ (void)d;(void)data;(void)size; int* f=(int*)user; if(id==7) *f=1; return 0; }

int main(void){
    if(ROGUE_SAVE_FORMAT_VERSION < 5u){ printf("STRINGS_SKIP v=%u\n", (unsigned)ROGUE_SAVE_FORMAT_VERSION); return 0; }
    rogue_save_manager_reset_for_tests();
    rogue_save_manager_init();
    rogue_register_core_save_components();
    int a = rogue_save_intern_string("health");
    int b = rogue_save_intern_string("mana");
    int c = rogue_save_intern_string("health"); /* duplicate */
    if(a<0||b<0||c!=a){ printf("STRINGS_FAIL intern logic a=%d b=%d c=%d\n",a,b,c); return 1; }
    if(rogue_save_manager_save_slot(0)!=0){ printf("STRINGS_FAIL save\n"); return 1; }
    /* verify string component present via iteration */
    int found=0; int rc = rogue_save_for_each_section(0, section_cb, &found);
    if(rc!=0 || !found){ printf("STRINGS_FAIL section rc=%d found=%d\n", rc, found); return 1; }
    printf("STRINGS_OK count=%d a=%d b=%d\n", rogue_save_intern_count(), a,b);
    return 0; }
