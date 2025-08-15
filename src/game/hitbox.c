#include "game/hitbox.h"
#include <math.h>

/* Normalize angle to (-pi, pi] for robust arc comparisons */
static float norm_angle(float a){ while(a <= -3.14159265f) a += 6.28318530f; while(a > 3.14159265f) a -= 6.28318530f; return a; }

static int point_in_capsule(const RogueHitboxCapsule* c, float x,float y){
    float ax=c->ax, ay=c->ay, bx=c->bx, by=c->by; float vx=bx-ax, vy=by-ay; float wx=x-ax, wy=y-ay;
    float seg_len2 = vx*vx + vy*vy;
    float t = (seg_len2>0)? (wx*vx + wy*vy)/seg_len2 : 0.0f; if(t<0) t=0; if(t>1) t=1;
    float cx = ax + vx*t; float cy = ay + vy*t; float dx = x-cx; float dy = y-cy; float d2 = dx*dx + dy*dy;
    return d2 <= c->radius * c->radius ? 1:0;
}

static int point_in_arc(const RogueHitboxArc* a, float x,float y){
    float dx = x - a->ox; float dy = y - a->oy; float r2 = dx*dx + dy*dy; float r = (float)sqrt(r2);
    if(r > a->radius || r < a->inner_radius) return 0;
    float ang = atan2f(dy,dx); ang = norm_angle(ang);
    float a0 = norm_angle(a->angle_start); float a1 = norm_angle(a->angle_end);
    /* Support wrapping: if interval crosses pi boundary, adjust logic */
    if(a0 <= a1){ return (ang >= a0 && ang <= a1)?1:0; }
    else { return (ang >= a0 || ang <= a1)?1:0; }
}

static int point_in_chain(const RogueHitboxChain* ch, float x,float y){
    if(ch->count <= 0) return 0; if(ch->count==1){ /* treat as circle */
        float dx=x - ch->px[0]; float dy=y - ch->py[0]; return (dx*dx+dy*dy) <= ch->radius*ch->radius; }
    for(int i=0;i<ch->count-1;i++){
        RogueHitboxCapsule segment = { ch->px[i], ch->py[i], ch->px[i+1], ch->py[i+1], ch->radius };
        if(point_in_capsule(&segment,x,y)) return 1;
    }
    return 0;
}

int rogue_hitbox_point_overlap(const RogueHitbox* h, float x, float y){
    if(!h) return 0;
    switch(h->type){
        case ROGUE_HITBOX_CAPSULE: return point_in_capsule(&h->u.capsule,x,y);
        case ROGUE_HITBOX_ARC: return point_in_arc(&h->u.arc,x,y);
        case ROGUE_HITBOX_CHAIN: return point_in_chain(&h->u.chain,x,y);
        case ROGUE_HITBOX_PROJECTILE_SPAWN: return 0; /* purely descriptive */
        default: return 0;
    }
}

float rogue_hitbox_projectile_spawn_angle(const RogueHitboxProjectileSpawn* ps, int index){
    if(!ps || index<0 || index >= ps->projectile_count || ps->projectile_count<=0) return 0.0f;
    if(ps->projectile_count==1) return ps->angle_center;
    float half = 0.5f * ps->spread_radians;
    float step = (ps->projectile_count>1)? (ps->spread_radians / (ps->projectile_count - 1)) : 0.0f;
    return ps->angle_center - half + step * index;
}
