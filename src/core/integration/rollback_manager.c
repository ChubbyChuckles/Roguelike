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

typedef enum { RB_ENTRY_FULL=1, RB_ENTRY_DELTA=2 } RollbackEntryType;
typedef struct RollbackEntry {
    uint32_t version;
    uint32_t base_version; // for delta entries reference FULL base
    size_t size; // full size (for delta original full size)
    void* data; // owned copy (full snapshot bytes or delta bytes)
    uint64_t hash; // hash of reconstructed full snapshot
    RollbackEntryType type;
    size_t delta_applied; // bytes in data when delta
} RollbackEntry;

typedef struct RollbackRing {
    uint32_t configured:1;
    uint32_t capacity; // <= ROGUE_MAX_ROLLBACK_SNAPSHOTS
    uint32_t head; // next write
    uint32_t count; // number of valid entries
    RollbackEntry entries[ROGUE_MAX_ROLLBACK_SNAPSHOTS];
} RollbackRing;

static RollbackRing g_rb[ROGUE_MAX_SYSTEMS];
static int g_participant_system_map[128]; // simple mapping for transaction participants

static RogueRollbackStats g_rb_stats;
// Event log (fixed ring)
#ifndef ROGUE_ROLLBACK_EVENT_CAP
#define ROGUE_ROLLBACK_EVENT_CAP 256
#endif
static RogueRollbackEvent g_events[ROGUE_ROLLBACK_EVENT_CAP];
static size_t g_event_count=0; static size_t g_event_head=0; static uint64_t g_event_seq=0;
// Ensure stats struct is zero-initialized explicitly (defensive for static analyzers)
static void rb_stats_init(void){ static int inited=0; if(!inited){ memset(&g_rb_stats,0,sizeof(g_rb_stats)); inited=1; } }

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
    // decide delta vs full (compare with previous FULL entry)
    RollbackEntry* prev_full=NULL; int idx_latest=(int)r->head-1; if(idx_latest<0) idx_latest = (int)r->capacity-1; if(r->count>0){ RollbackEntry* last=&r->entries[idx_latest]; if(last->type==RB_ENTRY_FULL) prev_full=last; else if(last->type==RB_ENTRY_DELTA){ // find its base
            for(uint32_t k=0;k<r->count;k++){ int pos=(int)r->head-1-(int)k; while(pos<0) pos+=(int)r->capacity; RollbackEntry* cand=&r->entries[pos]; if(cand->type==RB_ENTRY_FULL){ prev_full=cand; break; } }
        }
    }
    size_t full_size = snap->size;
    unsigned char* store_buf=NULL; size_t store_size=0; RollbackEntryType type=RB_ENTRY_FULL; uint32_t base_version=0; size_t delta_bytes=0;
    if(prev_full && prev_full->size==snap->size && prev_full->size>0){
        // build byte diff (simple contiguous copy of differing bytes preceded by count+index pairs)
        const unsigned char* a=prev_full->data; const unsigned char* b=(const unsigned char*)snap->data; // prev_full->data holds full snapshot
        // naive diff: store (uint32_t count, then pairs (offset(uint32_t),length(uint32_t),bytes...)) if savings > header
        size_t max_changes=0; for(size_t i=0;i<full_size;i++) if(a[i]!=b[i]){ max_changes++; while(i+1<full_size && a[i+1]!=b[i+1]) i++; }
        size_t est_header = 8 + max_changes * (8); // rough
        unsigned char* diff = malloc(full_size + est_header); if(!diff) return -3; unsigned char* p=diff; uint32_t range_count=0; p+=8; // reserve space (range_count, full_size)
        for(size_t i=0;i<full_size;i++) if(a[i]!=b[i]){ size_t start=i; while(i+1<full_size && a[i+1]!=b[i+1]) i++; size_t len = i-start+1; memcpy(p,&start,sizeof(uint32_t)); p+=4; uint32_t ulen=(uint32_t)len; memcpy(p,&ulen,4); p+=4; memcpy(p,b+start,len); p+=len; range_count++; }
        memcpy(diff,&range_count,4); memcpy(diff+4,&full_size,4);
        size_t diff_size = (size_t)(p-diff);
        if(diff_size < full_size){ store_buf=diff; store_size=diff_size; type=RB_ENTRY_DELTA; base_version=prev_full->version; delta_bytes=diff_size; g_rb_stats.delta_entries++; g_rb_stats.bytes_delta += diff_size; g_rb_stats.bytes_saved_via_delta += (full_size>diff_size? full_size-diff_size:0); }
        else { free(diff); }
    }
    if(!store_buf){ store_buf=malloc(full_size); if(!store_buf) return -3; memcpy(store_buf,snap->data,full_size); store_size=full_size; type=RB_ENTRY_FULL; g_rb_stats.bytes_full += full_size; }

    // overwrite slot
    RollbackEntry* e = &r->entries[r->head]; if(e->data){ free(e->data); }
    e->size=full_size; e->data=store_buf; e->version=snap->version; e->hash=snap->hash; e->type=type; e->base_version=base_version; e->delta_applied=delta_bytes;    
    r->head = (r->head + 1) % r->capacity;
    if(r->count < r->capacity) r->count++;
    g_rb_stats.checkpoints_captured++;
    return 0;
}

int rogue_rollback_capture(int system_id){
    if(system_id < 0 || system_id >= ROGUE_MAX_SYSTEMS) return -1;
    rb_stats_init();
    return rb_capture_locked(system_id);
}

int rogue_rollback_capture_multi(const int* system_ids, size_t count){ if(!system_ids) return -1; for(size_t i=0;i<count;i++){ int rc=rogue_rollback_capture(system_ids[i]); if(rc) return rc; } return 0; }

int rogue_rollback_step_back(int system_id, uint32_t steps){
    if(system_id < 0 || system_id >= ROGUE_MAX_SYSTEMS) return -1;
    RollbackRing* r = &g_rb[system_id];
    if(!r->configured) return -2;
    if(r->count==0){
        if(steps==0) return 0; // treat as no-op if asking for latest with no history (graceful)
        return -3; // no history to step back
    }
    if(steps >= r->count) return -4; // cannot step beyond history
    // head points to next write; latest is head-1
    int32_t idx = (int32_t)r->head - 1 - (int32_t)steps;
    while(idx < 0) idx += r->capacity;
    RollbackEntry* e = &r->entries[idx]; if(!e->data) return -5;
    // reconstruct if delta
    unsigned char* full_buf=NULL; if(e->type==RB_ENTRY_DELTA){ // find base FULL entry
        RollbackEntry* base=NULL; for(uint32_t k=0;k<r->count;k++){ int pos=(int)r->head-1-(int)k; while(pos<0) pos+=(int)r->capacity; RollbackEntry* cand=&r->entries[pos]; if(cand->type==RB_ENTRY_FULL && cand->version==e->base_version){ base=cand; break; } }
        if(!base) return -6;
        full_buf=malloc(e->size); if(!full_buf) return -7; memcpy(full_buf,base->data,base->size);
        // parse delta
        unsigned char* p=e->data; uint32_t range_count=0; memcpy(&range_count,p,4); p+=4; uint32_t full_size=0; memcpy(&full_size,p,4); p+=4; if(full_size!=e->size){ free(full_buf); return -8; }
        for(uint32_t ri=0;ri<range_count;ri++){ uint32_t off=0,len=0; memcpy(&off,p,4); p+=4; memcpy(&len,p,4); p+=4; if(off+len>full_size){ free(full_buf); return -9; } memcpy(full_buf+off,p,len); p+=len; }
    }
    const void* restore_data = (e->type==RB_ENTRY_FULL)? e->data : full_buf;
    RogueSystemSnapshot tmp = { system_id, NULL, e->version, e->hash, e->size, (void*)restore_data, 0 };
    int rc = rogue_snapshot_restore(system_id, &tmp);
    if(rc==0){
        g_rb_stats.restores_performed++;
        g_rb_stats.systems_rewound++;
        g_rb_stats.bytes_rewound += e->size;
        // validation: rehash live snapshot to ensure match (if snapshot manager retains it)
        const RogueSystemSnapshot* cur = rogue_snapshot_get(system_id);
        if(cur && cur->hash != e->hash){ g_rb_stats.validation_failures++; }
        // log event
        RogueRollbackEvent* ev=&g_events[g_event_head];
        ev->timestamp=++g_event_seq; ev->system_id=system_id; ev->to_version=e->version; ev->from_version=cur? cur->version:0; ev->bytes_rewound=e->size; ev->was_delta=(uint8_t)(e->type==RB_ENTRY_DELTA); ev->auto_triggered=0;
        g_event_head=(g_event_head+1)%ROGUE_ROLLBACK_EVENT_CAP; if(g_event_count<ROGUE_ROLLBACK_EVENT_CAP) g_event_count++;
    }
    if(full_buf) free(full_buf);
    return rc;
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

int rogue_rollback_partial(const int* system_ids, const uint32_t* steps, size_t count){ if(!system_ids||!steps) return -1; for(size_t i=0;i<count;i++){ int rc=rogue_rollback_step_back(system_ids[i], steps[i]); if(rc) return rc; g_rb_stats.partial_rollbacks++; } return 0; }

void rogue_rollback_get_stats(RogueRollbackStats* out){ if(!out) return; *out=g_rb_stats; }

int rogue_rollback_auto_for_participant(int participant_id){
    if(participant_id<0||participant_id>=(int)(sizeof(g_participant_system_map)/sizeof(g_participant_system_map[0]))) return -1;
    int sys = g_participant_system_map[participant_id];
    if(sys<=0) return -2; // 0 reserved / unmapped
    // manually reproduce step_back logic for logging auto flag
    RollbackRing* r=&g_rb[sys]; if(!r->configured||r->count==0) return -3; int32_t idx=(int32_t)r->head-1; if(idx<0) idx+=r->capacity; RollbackEntry* e=&r->entries[idx];
    unsigned char* full_buf=NULL; if(e->type==RB_ENTRY_DELTA){ RollbackEntry* base=NULL; for(uint32_t k=0;k<r->count;k++){ int pos=(int)r->head-1-(int)k; while(pos<0) pos+=r->capacity; RollbackEntry* cand=&r->entries[pos]; if(cand->type==RB_ENTRY_FULL && cand->version==e->base_version){ base=cand; break; } } if(!base) return -4; full_buf=malloc(e->size); if(!full_buf) return -5; memcpy(full_buf,base->data,base->size); unsigned char* p=e->data; uint32_t range_count=0; memcpy(&range_count,p,4); p+=4; uint32_t full_size=0; memcpy(&full_size,p,4); p+=4; if(full_size!=e->size){ free(full_buf); return -6; } for(uint32_t ri=0;ri<range_count;ri++){ uint32_t off=0,len=0; memcpy(&off,p,4); p+=4; memcpy(&len,p,4); p+=4; if(off+len>full_size){ free(full_buf); return -7; } memcpy(full_buf+off,p,len); p+=len; } }
    const void* restore_data=(e->type==RB_ENTRY_FULL)? e->data: full_buf;
    RogueSystemSnapshot tmp={ sys, NULL, e->version, e->hash, e->size, (void*)restore_data, 0 };
    int rc=rogue_snapshot_restore(sys,&tmp);
    if(rc==0){ g_rb_stats.restores_performed++; g_rb_stats.systems_rewound++; g_rb_stats.bytes_rewound += e->size; g_rb_stats.auto_rollbacks++; const RogueSystemSnapshot* cur=rogue_snapshot_get(sys); if(cur && cur->hash!=e->hash) g_rb_stats.validation_failures++; RogueRollbackEvent* ev=&g_events[g_event_head]; ev->timestamp=++g_event_seq; ev->system_id=sys; ev->to_version=e->version; ev->from_version=cur? cur->version:0; ev->bytes_rewound=e->size; ev->was_delta=(uint8_t)(e->type==RB_ENTRY_DELTA); ev->auto_triggered=1; g_event_head=(g_event_head+1)%ROGUE_ROLLBACK_EVENT_CAP; if(g_event_count<ROGUE_ROLLBACK_EVENT_CAP) g_event_count++; }
    if(full_buf) free(full_buf);
    return rc;
}

int rogue_rollback_map_participant(int participant_id, int system_id){ if(participant_id<0||participant_id>= (int)(sizeof(g_participant_system_map)/sizeof(g_participant_system_map[0]))) return -1; g_participant_system_map[participant_id]=system_id; return 0; }

int rogue_rollback_events_get(const RogueRollbackEvent** out_events, size_t* count){ if(!out_events||!count) return -1; if(g_event_count==0){ *out_events=NULL; *count=0; return 0; } // produce contiguous logical order oldest→newest (simple if ring wrapped we need temp order)
    // For simplicity, return physical array start; consumer must handle wrap (documented). We'll reorder only if wrapped into a static temp.
    *out_events=g_events; *count=g_event_count; return 0; }
