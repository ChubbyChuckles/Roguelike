/* hot_reload.c - Phase M3.3 hot reload infrastructure (Complete)
 * Functionality: registration + manual force trigger + automatic change
 * detection via content hash (FNV-1a 64) each tick.
 */
#include "util/hot_reload.h"
#include <string.h>
#include <stdio.h>

#ifndef ROGUE_HOT_RELOAD_CAP
#define ROGUE_HOT_RELOAD_CAP 64
#endif

typedef struct RogueHotReloadEntry {
    char id[64];
    char path[256];
    RogueHotReloadFn fn;
    void* user_data;
    unsigned long long last_hash; /* 0 means unknown/not yet hashed */
} RogueHotReloadEntry;

static RogueHotReloadEntry g_entries[ROGUE_HOT_RELOAD_CAP];
static int g_entry_count = 0;

void rogue_hot_reload_reset(void){ g_entry_count = 0; }

static int find_index(const char* id){ if(!id) return -1; for(int i=0;i<g_entry_count;i++){ if(strcmp(g_entries[i].id,id)==0) return i; } return -1; }

static unsigned long long hash_file(const char* path){
    FILE* f=NULL; unsigned long long h=1469598103934665603ull; /* FNV-1a 64 offset */
#if defined(_MSC_VER)
    fopen_s(&f,path,"rb");
#else
    f=fopen(path,"rb");
#endif
    if(!f) return 0ull; unsigned char buf[1024]; size_t n;
    while((n=fread(buf,1,sizeof buf,f))>0){ for(size_t i=0;i<n;i++){ h ^= (unsigned long long)buf[i]; h *= 1099511628211ull; }}
    fclose(f); return h;
}

int rogue_hot_reload_register(const char* id, const char* path, RogueHotReloadFn fn, void* user_data){
    if(!id || !*id || !path || !fn) return -1;
    if(find_index(id)>=0) return -1; /* duplicate */
    if(g_entry_count >= ROGUE_HOT_RELOAD_CAP) return -1;
    RogueHotReloadEntry* e = &g_entries[g_entry_count++];
#if defined(_MSC_VER)
    strncpy_s(e->id,sizeof e->id,id,_TRUNCATE);
    strncpy_s(e->path,sizeof e->path,path,_TRUNCATE);
#else
    strncpy(e->id,id,sizeof e->id -1); e->id[sizeof e->id -1]='\0';
    strncpy(e->path,path,sizeof e->path -1); e->path[sizeof e->path -1]='\0';
#endif
    e->fn = fn; e->user_data = user_data; e->last_hash = hash_file(e->path);
    return 0;
}

int rogue_hot_reload_force(const char* id){
    int idx = find_index(id); if(idx<0) return -1; RogueHotReloadEntry* e=&g_entries[idx]; e->fn(e->path, e->user_data); return 0;
}

int rogue_hot_reload_tick(void){
    int fired=0; for(int i=0;i<g_entry_count;i++){
        RogueHotReloadEntry* e=&g_entries[i];
        unsigned long long h = hash_file(e->path);
        if(h!=0ull && h!=e->last_hash){ e->last_hash = h; e->fn(e->path, e->user_data); fired++; }
    }
    return fired;
}
