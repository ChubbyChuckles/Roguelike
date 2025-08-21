#include "rollback_manager.h"
#include "snapshot_manager.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifndef ROGUE_MAX_SYSTEMS
#define ROGUE_MAX_SYSTEMS 64
#endif

#ifndef ROGUE_MAX_ROLLBACK_SNAPSHOTS
#define ROGUE_MAX_ROLLBACK_SNAPSHOTS 16
#endif

typedef struct RollbackEntry {
    uint32_t version;
    size_t size;
    void* data; // owned copy of snapshot payload
    uint64_t hash;
} RollbackEntry;

typedef struct RollbackRing {
    uint32_t configured:1;
    uint32_t capacity; // <= ROGUE_MAX_ROLLBACK_SNAPSHOTS
    uint32_t head; // next write
    uint32_t count; // number of valid entries
    RollbackEntry entries[ROGUE_MAX_ROLLBACK_SNAPSHOTS];
} RollbackRing;

static RollbackRing g_rb[ROGUE_MAX_SYSTEMS];

int rogue_rollback_configure(int system_id, uint32_t capacity){
    if(system_id < 0 || system_id >= ROGUE_MAX_SYSTEMS) return -1;
    if(capacity == 0 || capacity > ROGUE_MAX_ROLLBACK_SNAPSHOTS) return -2;
    RollbackRing* r = &g_rb[system_id];
    memset(r,0,sizeof(*r));
    r->configured=1; r->capacity=capacity; r->head=0; r->count=0;
    return 0;
}

static int rb_capture_locked(int system_id){
    RollbackRing* r = &g_rb[system_id];
    if(!r->configured) return -1;
    int rc = rogue_snapshot_capture(system_id); // updates global latest snapshot
    if(rc) return rc;
    const RogueSystemSnapshot* snap = rogue_snapshot_get(system_id);
    if(!snap || !snap->data) return -2;
    // overwrite existing slot (free prior)
    RollbackEntry* e = &r->entries[r->head];
    if(e->data) { free(e->data); e->data=NULL; }
    e->size = snap->size;
    e->data = malloc(snap->size);
    if(!e->data) return -3;
    memcpy(e->data, snap->data, snap->size);
    e->version = snap->version;
    e->hash = snap->hash;
    r->head = (r->head + 1) % r->capacity;
    if(r->count < r->capacity) r->count++;
    return 0;
}

int rogue_rollback_capture(int system_id){
    if(system_id < 0 || system_id >= ROGUE_MAX_SYSTEMS) return -1;
    return rb_capture_locked(system_id);
}

int rogue_rollback_step_back(int system_id, uint32_t steps){
    if(system_id < 0 || system_id >= ROGUE_MAX_SYSTEMS) return -1;
    RollbackRing* r = &g_rb[system_id];
    if(!r->configured) return -2;
    if(r->count==0) return -3;
    if(steps >= r->count) return -4; // cannot step beyond history
    // head points to next write; latest is head-1
    int32_t idx = (int32_t)r->head - 1 - (int32_t)steps;
    while(idx < 0) idx += r->capacity;
    RollbackEntry* e = &r->entries[idx];
    if(!e->data) return -5;
    // construct transient snapshot wrapper to pass to restore API
    RogueSystemSnapshot tmp = { system_id, NULL, e->version, e->hash, e->size, e->data, 0 };
    return rogue_snapshot_restore(system_id, &tmp);
}

int rogue_rollback_latest(int system_id){
    return rogue_rollback_step_back(system_id,0);
}

int rogue_rollback_purge(int system_id){
    if(system_id < 0 || system_id >= ROGUE_MAX_SYSTEMS) return -1;
    RollbackRing* r = &g_rb[system_id];
    if(!r->configured) return -2;
    for(uint32_t i=0;i<r->capacity;i++){ if(r->entries[i].data){ free(r->entries[i].data); r->entries[i].data=NULL; } }
    r->head=0; r->count=0; memset(r->entries,0,sizeof(r->entries));
    return 0;
}

int rogue_rollback_dump(void* fptr){
    FILE* f = (FILE*)fptr;
    if(!f) f = stdout;
    fprintf(f,"Rollback Manager State\n");
    for(int i=0;i<ROGUE_MAX_SYSTEMS;i++){
        RollbackRing* r = &g_rb[i];
        if(!r->configured) continue;
        fprintf(f,"System %d: cap=%u count=%u head=%u versions:",i,r->capacity,r->count,r->head);
        for(uint32_t k=0;k<r->count;k++){
            int32_t pos = (int32_t)r->head - 1 - (int32_t)k; while(pos < 0) pos += r->capacity; RollbackEntry* e=&r->entries[pos]; fprintf(f," %u", e->version);
        }
        fprintf(f,"\n");
    }
    return 0;
}
