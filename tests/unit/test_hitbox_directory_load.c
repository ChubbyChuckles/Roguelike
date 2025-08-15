#include "game/hitbox_load.h"
#include <assert.h>
#include <stdio.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

static void write_file(const char* path, const char* contents){ FILE* f=fopen(path,"wb"); assert(f); fputs(contents,f); fclose(f); }

int main(void){
#ifdef _WIN32
    _mkdir("hitboxes");
#else
    mkdir("hitboxes", 0777);
#endif
    write_file("hitboxes/slash.hitbox","[{\n  \"type\":\"arc\", \"ox\":0, \"oy\":0, \"radius\":1.5, \"angle_start\":0, \"angle_end\":1.57\n}]");
    write_file("hitboxes/thrust.json","[{\n  \"type\":\"capsule\", \"ax\":0, \"ay\":0, \"bx\":2, \"by\":0, \"radius\":0.2\n}]");
    RogueHitbox hb[16]; int count=0; int ok = rogue_hitbox_load_directory("hitboxes", hb, 16, &count); assert(ok); assert(count==2);
    /* Basic property checks */
    int arc_found=0, capsule_found=0; for(int i=0;i<count;i++){ if(hb[i].type==ROGUE_HITBOX_ARC) arc_found=1; if(hb[i].type==ROGUE_HITBOX_CAPSULE) capsule_found=1; }
    assert(arc_found && capsule_found);
    return 0;
}
