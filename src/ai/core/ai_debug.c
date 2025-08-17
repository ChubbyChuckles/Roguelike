#include "ai/core/ai_debug.h"
#include "ai/core/behavior_tree.h"
#include "ai/core/blackboard.h"
#include "ai/perception/perception.h"
#include "ai/core/ai_trace.h"
#include "util/determinism.h"
#include <stdio.h>
#include <string.h>

/* 10.1 Behavior tree visualization */
static void viz_rec(RogueBTNode* node, char* out, int cap, int depth, int* written){
    if(!node || *written >= cap) return; char line[128];
    int len = snprintf(line,sizeof line,"%*s- %s\n", depth*2, "", node->debug_name?node->debug_name:"?");
    if(*written + len < cap){ memcpy(out+*written,line,len); *written += len; }
    for(uint16_t i=0;i<node->child_count;i++) viz_rec(node->children[i],out,cap,depth+1,written);
}
int rogue_ai_bt_visualize(struct RogueBehaviorTree* tree, char* out, int cap){ if(!tree||!out||cap<=0) return -1; int w=0; viz_rec(tree->root,out,cap,0,&w); if(w>=cap) w=cap-1; out[w]='\0'; return w; }

/* 10.3 Blackboard inspector */
int rogue_ai_blackboard_dump(struct RogueBlackboard* bb, char* out, int cap){ if(!bb||!out||cap<=0) return -1; int w=0; for(int i=0;i<bb->count;i++){ RogueBBEntry* e=&bb->entries[i]; char line[160]; switch(e->type){ case ROGUE_BB_INT: snprintf(line,sizeof line,"%s=%d\n",e->key,e->v.i); break; case ROGUE_BB_FLOAT: snprintf(line,sizeof line,"%s=%.3f\n",e->key,e->v.f); break; case ROGUE_BB_BOOL: snprintf(line,sizeof line,"%s=%s\n",e->key,e->v.b?"true":"false"); break; case ROGUE_BB_PTR: snprintf(line,sizeof line,"%s=%p\n",e->key,e->v.p); break; case ROGUE_BB_VEC2: snprintf(line,sizeof line,"%s=(%.2f,%.2f)\n",e->key,e->v.v2.x,e->v.v2.y); break; case ROGUE_BB_TIMER: snprintf(line,sizeof line,"%s=timer(%.2f)\n",e->key,e->v.timer); break; default: snprintf(line,sizeof line,"%s=?\n",e->key); }
        int len=(int)strlen(line); if(w+len<cap){ memcpy(out+w,line,len); w+=len; } else break; }
    if(w>=cap) w=cap-1; out[w]='\0'; return w; }

/* 10.2 Perception overlay primitives */
int rogue_ai_perception_collect_debug(const RoguePerceptionAgent* a, float player_x, float player_y, RogueAIDebugPrimitive* out, int max, float fov_deg, float vision_dist){ if(!a||!out||max<=0) return 0; (void)fov_deg; (void)player_x; (void)player_y; int count=0; // simplified; emit facing line only
    if(count<max){ out[count].ax=a->x; out[count].ay=a->y; out[count].bx=a->x + a->facing_x*vision_dist; out[count].by=a->y + a->facing_y*vision_dist; out[count].kind=ROGUE_AI_DEBUG_PRIM_FOV_CONE; count++; }
    // LOS ray to player
    if(count<max){ out[count].ax=a->x; out[count].ay=a->y; out[count].bx=player_x; out[count].by=player_y; out[count].kind=ROGUE_AI_DEBUG_PRIM_LOS_RAY; count++; }
    return count; }

/* 10.4 Trace export JSON */
int rogue_ai_trace_export_json(const struct RogueAITraceBuffer* tb, char* out, int cap){ if(!tb||!out||cap<=0) return -1; int w=0; if(w<cap) out[w++]='['; for(int i=0;i<tb->count;i++){ int idx = ( (int)tb->cursor - tb->count + i ); if(idx<0) idx += ROGUE_AI_TRACE_CAP; const RogueAITraceEntry* e=&tb->entries[idx]; char buf[64]; int len=snprintf(buf,sizeof buf,"%s{\"tick\":%u,\"hash\":%u}", i?",":"", e->tick_index, e->hash); if(w+len<cap){ memcpy(out+w,buf,len); w+=len; } else break; } if(w<cap) out[w++]=']'; if(w>=cap) w=cap-1; out[w]='\0'; return w; }

/* 10.5 Determinism verifier */
static uint32_t path_hash(RogueBehaviorTree* t){ char buf[256]; int n=rogue_behavior_tree_serialize_active_path(t,buf,sizeof buf); if(n<0) return 0; // simple FNV1a32
    uint32_t h=2166136261u; for(int i=0;i<n;i++){ h^=(unsigned char)buf[i]; h*=16777619u; } return h; }
int rogue_ai_determinism_verify(RogueAIBTFactory factory, int ticks, uint64_t* out_hash){ if(!factory||ticks<=0) return 0; RogueBehaviorTree* a=factory(); RogueBehaviorTree* b=factory(); if(!a||!b) return 0; uint64_t accumA=0, accumB=0; for(int i=0;i<ticks;i++){ rogue_behavior_tree_tick(a,NULL,0.016f); rogue_behavior_tree_tick(b,NULL,0.016f); uint32_t ha=path_hash(a); uint32_t hb=path_hash(b); accumA = rogue_fnv1a64(&ha,sizeof ha,accumA?accumA:0xcbf29ce484222325ULL); accumB = rogue_fnv1a64(&hb,sizeof hb,accumB?accumB:0xcbf29ce484222325ULL); if(ha!=hb){ rogue_behavior_tree_destroy(a); rogue_behavior_tree_destroy(b); if(out_hash) *out_hash=0; return 0; } }
    if(out_hash) *out_hash=accumA; rogue_behavior_tree_destroy(a); rogue_behavior_tree_destroy(b); return 1; }
