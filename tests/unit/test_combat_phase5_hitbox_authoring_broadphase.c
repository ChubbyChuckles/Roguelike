#define SDL_MAIN_HANDLED
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "game/hitbox.h"
#include "game/hitbox_load.h"

static const char* SAMPLE_JSON =
"[\n"
" { \"type\":\"capsule\", \"ax\":0, \"ay\":0, \"bx\":2, \"by\":0, \"r\":0.5 },\n"
" { \"type\":\"arc\", \"ox\":0, \"oy\":0, \"radius\":2, \"a0\":0, \"a1\":1.57079632679 },\n"
" { \"type\":\"chain\", \"radius\":0.3, \"points\":[ [0,0],[1,0],[1,1] ] },\n"
" { \"type\":\"projectile_spawn\", \"count\":5, \"ox\":0, \"oy\":0, \"speed\":6, \"spread\":3.1415926535, \"center\":0 }\n"
"]";

int main(){
    RogueHitbox boxes[8]; int count=0; int ok = rogue_hitbox_load_sequence_from_memory(SAMPLE_JSON, boxes, 8, &count); assert(ok); assert(count==4);
    assert(boxes[0].type == ROGUE_HITBOX_CAPSULE);
    assert(boxes[1].type == ROGUE_HITBOX_ARC);
    assert(boxes[2].type == ROGUE_HITBOX_CHAIN && boxes[2].u.chain.count==3);
    assert(boxes[3].type == ROGUE_HITBOX_PROJECTILE_SPAWN && boxes[3].u.proj.projectile_count==5);
    /* Validate projectile spread ends & center */
    float first = rogue_hitbox_projectile_spawn_angle(&boxes[3].u.proj,0);
    float mid   = rogue_hitbox_projectile_spawn_angle(&boxes[3].u.proj,2);
    float last  = rogue_hitbox_projectile_spawn_angle(&boxes[3].u.proj,4);
    const float PI_F=3.14159265358979323846f;
    assert(fabsf(first + PI_F*0.5f) < 1e-4f);
    assert(fabsf(mid - 0.0f) < 1e-4f);
    assert(fabsf(last - PI_F*0.5f) < 1e-4f);
    /* Broadphase + narrow-phase test with capsule (0,0)-(2,0) r=0.5 from sample (boxes[0]) */
    const RogueHitbox* cap = &boxes[0];
    float xs[8] = {0, 1, 2, 1.5f, -0.2f, 0.5f,  1.0f, 3.0f};
    float ys[8] = {0, 0, 0, 0.49f,  0.0f, 0.51f, 0.6f, 0.0f};
    int alive[8] = {1,1,1,1,1,1,1,1};
    int idx[8]; int n = rogue_hitbox_collect_point_overlaps(cap,xs,ys,alive,8,idx,8);
    /* Points inside: indices 0,1,2,3 (y=0.49 within radius), others out (0.51 outside, 0.6 outside, x=3 outside) */
    assert(n==4);
    int seen[8]={0}; for(int i=0;i<n;i++){ seen[idx[i]]=1; }
    assert(seen[0]&&seen[1]&&seen[2]&&seen[3]);
    printf("phase5_hitbox_authoring_broadphase: OK\n");
    return 0;
}
