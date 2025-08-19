#define SDL_MAIN_HANDLED
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "game/weapon_pose.h"

/* Minimal fake renderer dependency bypass (no textures expected) */
int main(){
#if defined(_MSC_VER)
    _mkdir("assets");
    _mkdir("assets\\weapons");
#else
    mkdir("assets",0755);
    mkdir("assets/weapons",0755);
#endif
    /* Write a temporary pose file */
    const char* path = "assets/weapons/weapon_0_pose.json";
    /* Ensure directory exists (assumed). */
    FILE* f=NULL; fopen_s(&f,path,"wb"); if(!f){ printf("cannot_open_pose_dir\n"); return 1; }
    fprintf(f,"{\n  \"weapon_id\":0,\n  \"frames\":[\n");
    for(int i=0;i<8;i++){
        fprintf(f,"    {\"dx\":%d,\"dy\":%d,\"angle\":%d,\"scale\":1.0,\"pivot_x\":0.5,\"pivot_y\":0.5}%s\n", i, -i, i*5, (i==7)?"":" ,");
    }
    fprintf(f,"  ]\n}\n"); fclose(f);
    if(!rogue_weapon_pose_ensure(0)){ printf("ensure_failed\n"); return 1; }
    const RogueWeaponPoseFrame* fr = rogue_weapon_pose_get(0,5);
    if(!fr){ printf("frame_null\n"); return 1; }
    if((int)fr->dx!=5 || (int)fr->dy!=-5 || (int)fr->angle!=25){ printf("mismatch dx=%.2f dy=%.2f angle=%.2f\n", fr->dx, fr->dy, fr->angle); return 1; }
    printf("weapon_pose_loader_ok\n");
    return 0;
}
