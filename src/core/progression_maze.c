#include "core/progression_maze.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

static unsigned int xrng_pm(unsigned int* s){ unsigned int x=*s; x^=x<<13; x^=x>>17; x^=x<<5; *s=x; return x; }
static float frand_pm(unsigned int* s){ return (float)(xrng_pm(s) / (double)0xFFFFFFFFu); }

int rogue_progression_maze_build(const char* config_path, RogueProgressionMaze* out_maze){
    if(!out_maze) return 0; memset(out_maze,0,sizeof *out_maze);
    if(!rogue_skill_maze_generate(config_path, &out_maze->base)) return 0;
    int n = out_maze->base.node_count; int e = out_maze->base.edge_count; if(n<=0){ rogue_progression_maze_free(out_maze); return 0; }
    out_maze->meta = (RogueProgressionMazeNodeMeta*)malloc(sizeof(RogueProgressionMazeNodeMeta)* (size_t)n);
    if(!out_maze->meta){ rogue_progression_maze_free(out_maze); return 0; }
    /* Build temporary adjacency counts */
    int* deg=(int*)calloc((size_t)n,sizeof(int)); if(!deg){ rogue_progression_maze_free(out_maze); return 0; }
    for(int i=0;i<e;i++){ int a=out_maze->base.edges[i].from; int b=out_maze->base.edges[i].to; if((unsigned)a<(unsigned)n && (unsigned)b<(unsigned)n){ deg[a]++; deg[b]++; } }
    /* Procedural optional branch augmentation: add up to 5% new leaf nodes connecting to random high-ring node */
    unsigned int rs=0xC0FFEEu; int add_target = n/20; if(add_target<1) add_target=1; int created=0; int max_new = add_target;
    if(max_new>0){ int new_total = n + max_new; RogueSkillMazeNode* nn = (RogueSkillMazeNode*)realloc(out_maze->base.nodes, sizeof(RogueSkillMazeNode)*(size_t)new_total); if(nn){ out_maze->base.nodes=nn; }
        for(int k=0;k<max_new;k++){ int anchor=-1; int tries=16; while(tries--){ int cand=(int)(xrng_pm(&rs)% (unsigned)n); if(out_maze->base.nodes[cand].ring >= out_maze->base.rings-1){ anchor=cand; break; } }
            if(anchor<0) anchor = (int)(xrng_pm(&rs)% (unsigned)n); int idx = out_maze->base.node_count; out_maze->base.nodes[idx].x = out_maze->base.nodes[anchor].x + frand_pm(&rs)*40.0f - 20.0f; out_maze->base.nodes[idx].y = out_maze->base.nodes[anchor].y + frand_pm(&rs)*40.0f - 20.0f; out_maze->base.nodes[idx].ring = out_maze->base.nodes[anchor].ring; out_maze->base.nodes[idx].a=-1; out_maze->base.nodes[idx].b=-1; out_maze->base.node_count++; /* grow edge array */
            RogueSkillMazeEdge* ne = (RogueSkillMazeEdge*)realloc(out_maze->base.edges, sizeof(RogueSkillMazeEdge)*(size_t)(out_maze->base.edge_count+1)); if(ne){ out_maze->base.edges=ne; out_maze->base.edges[out_maze->base.edge_count].from=anchor; out_maze->base.edges[out_maze->base.edge_count].to=idx; out_maze->base.edge_count++; deg = (int*)realloc(deg, sizeof(int)*(size_t)out_maze->base.node_count); if(deg){ deg[anchor]++; deg[idx]=1; } } created++; }
        n = out_maze->base.node_count; e = out_maze->base.edge_count;
    }
    /* Build adjacency array */
    int total=0; for(int i=0;i<n;i++) total+=deg[i]; out_maze->adjacency=(int*)malloc(sizeof(int)*(size_t)total); if(!out_maze->adjacency){ free(deg); rogue_progression_maze_free(out_maze); return 0; }
    out_maze->total_adjacency=total; int* offsets=(int*)malloc(sizeof(int)* (size_t)n); if(!offsets){ free(deg); rogue_progression_maze_free(out_maze); return 0; }
    int acc=0; for(int i=0;i<n;i++){ offsets[i]=acc; acc+=deg[i]; }
    int* fill=(int*)calloc((size_t)n,sizeof(int)); if(!fill){ free(offsets); free(deg); rogue_progression_maze_free(out_maze); return 0; }
    for(int i=0;i<e;i++){ int a=out_maze->base.edges[i].from; int b=out_maze->base.edges[i].to; if((unsigned)a<(unsigned)n && (unsigned)b<(unsigned)n){ out_maze->adjacency[offsets[a]+fill[a]++]=b; out_maze->adjacency[offsets[b]+fill[b]++]=a; } }
    /* Populate meta */
    for(int i=0;i<n;i++){
        RogueProgressionMazeNodeMeta* m=&out_maze->meta[i]; m->node_id=i; m->ring=out_maze->base.nodes[i].ring; if(m->ring<1) m->ring=1; m->level_req=m->ring*5; /* simple heuristic */
        m->str_req= (m->ring>=3)? (m->ring*2):0; m->dex_req=(m->ring>=2)? (m->ring*2 -2):0; m->int_req=(m->ring>=4)? (m->ring*2 -4):0; m->vit_req=(m->ring>=5)? (m->ring*2 -6):0;
        /* cost ramp: base 1 then +1 every two rings */
        m->cost_points = 1 + (m->ring-1)/2; m->tags=0; m->flags=0; m->adj_start=offsets[i]; m->adj_count=deg[i];
    }
    /* Flag optional leaf nodes (degree==1 and ring>= base.rings-1) */
    int optional=0; for(int i=0;i<n;i++){ if(deg[i]==1 && out_maze->base.nodes[i].ring >= out_maze->base.rings-1){ out_maze->meta[i].flags |= ROGUE_MAZE_FLAG_OPTIONAL; optional++; } }
    /* Difficulty tagging: nodes with degree>4 in inner 50% rings get high difficulty flag */
    int half_ring = out_maze->base.rings/2 + 1; for(int i=0;i<n;i++){ if(out_maze->meta[i].ring <= half_ring && deg[i]>4){ out_maze->meta[i].flags |= ROGUE_MAZE_FLAG_DIFFICULTY; } }
    /* Keystone heuristic (Phase 7 scaffolding): Nodes that are articulation-like (high degree >=5) on outer 30% rings become keystones. */
    int ring_threshold = (int)(out_maze->base.rings * 0.70f); if(ring_threshold<1) ring_threshold=1;
    for(int i=0;i<n;i++){
        int ring = out_maze->meta[i].ring; int degree = deg[i];
        if(ring >= ring_threshold && degree >=5){ out_maze->meta[i].flags |= ROGUE_MAZE_FLAG_KEYSTONE; }
    }
    out_maze->optional_nodes=optional; free(offsets); free(fill); free(deg); return 1; }

void rogue_progression_maze_free(RogueProgressionMaze* m){ if(!m) return; rogue_skill_maze_free(&m->base); free(m->meta); free(m->adjacency); m->meta=NULL; m->adjacency=NULL; m->optional_nodes=0; m->total_adjacency=0; }

int rogue_progression_maze_node_unlockable(const RogueProgressionMaze* m, int node_id, int level,int str,int dex,int intel,int vit){ if(!m||!m->meta||node_id<0||node_id>=m->base.node_count) return 0; const RogueProgressionMazeNodeMeta* meta=&m->meta[node_id]; if(level < meta->level_req) return 0; if(str < meta->str_req) return 0; if(dex < meta->dex_req) return 0; if(intel < meta->int_req) return 0; if(vit < meta->vit_req) return 0; return 1; }

int rogue_progression_maze_shortest_cost(const RogueProgressionMaze* m, int from_node, int to_node){ if(!m||!m->meta) return -1; int n=m->base.node_count; if(from_node<0||from_node>=n||to_node<0||to_node>=n) return -1; if(from_node==to_node) return 0; int* dist=(int*)malloc(sizeof(int)*(size_t)n); unsigned char* vis=(unsigned char*)calloc((size_t)n,1); if(!dist||!vis){ free(dist); free(vis); return -1; } for(int i=0;i<n;i++) dist[i]=INT_MAX; dist[from_node]=0; for(int iter=0; iter<n; ++iter){ int u=-1; int best=INT_MAX; for(int i=0;i<n;i++) if(!vis[i] && dist[i]<best){ best=dist[i]; u=i; } if(u<0) break; if(u==to_node) break; vis[u]=1; const RogueProgressionMazeNodeMeta* mu=&m->meta[u]; for(int k=0;k<mu->adj_count;k++){ int v=m->adjacency[mu->adj_start+k]; if(v<0||v>=n) continue; if(vis[v]) continue; int w = m->meta[v].cost_points; if(dist[u]!=INT_MAX && dist[u]+w < dist[v]) dist[v]=dist[u]+w; } }
    int out = dist[to_node]==INT_MAX? -1: dist[to_node]; free(dist); free(vis); return out; }

int rogue_progression_maze_orphan_count(const RogueProgressionMaze* m){ if(!m||!m->meta) return -1; int n=m->base.node_count; int orphans=0; for(int i=1;i<n;i++){ const RogueProgressionMazeNodeMeta* meta=&m->meta[i]; if(meta->adj_count==0 && !(meta->flags & 1u)) orphans++; } return orphans; }

int rogue_progression_maze_is_keystone(const RogueProgressionMaze* m, int node_id){ if(!m||!m->meta||node_id<0||node_id>=m->base.node_count) return 0; return (m->meta[node_id].flags & ROGUE_MAZE_FLAG_KEYSTONE)?1:0; }
int rogue_progression_maze_keystone_total(const RogueProgressionMaze* m){ if(!m||!m->meta) return 0; int n=m->base.node_count; int c=0; for(int i=0;i<n;i++) if(m->meta[i].flags & ROGUE_MAZE_FLAG_KEYSTONE) c++; return c; }

int rogue_progression_ring_expansions_unlocked(int player_level){ /* simple milestone: +1 ring every 25 levels starting at 50 (Phase 7 baseline) */
    if(player_level < 50) return 0; int extra = (player_level - 50)/25 + 1; if(extra>4) extra=4; return extra; }
