/* asset_dep.c - Phase M3.4 asset dependency graph & hashing */
#include "util/asset_dep.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef ROGUE_ASSET_DEP_CAP
#define ROGUE_ASSET_DEP_CAP 256
#endif
#ifndef ROGUE_ASSET_DEP_MAX_DEPS
#define ROGUE_ASSET_DEP_MAX_DEPS 16
#endif

typedef struct RogueAssetDepNode {
    char id[64];
    char path[256];
    int dep_indices[ROGUE_ASSET_DEP_MAX_DEPS];
    int dep_count;
    unsigned long long cached_hash; /* 0 = unknown */
    unsigned char visiting; /* DFS cycle detection */
} RogueAssetDepNode;

static RogueAssetDepNode g_nodes[ROGUE_ASSET_DEP_CAP];
static int g_node_count=0;

void rogue_asset_dep_reset(void){ g_node_count=0; }

static int find_node(const char* id){ if(!id) return -1; for(int i=0;i<g_node_count;i++){ if(strcmp(g_nodes[i].id,id)==0) return i; } return -1; }

static unsigned long long hash_file(const char* path){
    if(!path || !*path) return 1469598103934665603ull; /* empty virtual node base */
    FILE* f=NULL; unsigned long long h=1469598103934665603ull;
#if defined(_MSC_VER)
    fopen_s(&f,path,"rb");
#else
    f=fopen(path,"rb");
#endif
    if(!f) return h; unsigned char buf[1024]; size_t n; while((n=fread(buf,1,sizeof buf,f))>0){ for(size_t i=0;i<n;i++){ h ^= (unsigned long long)buf[i]; h *= 1099511628211ull; }} fclose(f); return h;
}

static unsigned long long combine_hash(unsigned long long cur, unsigned long long child){ cur ^= child; cur *= 1099511628211ull; return cur; }

static int dfs_cycle_check(int idx){ RogueAssetDepNode* n=&g_nodes[idx]; if(n->visiting==1) return -1; if(n->visiting==2) return 0; n->visiting=1; for(int i=0;i<n->dep_count;i++){ if(dfs_cycle_check(n->dep_indices[i])<0) return -1; } n->visiting=2; return 0; }

int rogue_asset_dep_register(const char* id, const char* path, const char** deps, int dep_count){
    if(!id || !*id || dep_count<0) return -3; if(find_node(id)>=0) return -1; if(g_node_count>=ROGUE_ASSET_DEP_CAP) return -3; if(dep_count>ROGUE_ASSET_DEP_MAX_DEPS) dep_count=ROGUE_ASSET_DEP_MAX_DEPS;
    RogueAssetDepNode* n=&g_nodes[g_node_count];
#if defined(_MSC_VER)
    strncpy_s(n->id,sizeof n->id,id,_TRUNCATE); if(path) strncpy_s(n->path,sizeof n->path,path,_TRUNCATE); else n->path[0]='\0';
#else
    strncpy(n->id,id,sizeof n->id -1); n->id[sizeof n->id -1]='\0'; if(path){ strncpy(n->path,path,sizeof n->path -1); n->path[sizeof n->path -1]='\0'; } else n->path[0]='\0';
#endif
    n->dep_count=0; n->cached_hash=0; n->visiting=0; for(int i=0;i<dep_count;i++){ n->dep_indices[i]=-1; }
    g_node_count++;
    for(int i=0;i<dep_count;i++){ int di=find_node(deps[i]); if(di>=0){ n->dep_indices[n->dep_count++]=di; }}
    for(int i=0;i<g_node_count;i++){ g_nodes[i].visiting=0; }
    for(int i=0;i<g_node_count;i++){ if(dfs_cycle_check(i)<0){ g_nodes[g_node_count-1].id[0]='\0'; g_node_count--; return -2; }}
    return 0;
}

int rogue_asset_dep_invalidate(const char* id){ int idx=find_node(id); if(idx<0) return -1; g_nodes[idx].cached_hash=0; return 0; }

static unsigned long long compute_hash(int idx){ RogueAssetDepNode* n=&g_nodes[idx]; if(n->cached_hash) return n->cached_hash; unsigned long long h=hash_file(n->path); for(int i=0;i<n->dep_count;i++){ h=combine_hash(h, compute_hash(n->dep_indices[i])); } n->cached_hash=h; return h; }

int rogue_asset_dep_hash(const char* id, unsigned long long* out_hash){ int idx=find_node(id); if(idx<0||!out_hash) return -1; *out_hash=compute_hash(idx); return 0; }
