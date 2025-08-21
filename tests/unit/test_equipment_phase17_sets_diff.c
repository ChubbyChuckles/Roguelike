/* Phase 17.4: sets diff tool test */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/equipment/equipment_modding.h"
#include "core/equipment/equipment_content.h"

static int write_file(const char* path, const char* text){ FILE* f=NULL; 
#if defined(_MSC_VER)
    fopen_s(&f,path,"wb");
#else
    f=fopen(path,"wb");
#endif
    if(!f) return -1; fputs(text,f); fclose(f); return 0; }

static void ck(int c, const char* m){ if(!c){ printf("FAIL:%s\n", m); exit(1);} }

int main(){
    rogue_sets_reset();
    const char* base_path = "tmp_sets_base.json";
    const char* mod_path  = "tmp_sets_mod.json";
    const char* base_json = "[ { \"set_id\": 10, \"bonuses\": [ { \"pieces\":2, \"strength\":5 } ] }, { \"set_id\": 20, \"bonuses\": [ { \"pieces\":3, \"dexterity\":4 } ] } ]";
    const char* mod_json  = "[ { \"set_id\": 10, \"bonuses\": [ { \"pieces\":2, \"strength\":6 } ] }, { \"set_id\": 30, \"bonuses\": [ { \"pieces\":4, \"vitality\":7 } ] } ]"; /* set 20 removed, 30 added, 10 changed */
    ck(write_file(base_path, base_json)==0, "write base");
    ck(write_file(mod_path, mod_json)==0, "write mod");
    char buf[256]; int n = rogue_sets_diff(base_path, mod_path, buf, sizeof buf);
    ck(n>0, "diff success");
    ck(strstr(buf,"\"added\":[30]")!=NULL, "added 30");
    ck(strstr(buf,"\"removed\":[20]")!=NULL, "removed 20");
    ck(strstr(buf,"\"changed\":[10]")!=NULL, "changed 10");
    remove(base_path); remove(mod_path);
    printf("Phase17.4 sets diff OK (%s)\n", buf);
    return 0;
}
