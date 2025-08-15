/* projectiles_config.c - Phase M3.5 projectile & impact tuning */
#include "core/projectiles_config.h"
#include "util/kv_parser.h"
#include "util/hot_reload.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static RogueProjectileTuning g_tuning = { 260.0f, 10, 6, 340.0f, 120.0f, 2.5f, 3.5f, 4.0f, 3.0f, 0.2f };
static char g_watch_path[256];
static int g_registered = 0;

static void apply_entry(const char* k, const char* v){ if(!k||!v) return; 
    if(strcmp(k,"IMPACT_LIFE_MS")==0) g_tuning.impact_life_ms = (float)atof(v);
    else if(strcmp(k,"SHARD_COUNT_HIT")==0) g_tuning.shard_count_hit = atoi(v);
    else if(strcmp(k,"SHARD_COUNT_EXPIRE")==0) g_tuning.shard_count_expire = atoi(v);
    else if(strcmp(k,"SHARD_LIFE_MIN_MS")==0) g_tuning.shard_life_min_ms = (float)atof(v);
    else if(strcmp(k,"SHARD_LIFE_VAR_MS")==0) g_tuning.shard_life_var_ms = (float)atof(v);
    else if(strcmp(k,"SHARD_SPEED_MIN")==0) g_tuning.shard_speed_min = (float)atof(v);
    else if(strcmp(k,"SHARD_SPEED_VAR")==0) g_tuning.shard_speed_var = (float)atof(v);
    else if(strcmp(k,"SHARD_SIZE_MIN")==0) g_tuning.shard_size_min = (float)atof(v);
    else if(strcmp(k,"SHARD_SIZE_VAR")==0) g_tuning.shard_size_var = (float)atof(v);
    else if(strcmp(k,"GRAVITY")==0) g_tuning.shard_gravity = (float)atof(v);
}

int rogue_projectiles_config_load(const char* path){
    RogueKVFile kv;
    if(!rogue_kv_load_file(path,&kv)) return -1;
    int cur=0; RogueKVEntry ent; RogueKVError err;
    while(rogue_kv_next(&kv,&cur,&ent,&err)){
        apply_entry(ent.key, ent.value);
    }
    rogue_kv_free(&kv);
    return 0;
}
static void reload_cb(const char* path, void* user){ (void)user; rogue_projectiles_config_load(path); }
int rogue_projectiles_config_load_and_watch(const char* path){ int r=rogue_projectiles_config_load(path); if(r==0){ if(!g_registered){
#if defined(_MSC_VER)
    strncpy_s(g_watch_path,sizeof g_watch_path,path,_TRUNCATE);
#else
    strncpy(g_watch_path,path,sizeof g_watch_path -1); g_watch_path[sizeof g_watch_path -1]='\0';
#endif
    if(rogue_hot_reload_register("projectiles_cfg", g_watch_path, reload_cb, NULL)==0){ g_registered=1; }
 }} return r; }
const RogueProjectileTuning* rogue_projectiles_tuning(void){ return &g_tuning; }
void rogue_projectiles_config_reset(void){ g_tuning = (RogueProjectileTuning){ 260.0f, 10, 6, 340.0f, 120.0f, 2.5f, 3.5f, 4.0f, 3.0f, 0.2f }; }
int rogue_projectiles_spawn_test_shards(int hit); /* forward - implemented in projectiles_update.c */
