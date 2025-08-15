/* hot_reload.c - Phase M3.3 hot reload infrastructure (Partial)
 * Current functionality: registration + manual force trigger.
 * Future increments: timestamp tracking per entry + automatic detection.
 */
#include "util/hot_reload.h"
#include <string.h>

#ifndef ROGUE_HOT_RELOAD_CAP
#define ROGUE_HOT_RELOAD_CAP 64
#endif

typedef struct RogueHotReloadEntry {
    char id[64];
    char path[256];
    RogueHotReloadFn fn;
    void* user_data;
    /* future: store last modification timestamp */
} RogueHotReloadEntry;

static RogueHotReloadEntry g_entries[ROGUE_HOT_RELOAD_CAP];
static int g_entry_count = 0;

void rogue_hot_reload_reset(void){ g_entry_count = 0; }

static int find_index(const char* id){ if(!id) return -1; for(int i=0;i<g_entry_count;i++){ if(strcmp(g_entries[i].id,id)==0) return i; } return -1; }

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
    e->fn = fn; e->user_data = user_data;
    return 0;
}

int rogue_hot_reload_force(const char* id){
    int idx = find_index(id); if(idx<0) return -1; RogueHotReloadEntry* e=&g_entries[idx]; e->fn(e->path, e->user_data); return 0;
}

int rogue_hot_reload_tick(void){
    /* Placeholder: timestamp polling not yet implemented. */
    return 0;
}
