/* Phase 17.2: Hot reload of equipment set definitions test */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/equipment_content.h"
#include "util/hot_reload.h"

/* simple assert helper */
static void ck(int cond, const char* msg){ if(!cond){ printf("FAIL:%s\n", msg); exit(1);} }

static int write_file(const char* path, const char* text){ FILE* f=NULL; 
#if defined(_MSC_VER)
    fopen_s(&f,path,"wb");
#else
    f=fopen(path,"wb");
#endif
    if(!f) return -1; fputs(text,f); fclose(f); return 0; }

int main(void){
    rogue_sets_reset();
    rogue_hot_reload_reset();

    const char* path = "tmp_equipment_sets_hot_reload.json";
    /* initial file: one set with two bonuses */
    const char* v1 = "[ { \"set_id\": 101, \"bonuses\": [ { \"pieces\":2, \"strength\":5 }, { \"pieces\":4, \"strength\":10 } ] } ]";
    const char* v2 = "[ { \"set_id\": 101, \"bonuses\": [ { \"pieces\":2, \"strength\":5 }, { \"pieces\":4, \"strength\":10 } ] }, { \"set_id\": 202, \"bonuses\": [ { \"pieces\":3, \"dexterity\":7 } ] } ]";

    ck(write_file(path,v1)==0, "write v1");
    ck(rogue_equipment_sets_register_hot_reload("equip_sets", path)==0, "register reload");

    /* initial force load */
    ck(rogue_hot_reload_force("equip_sets")==0, "force load v1");
    ck(rogue_set_count()==1, "one set after v1 load");
    const RogueSetDef* s1 = rogue_set_find(101); ck(s1 && s1->bonus_count==2, "set 101 present w/2 bonuses");

    /* modify file to add second set */
    ck(write_file(path,v2)==0, "write v2");
    int fired = rogue_hot_reload_tick();
    ck(fired==1, "tick fired once after modification");
    ck(rogue_set_count()==2, "two sets after v2 load");
    ck(rogue_set_find(202)!=NULL, "set 202 present");

    /* determinism: force should not change count */
    ck(rogue_hot_reload_force("equip_sets")==0, "force second time");
    ck(rogue_set_count()==2, "count stable after force");

    remove(path);
    printf("Phase17.2 equipment hot reload OK (sets=%d)\n", rogue_set_count());
    return 0;
}
