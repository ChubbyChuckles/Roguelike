/* Phase 16.2: Set builder + live bonus preview JSON tooling test */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "core/equipment/equipment_content.h"

static int write_temp_sets(const char* path){
    const char* json = "[\n"
        " {\"set_id\":101,\"bonuses\":[{\"pieces\":2,\"strength\":4,\"dexterity\":0,\"vitality\":0,\"intelligence\":0,\"armor_flat\":0,\"resist_fire\":1,\"resist_cold\":0,\"resist_light\":0,\"resist_poison\":0,\"resist_status\":0,\"resist_physical\":0},{\"pieces\":4,\"strength\":8,\"dexterity\":2,\"vitality\":1,\"intelligence\":0,\"armor_flat\":5,\"resist_fire\":2,\"resist_cold\":1,\"resist_light\":0,\"resist_poison\":0,\"resist_status\":0,\"resist_physical\":1}]}\n"
        "]";
    FILE* f=NULL;
#if defined(_MSC_VER)
    if(fopen_s(&f,path,"wb")!=0) f=NULL;
#else
    f=fopen(path,"wb");
#endif
    if(!f) return 0; fwrite(json,1,strlen(json),f); fclose(f); return 1;
}

/* Validate interpolation: at 3 pieces between thresholds 2->4 expect halfway toward second bonus delta. */
static int expect_preview_halfway(){
    int str=0,dex=0,vit=0,intel=0,armor=0,rf=0,rc=0,rl=0,rp=0,rs=0,rphys=0;
    rogue_set_preview_apply(101,3,&str,&dex,&vit,&intel,&armor,&rf,&rc,&rl,&rp,&rs,&rphys);
    /* Base at 2 pieces: str4 fire1. At 4 pieces: str8 dex2 vit1 armor5 fire2 cold1 phys1. Halfway (3 pieces) adds ~50% delta: +2 str, +1 dex, +0 vit (int trunc), +0 armor (int trunc from 2.5 ->2), +0 fire (0.5->0), +0 cold, +0 phys. */
    if(str != 6) return 10; /* expect exactly 6 after trunc */
    if(dex !=1) return 11;
    if(armor <2 || armor>3) return 12; /* allow 2 or 3 depending on trunc rounding path */
    return 0;
}

int main(void){
    rogue_sets_reset();
    const char* path="temp_sets_phase16.json";
    if(!write_temp_sets(path)){ fprintf(stderr,"write sets json failed\n"); return 1; }
    int added = rogue_sets_load_from_json(path);
    if(added !=1){ fprintf(stderr,"expected 1 set added got %d\n", added); return 2; }
    if(rogue_set_count()!=1){ fprintf(stderr,"registry count mismatch\n"); return 3; }
    const RogueSetDef* sd=rogue_set_find(101); if(!sd){ fprintf(stderr,"find set failed\n"); return 4; }
    if(sd->bonus_count!=2 || sd->bonuses[0].pieces!=2 || sd->bonuses[1].pieces!=4){ fprintf(stderr,"bonus thresholds wrong\n"); return 5; }
    int rc=expect_preview_halfway(); if(rc!=0){ fprintf(stderr,"preview halfway check failed %d\n", rc); return 6; }
    char buf[1024]; int n=rogue_sets_export_json(buf,(int)sizeof buf); if(n<0){ fprintf(stderr,"export failed\n"); return 7; }
    if(strstr(buf,"\"set_id\":101")==NULL || strstr(buf,"\"pieces\":4")==NULL){ fprintf(stderr,"export missing tokens\n"); return 8; }
    remove(path);
    printf("Phase16.2 set builder JSON + preview OK (%d chars)\n", n);
    return 0;
}
