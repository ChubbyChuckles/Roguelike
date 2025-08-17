#include "advanced_nodes.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Helper: allocate child array if needed */
static int ensure_child_array(RogueBTNode* n){
    if(!n) return 0; if(n->children) return 1; if(n->child_capacity==0) n->child_capacity=4; n->children=(RogueBTNode**)calloc(n->child_capacity,sizeof(RogueBTNode*)); return n->children!=NULL; }

/* Parallel */
static RogueBTStatus tick_parallel(RogueBTNode* node, RogueBlackboard* bb, float dt){
    int any_running=0; for(uint16_t i=0;i<node->child_count;i++){ RogueBTNode* c=node->children[i]; RogueBTStatus st=c->vtable->tick(c,bb,dt); if(st==ROGUE_BT_FAILURE) return ROGUE_BT_FAILURE; if(st==ROGUE_BT_RUNNING) any_running=1; }
    return any_running?ROGUE_BT_RUNNING:ROGUE_BT_SUCCESS; }
RogueBTNode* rogue_bt_parallel(const char* name){ return rogue_bt_node_create(name,2,tick_parallel); }

/* Utility Selector */
typedef struct UtilityChildMeta { RogueUtilityScorer scorer; } UtilityChildMeta;
typedef struct UtilitySelectorData { UtilityChildMeta* metas; } UtilitySelectorData;
static RogueBTStatus tick_utility_selector(RogueBTNode* node, RogueBlackboard* bb, float dt){
    if(!node) return ROGUE_BT_FAILURE; UtilitySelectorData* d=(UtilitySelectorData*)node->user_data; if(!d) return ROGUE_BT_FAILURE; if(node->child_count==0) return ROGUE_BT_FAILURE; float best=-1e30f; int best_i=-1; for(uint16_t i=0;i<node->child_count;i++){ float s=0.0f; if(d->metas && d->metas[i].scorer.fn) s=d->metas[i].scorer.fn(bb,d->metas[i].scorer.user_data); if(s>best){ best=s; best_i=i; } }
    if(best_i<0) return ROGUE_BT_FAILURE; return node->children[best_i]->vtable->tick(node->children[best_i],bb,dt); }
RogueBTNode* rogue_bt_utility_selector(const char* name){ RogueBTNode* n=rogue_bt_node_create(name,2,tick_utility_selector); if(n){ UtilitySelectorData* d=(UtilitySelectorData*)calloc(1,sizeof(UtilitySelectorData)); n->user_data=d; } return n; }
int rogue_bt_utility_set_child_scorer(RogueBTNode* utility_node, RogueBTNode* child, RogueUtilityScorer scorer){
    if(!utility_node||!child) return 0; if(utility_node->vtable->tick != tick_utility_selector) return 0;
    UtilitySelectorData* d=(UtilitySelectorData*)utility_node->user_data; if(!d) return 0;
    if(!rogue_bt_node_add_child(utility_node, child)) return 0;
    /* Resize metas to match child_capacity */
    if(!d->metas){ d->metas=(UtilityChildMeta*)calloc(utility_node->child_capacity,sizeof(UtilityChildMeta)); }
    else if(utility_node->child_capacity > 0){ d->metas=(UtilityChildMeta*)realloc(d->metas, utility_node->child_capacity * sizeof(UtilityChildMeta)); }
    d->metas[utility_node->child_count-1].scorer = scorer;
    return 1;
}

/* Custom destroy hook for nodes with allocated user_data */
void rogue_bt_advanced_cleanup(RogueBTNode* node){
    if(!node) return;
    if(!node->user_data) return;
    if(node->vtable && node->vtable->tick==tick_utility_selector){
        UtilitySelectorData* d=(UtilitySelectorData*)node->user_data;
        if(d){ if(d->metas) free(d->metas); free(d); }
        node->user_data=NULL;
        return;
    }
    /* Other advanced nodes: single allocation */
    free(node->user_data);
    node->user_data=NULL;
}

/* Conditions */
typedef struct CondPlayerVisible { const char* player_pos_key; const char* agent_pos_key; const char* agent_facing_key; float fov_deg; float max_dist; } CondPlayerVisible;
static RogueBTStatus tick_cond_player_visible(RogueBTNode* node, RogueBlackboard* bb, float dt){ (void)dt; CondPlayerVisible* d=(CondPlayerVisible*)node->user_data; RogueBBVec2 player, agent, facing; if(!rogue_bb_get_vec2(bb,d->player_pos_key,&player)) return ROGUE_BT_FAILURE; if(!rogue_bb_get_vec2(bb,d->agent_pos_key,&agent)) return ROGUE_BT_FAILURE; if(!rogue_bb_get_vec2(bb,d->agent_facing_key,&facing)) return ROGUE_BT_FAILURE; RoguePerceptionAgent pa={0}; pa.x=agent.x; pa.y=agent.y; pa.facing_x=facing.x; pa.facing_y=facing.y; return rogue_perception_can_see(&pa,player.x,player.y,d->fov_deg,d->max_dist,NULL)?ROGUE_BT_SUCCESS:ROGUE_BT_FAILURE; }
RogueBTNode* rogue_bt_condition_player_visible(const char* name,const char* bb_player_pos_key,const char* bb_agent_pos_key,const char* bb_agent_facing_key,float fov_deg,float max_dist){ RogueBTNode* n=rogue_bt_node_create(name,0,tick_cond_player_visible); if(!n) return NULL; CondPlayerVisible* d=(CondPlayerVisible*)calloc(1,sizeof(CondPlayerVisible)); d->player_pos_key=bb_player_pos_key; d->agent_pos_key=bb_agent_pos_key; d->agent_facing_key=bb_agent_facing_key; d->fov_deg=fov_deg; d->max_dist=max_dist; n->user_data=d; return n; }

typedef struct CondTimerElapsed { const char* timer_key; float min_value; } CondTimerElapsed;
static RogueBTStatus tick_cond_timer_elapsed(RogueBTNode* node, RogueBlackboard* bb, float dt){ (void)dt; CondTimerElapsed* d=(CondTimerElapsed*)node->user_data; float t=0.0f; if(!rogue_bb_get_timer(bb,d->timer_key,&t)) return ROGUE_BT_FAILURE; return (t >= d->min_value)?ROGUE_BT_SUCCESS:ROGUE_BT_FAILURE; }
RogueBTNode* rogue_bt_condition_timer_elapsed(const char* name,const char* bb_timer_key,float min_value){ RogueBTNode* n=rogue_bt_node_create(name,0,tick_cond_timer_elapsed); if(!n) return NULL; CondTimerElapsed* d=(CondTimerElapsed*)calloc(1,sizeof(CondTimerElapsed)); d->timer_key=bb_timer_key; d->min_value=min_value; n->user_data=d; return n; }

typedef struct CondHealthBelow { const char* health_key; float threshold; } CondHealthBelow;
static RogueBTStatus tick_cond_health_below(RogueBTNode* node,RogueBlackboard* bb,float dt){ (void)dt; CondHealthBelow* d=(CondHealthBelow*)node->user_data; float hp=0.0f; if(!rogue_bb_get_float(bb,d->health_key,&hp)) return ROGUE_BT_FAILURE; return (hp < d->threshold)?ROGUE_BT_SUCCESS:ROGUE_BT_FAILURE; }
RogueBTNode* rogue_bt_condition_health_below(const char* name,const char* bb_health_key,float threshold){ RogueBTNode* n=rogue_bt_node_create(name,0,tick_cond_health_below); if(!n) return NULL; CondHealthBelow* d=(CondHealthBelow*)calloc(1,sizeof(CondHealthBelow)); d->health_key=bb_health_key; d->threshold=threshold; n->user_data=d; return n; }

/* Actions */
typedef struct ActionMoveTo { const char* target_pos_key; const char* agent_pos_key; float speed; const char* reached_flag_key; } ActionMoveTo;
static RogueBTStatus tick_action_move_to(RogueBTNode* node,RogueBlackboard* bb,float dt){ ActionMoveTo* d=(ActionMoveTo*)node->user_data; RogueBBVec2 target, agent; if(!rogue_bb_get_vec2(bb,d->target_pos_key,&target)) return ROGUE_BT_FAILURE; if(!rogue_bb_get_vec2(bb,d->agent_pos_key,&agent)) return ROGUE_BT_FAILURE; float dx=target.x-agent.x, dy=target.y-agent.y; float dist2=dx*dx+dy*dy; if(dist2 < 0.05f){ rogue_bb_set_bool(bb,d->reached_flag_key,true); return ROGUE_BT_SUCCESS; } float dist=sqrtf(dist2); float nx=dx/dist, ny=dy/dist; agent.x += nx * d->speed * dt; agent.y += ny * d->speed * dt; rogue_bb_set_vec2(bb,d->agent_pos_key,agent.x,agent.y); rogue_bb_set_bool(bb,d->reached_flag_key,false); return ROGUE_BT_RUNNING; }
RogueBTNode* rogue_bt_action_move_to(const char* name,const char* bb_target_pos_key,const char* bb_agent_pos_key,float speed,const char* bb_out_reached_flag){ RogueBTNode* n=rogue_bt_node_create(name,0,tick_action_move_to); if(!n) return NULL; ActionMoveTo* d=(ActionMoveTo*)calloc(1,sizeof(ActionMoveTo)); d->target_pos_key=bb_target_pos_key; d->agent_pos_key=bb_agent_pos_key; d->speed=speed; d->reached_flag_key=bb_out_reached_flag; n->user_data=d; return n; }

typedef struct ActionFleeFrom { const char* threat_pos_key; const char* agent_pos_key; float speed; } ActionFleeFrom;
static RogueBTStatus tick_action_flee_from(RogueBTNode* node,RogueBlackboard* bb,float dt){ ActionFleeFrom* d=(ActionFleeFrom*)node->user_data; RogueBBVec2 threat, agent; if(!rogue_bb_get_vec2(bb,d->threat_pos_key,&threat)) return ROGUE_BT_FAILURE; if(!rogue_bb_get_vec2(bb,d->agent_pos_key,&agent)) return ROGUE_BT_FAILURE; float dx=agent.x-threat.x, dy=agent.y-threat.y; float dist2=dx*dx+dy*dy; if(dist2 < 0.0001f){ dx=1; dy=0; dist2=1; } float dist=sqrtf(dist2); agent.x += (dx/dist) * d->speed * dt; agent.y += (dy/dist) * d->speed * dt; rogue_bb_set_vec2(bb,d->agent_pos_key,agent.x,agent.y); return ROGUE_BT_RUNNING; }
RogueBTNode* rogue_bt_action_flee_from(const char* name,const char* bb_threat_pos_key,const char* bb_agent_pos_key,float speed){ RogueBTNode* n=rogue_bt_node_create(name,0,tick_action_flee_from); if(!n) return NULL; ActionFleeFrom* d=(ActionFleeFrom*)calloc(1,sizeof(ActionFleeFrom)); d->threat_pos_key=bb_threat_pos_key; d->agent_pos_key=bb_agent_pos_key; d->speed=speed; n->user_data=d; return n; }

typedef struct ActionAttack { const char* flag_key; const char* cooldown_timer_key; float cooldown; } ActionAttack;
static RogueBTStatus tick_action_attack_melee(RogueBTNode* node,RogueBlackboard* bb,float dt){ (void)dt; ActionAttack* d=(ActionAttack*)node->user_data; bool in_range=false; if(!rogue_bb_get_bool(bb,d->flag_key,&in_range) || !in_range) return ROGUE_BT_FAILURE; /* begin attack: reset cooldown timer */ rogue_bb_set_timer(bb,d->cooldown_timer_key,0.0f); return ROGUE_BT_SUCCESS; }
RogueBTNode* rogue_bt_action_attack_melee(const char* name,const char* bb_in_range_flag_key,const char* bb_cooldown_timer_key,float cooldown_seconds){ RogueBTNode* n=rogue_bt_node_create(name,0,tick_action_attack_melee); if(!n) return NULL; ActionAttack* d=(ActionAttack*)calloc(1,sizeof(ActionAttack)); d->flag_key=bb_in_range_flag_key; d->cooldown_timer_key=bb_cooldown_timer_key; d->cooldown=cooldown_seconds; n->user_data=d; return n; }
static RogueBTStatus tick_action_attack_ranged(RogueBTNode* node,RogueBlackboard* bb,float dt){ (void)dt; ActionAttack* d=(ActionAttack*)node->user_data; bool clear=false; if(!rogue_bb_get_bool(bb,d->flag_key,&clear) || !clear) return ROGUE_BT_FAILURE; rogue_bb_set_timer(bb,d->cooldown_timer_key,0.0f); return ROGUE_BT_SUCCESS; }
RogueBTNode* rogue_bt_action_attack_ranged(const char* name,const char* bb_line_clear_flag_key,const char* bb_cooldown_timer_key,float cooldown_seconds){ RogueBTNode* n=rogue_bt_node_create(name,0,tick_action_attack_ranged); if(!n) return NULL; ActionAttack* d=(ActionAttack*)calloc(1,sizeof(ActionAttack)); d->flag_key=bb_line_clear_flag_key; d->cooldown_timer_key=bb_cooldown_timer_key; d->cooldown=cooldown_seconds; n->user_data=d; return n; }

/* Strafe Action: move perpendicular to vector (target - agent) alternating left/right via bool flag; succeeds after duration */
typedef struct ActionStrafe { const char* target_pos_key; const char* agent_pos_key; const char* left_flag_key; float speed; float duration; float elapsed; int direction; } ActionStrafe;
static RogueBTStatus tick_action_strafe(RogueBTNode* node,RogueBlackboard* bb,float dt){ ActionStrafe* d=(ActionStrafe*)node->user_data; RogueBBVec2 target, agent; if(!rogue_bb_get_vec2(bb,d->target_pos_key,&target)) return ROGUE_BT_FAILURE; if(!rogue_bb_get_vec2(bb,d->agent_pos_key,&agent)) return ROGUE_BT_FAILURE; bool left=false; rogue_bb_get_bool(bb,d->left_flag_key,&left); d->direction = left? -1: 1; float vx=target.x-agent.x, vy=target.y-agent.y; float len=sqrtf(vx*vx+vy*vy); if(len<0.0001f){ vx=1; vy=0; len=1; } vx/=len; vy/=len; /* perpendicular */ float px = -vy * d->direction; float py = vx * d->direction; agent.x += px * d->speed * dt; agent.y += py * d->speed * dt; rogue_bb_set_vec2(bb,d->agent_pos_key,agent.x,agent.y); d->elapsed += dt; if(d->elapsed >= d->duration){ /* flip flag for next time */ rogue_bb_set_bool(bb,d->left_flag_key,!left); return ROGUE_BT_SUCCESS; } return ROGUE_BT_RUNNING; }
RogueBTNode* rogue_bt_action_strafe(const char* name,const char* bb_target_pos_key,const char* bb_agent_pos_key,const char* bb_strafe_left_flag_key,float speed,float duration_seconds){ RogueBTNode* n=rogue_bt_node_create(name,0,tick_action_strafe); if(!n) return NULL; ActionStrafe* d=(ActionStrafe*)calloc(1,sizeof(ActionStrafe)); d->target_pos_key=bb_target_pos_key; d->agent_pos_key=bb_agent_pos_key; d->left_flag_key=bb_strafe_left_flag_key; d->speed=speed; d->duration=duration_seconds; d->elapsed=0.0f; d->direction=1; n->user_data=d; return n; }

/* Tactical Flank Attempt: compute flank point perpendicular to approach vector at distance offset; write to BB vector key; success immediate */
typedef struct TacticalFlank { const char* player_pos_key; const char* agent_pos_key; const char* out_flank_key; float offset; } TacticalFlank;
static RogueBTStatus tick_tactical_flank(RogueBTNode* node,RogueBlackboard* bb,float dt){ (void)dt; TacticalFlank* d=(TacticalFlank*)node->user_data; RogueBBVec2 player, agent; if(!rogue_bb_get_vec2(bb,d->player_pos_key,&player)) return ROGUE_BT_FAILURE; if(!rogue_bb_get_vec2(bb,d->agent_pos_key,&agent)) return ROGUE_BT_FAILURE; float vx=player.x-agent.x, vy=player.y-agent.y; float len=sqrtf(vx*vx+vy*vy); if(len<0.0001f){ vx=1; vy=0; len=1; } vx/=len; vy/=len; /* choose perpendicular based on deterministic sign: prefer left (negative y) */ float px=-vy; float py=vx; float flank_x=player.x + px * d->offset; float flank_y=player.y + py * d->offset; rogue_bb_set_vec2(bb,d->out_flank_key,flank_x,flank_y); return ROGUE_BT_SUCCESS; }
RogueBTNode* rogue_bt_tactical_flank_attempt(const char* name,const char* bb_player_pos_key,const char* bb_agent_pos_key,const char* bb_out_flank_target_key,float offset){ RogueBTNode* n=rogue_bt_node_create(name,0,tick_tactical_flank); if(!n) return NULL; TacticalFlank* d=(TacticalFlank*)calloc(1,sizeof(TacticalFlank)); d->player_pos_key=bb_player_pos_key; d->agent_pos_key=bb_agent_pos_key; d->out_flank_key=bb_out_flank_target_key; d->offset=offset; n->user_data=d; return n; }

/* Tactical Regroup: move toward a regroup point until within small radius then success */
typedef struct TacticalRegroup { const char* regroup_pos_key; const char* agent_pos_key; float speed; } TacticalRegroup;
static RogueBTStatus tick_tactical_regroup(RogueBTNode* node,RogueBlackboard* bb,float dt){ TacticalRegroup* d=(TacticalRegroup*)node->user_data; RogueBBVec2 target, agent; if(!rogue_bb_get_vec2(bb,d->regroup_pos_key,&target)) return ROGUE_BT_FAILURE; if(!rogue_bb_get_vec2(bb,d->agent_pos_key,&agent)) return ROGUE_BT_FAILURE; float dx=target.x-agent.x, dy=target.y-agent.y; float dist2=dx*dx+dy*dy; if(dist2 < 0.04f){ return ROGUE_BT_SUCCESS; } float dist=sqrtf(dist2); agent.x += (dx/dist)*d->speed*dt; agent.y += (dy/dist)*d->speed*dt; rogue_bb_set_vec2(bb,d->agent_pos_key,agent.x,agent.y); return ROGUE_BT_RUNNING; }
RogueBTNode* rogue_bt_tactical_regroup(const char* name,const char* bb_regroup_point_key,const char* bb_agent_pos_key,float speed){ RogueBTNode* n=rogue_bt_node_create(name,0,tick_tactical_regroup); if(!n) return NULL; TacticalRegroup* d=(TacticalRegroup*)calloc(1,sizeof(TacticalRegroup)); d->regroup_pos_key=bb_regroup_point_key; d->agent_pos_key=bb_agent_pos_key; d->speed=speed; n->user_data=d; return n; }

/* Tactical Cover Seek: move to point on obstacle perimeter opposite player line; succeed when reached & flag set */
typedef struct TacticalCoverSeek { const char* player_pos_key; const char* agent_pos_key; const char* obstacle_pos_key; const char* out_cover_point_key; const char* out_flag_key; float obstacle_radius; float speed; int computed; float cover_x, cover_y; } TacticalCoverSeek;
static RogueBTStatus tick_tactical_cover_seek(RogueBTNode* node,RogueBlackboard* bb,float dt){ TacticalCoverSeek* d=(TacticalCoverSeek*)node->user_data; RogueBBVec2 player, agent, obstacle; if(!rogue_bb_get_vec2(bb,d->player_pos_key,&player)) return ROGUE_BT_FAILURE; if(!rogue_bb_get_vec2(bb,d->agent_pos_key,&agent)) return ROGUE_BT_FAILURE; if(!rogue_bb_get_vec2(bb,d->obstacle_pos_key,&obstacle)) return ROGUE_BT_FAILURE; if(!d->computed){ float vx=player.x-obstacle.x, vy=player.y-obstacle.y; float len=sqrtf(vx*vx+vy*vy); if(len<0.0001f){ vx=1; vy=0; len=1; } vx/=len; vy/=len; /* cover point is opposite side of obstacle from player */ d->cover_x = obstacle.x - vx * d->obstacle_radius; d->cover_y = obstacle.y - vy * d->obstacle_radius; rogue_bb_set_vec2(bb,d->out_cover_point_key,d->cover_x,d->cover_y); d->computed=1; }
    float dx=d->cover_x-agent.x, dy=d->cover_y-agent.y; float dist2=dx*dx+dy*dy; if(dist2 < 0.04f){ /* arrival -> check occlusion: line from player to agent intersects obstacle circle? */ float pvx=d->cover_x-player.x, pvy=d->cover_y-player.y; float ovx=obstacle.x-player.x, ovy=obstacle.y-player.y; /* Project obstacle center onto player->cover segment */ float seg_len2 = pvx*pvx+pvy*pvy; if(seg_len2>0){ float t=((ovx*pvx)+(ovy*pvy))/seg_len2; if(t<0) t=0; else if(t>1) t=1; float projx=player.x + pvx*t; float projy=player.y + pvy*t; float cx=obstacle.x-projx, cy=obstacle.y-projy; float dist_c2=cx*cx+cy*cy; if(dist_c2 <= d->obstacle_radius*d->obstacle_radius*1.05f){ rogue_bb_set_bool(bb,d->out_flag_key,true); return ROGUE_BT_SUCCESS; } }
        /* If occlusion check fails treat as failure (no valid cover) */
        return ROGUE_BT_FAILURE; }
    float dist=sqrtf(dist2); agent.x += (dx/dist)*d->speed*dt; agent.y += (dy/dist)*d->speed*dt; rogue_bb_set_vec2(bb,d->agent_pos_key,agent.x,agent.y); return ROGUE_BT_RUNNING; }
RogueBTNode* rogue_bt_tactical_cover_seek(const char* name,const char* bb_player_pos_key,const char* bb_agent_pos_key,const char* bb_obstacle_pos_key,const char* bb_out_cover_point_key,const char* bb_out_in_cover_flag_key,float obstacle_radius,float move_speed){ RogueBTNode* n=rogue_bt_node_create(name,0,tick_tactical_cover_seek); if(!n) return NULL; TacticalCoverSeek* d=(TacticalCoverSeek*)calloc(1,sizeof(TacticalCoverSeek)); d->player_pos_key=bb_player_pos_key; d->agent_pos_key=bb_agent_pos_key; d->obstacle_pos_key=bb_obstacle_pos_key; d->out_cover_point_key=bb_out_cover_point_key; d->out_flag_key=bb_out_in_cover_flag_key; d->obstacle_radius=obstacle_radius; d->speed=move_speed; d->computed=0; d->cover_x=0; d->cover_y=0; n->user_data=d; return n; }

/* Decorators */
typedef struct DecorCooldown { RogueBTNode* child; const char* timer_key; float cooldown; } DecorCooldown; 
static RogueBTStatus tick_decor_cooldown(RogueBTNode* node,RogueBlackboard* bb,float dt){ DecorCooldown* d=(DecorCooldown*)node->user_data; float t=0.0f; if(!rogue_bb_get_timer(bb,d->timer_key,&t)) return ROGUE_BT_FAILURE; if(t < d->cooldown){ /* advance timer */ rogue_bb_set_timer(bb,d->timer_key,t+dt); return ROGUE_BT_FAILURE; } RogueBTStatus st=d->child->vtable->tick(d->child,bb,dt); if(st==ROGUE_BT_SUCCESS){ rogue_bb_set_timer(bb,d->timer_key,0.0f); } else { /* advance timer even if not success to avoid stall */ rogue_bb_set_timer(bb,d->timer_key,t+dt); } return st; }
RogueBTNode* rogue_bt_decorator_cooldown(const char* name,RogueBTNode* child,const char* bb_timer_key,float cooldown_seconds){ RogueBTNode* n=rogue_bt_node_create(name,1,tick_decor_cooldown); if(!n) return NULL; DecorCooldown* d=(DecorCooldown*)calloc(1,sizeof(DecorCooldown)); d->child=child; d->timer_key=bb_timer_key; d->cooldown=cooldown_seconds; n->user_data=d; n->children=(RogueBTNode**)calloc(1,sizeof(RogueBTNode*)); n->children[0]=child; n->child_count=1; return n; }

typedef struct DecorRetry { RogueBTNode* child; int attempts; int max_attempts; } DecorRetry;
static RogueBTStatus tick_decor_retry(RogueBTNode* node,RogueBlackboard* bb,float dt){ DecorRetry* d=(DecorRetry*)node->user_data; RogueBTStatus st=d->child->vtable->tick(d->child,bb,dt); if(st==ROGUE_BT_FAILURE){ d->attempts++; if(d->attempts < d->max_attempts) return ROGUE_BT_RUNNING; else return ROGUE_BT_FAILURE; } d->attempts=0; return st; }
RogueBTNode* rogue_bt_decorator_retry(const char* name,RogueBTNode* child,int max_attempts){ RogueBTNode* n=rogue_bt_node_create(name,1,tick_decor_retry); if(!n) return NULL; DecorRetry* d=(DecorRetry*)calloc(1,sizeof(DecorRetry)); d->child=child; d->attempts=0; d->max_attempts=max_attempts; n->user_data=d; n->children=(RogueBTNode**)calloc(1,sizeof(RogueBTNode*)); n->children[0]=child; n->child_count=1; return n; }
