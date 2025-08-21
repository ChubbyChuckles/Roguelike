#include "snapshot_manager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef SNAPSHOT_CAP
#define SNAPSHOT_CAP 64
#endif

static RogueSystemSnapshot g_snaps[SNAPSHOT_CAP];
static RogueSnapshotDesc g_descs[SNAPSHOT_CAP];
static uint32_t g_reg_count = 0;
static uint64_t g_capture_counter = 0;
static RogueSnapshotStats g_stats;
// dependency adjacency matrix (simple dense for small cap)
static uint8_t g_dep[SNAPSHOT_CAP][SNAPSHOT_CAP];

// replay log
typedef struct DeltaLogEntry { RogueSnapshotDeltaRecord rec; } DeltaLogEntry;
static DeltaLogEntry* g_log_entries = NULL; static size_t g_log_cap=0; static size_t g_log_count=0; static size_t g_log_head=0; // ring

static uint64_t fnv1a64(const void* data, size_t len){ const unsigned char* p=data; uint64_t h=1469598103934665603ull; while(len--){ h^=*p++; h*=1099511628211ull; } return h; }

int rogue_snapshot_register(const RogueSnapshotDesc* desc){ if(!desc||!desc->capture) return -1; if(g_reg_count>=SNAPSHOT_CAP) return -1; for(uint32_t i=0;i<g_reg_count;i++) if(g_descs[i].system_id==desc->system_id) return -2; g_descs[g_reg_count]=*desc; g_snaps[g_reg_count]=(RogueSystemSnapshot){ desc->system_id, desc->name, 0,0,0,NULL,0 }; g_reg_count++; g_stats.registered_systems=g_reg_count; return 0; }

static int index_of(int system_id){ for(uint32_t i=0;i<g_reg_count;i++) if(g_descs[i].system_id==system_id) return (int)i; return -1; }

int rogue_snapshot_capture(int system_id){ int idx=index_of(system_id); if(idx<0) return -1; RogueSnapshotDesc* d=&g_descs[idx]; void* data=NULL; size_t size=0; uint32_t ver=0; int rc=d->capture(d->user,&data,&size,&ver); if(rc!=0){ g_stats.total_capture_failures++; return -2; } if(d->max_size && size>d->max_size){ free(data); return -3; } RogueSystemSnapshot* s=&g_snaps[idx]; if(ver<=s->version && s->data){ free(data); return -4; } if(s->data) free(s->data); s->data=data; s->size=size; s->version=ver; s->hash=fnv1a64(data,size); s->timestamp=++g_capture_counter; g_stats.total_captures++; g_stats.total_bytes_stored += size; return 0; }

const RogueSystemSnapshot* rogue_snapshot_get(int system_id){ int idx=index_of(system_id); if(idx<0) return NULL; return &g_snaps[idx]; }

int rogue_snapshot_delta_build(const RogueSystemSnapshot* base, const RogueSystemSnapshot* target, RogueSnapshotDelta* out){ if(!base||!target||!out) return -1; if(base->system_id!=target->system_id) return -2; if(base->version>=target->version) return -3; memset(out,0,sizeof(*out)); out->system_id=base->system_id; out->base_version=base->version; out->target_version=target->version; const unsigned char* a=base->data; const unsigned char* b=target->data; size_t max = base->size<target->size? base->size: target->size; size_t cap=16; RogueSnapshotDeltaRange* ranges=malloc(sizeof(*ranges)*cap); size_t rcount=0; size_t data_cap=64; unsigned char* data_buf=malloc(data_cap); size_t data_len=0; size_t i=0; uint64_t build_start_ns=0; // timing stub (could integrate platform timer)
 while(i<max){ if(a[i]!=b[i]){ size_t start=i; size_t off=i; while(i<max && a[i]!=b[i]){ i++; } size_t length=i-start; if(rcount==cap){ cap*=2; ranges=realloc(ranges,sizeof(*ranges)*cap); } ranges[rcount]=(RogueSnapshotDeltaRange){ off,length }; if(data_len+length>data_cap){ while(data_len+length>data_cap) data_cap*=2; data_buf=realloc(data_buf,data_cap); } memcpy(data_buf+data_len,b+off,length); data_len+=length; rcount++; } else { i++; } }
 if(target->size>base->size){ size_t extra = target->size - base->size; if(rcount==cap){ cap*=2; ranges=realloc(ranges,sizeof(*ranges)*cap); } ranges[rcount]=(RogueSnapshotDeltaRange){ base->size, extra }; if(data_len+extra>data_cap){ while(data_len+extra>data_cap) data_cap*=2; data_buf=realloc(data_buf,data_cap); } memcpy(data_buf+data_len,b+base->size,extra); data_len+=extra; rcount++; }
 out->ranges=ranges; out->range_count=rcount; out->data=data_buf; out->data_size=data_len; g_stats.total_delta_generated++; g_stats.total_delta_bytes += data_len; if(base->size==target->size) g_stats.bytes_saved_via_delta += (uint64_t)(target->size>data_len? target->size - data_len:0); (void)build_start_ns; return 0; }

int rogue_snapshot_delta_apply(const RogueSystemSnapshot* base, const RogueSnapshotDelta* delta, void** out_new_data, size_t* out_size, uint64_t* out_hash){ if(!base||!delta||!out_new_data||!out_size) return -1; if(base->system_id!=delta->system_id) return -2; if(base->version!=delta->base_version) { g_stats.delta_apply_failures++; return -3; } size_t size=base->size; for(size_t i=0;i<delta->range_count;i++){ size_t end = delta->ranges[i].offset + delta->ranges[i].length; if(end>size) size=end; }
 unsigned char* buf=malloc(size); if(!buf) return -4; memcpy(buf,base->data,base->size); size_t data_off=0; for(size_t r=0;r<delta->range_count;r++){ RogueSnapshotDeltaRange* rg=&delta->ranges[r]; if(rg->offset+rg->length>size){ free(buf); g_stats.delta_apply_failures++; return -5; } memcpy(buf+rg->offset, delta->data+data_off, rg->length); data_off += rg->length; }
 uint64_t hash = fnv1a64(buf,size); if(out_hash) *out_hash=hash; *out_new_data=buf; *out_size=size; g_stats.total_delta_applied++; return 0; }

void rogue_snapshot_delta_free(RogueSnapshotDelta* d){ if(!d) return; free(d->ranges); free(d->data); memset(d,0,sizeof(*d)); }

void rogue_snapshot_get_stats(RogueSnapshotStats* out){ if(!out) return; *out=g_stats; }

uint64_t rogue_snapshot_rehash(const RogueSystemSnapshot* snap){ if(!snap||!snap->data) return 0; return fnv1a64(snap->data,snap->size); }

int rogue_snapshot_restore(int system_id, const RogueSystemSnapshot* snap){ if(!snap) return -1; int idx=index_of(system_id); if(idx<0) return -2; RogueSnapshotDesc* d=&g_descs[idx]; if(!d->restore) return -3; // no hook
 return d->restore(d->user, snap->data, snap->size, snap->version); }

void rogue_snapshot_dump(void* fptr){ FILE* f=fptr? (FILE*)fptr: stdout; fprintf(f,"[snapshots] systems=%u captures=%llu deltas=%llu bytes=%llu delta_bytes=%llu saved=%llu\n", g_stats.registered_systems,(unsigned long long)g_stats.total_captures,(unsigned long long)g_stats.total_delta_generated,(unsigned long long)g_stats.total_bytes_stored,(unsigned long long)g_stats.total_delta_bytes,(unsigned long long)g_stats.bytes_saved_via_delta); for(uint32_t i=0;i<g_reg_count;i++){ RogueSystemSnapshot* s=&g_snaps[i]; fprintf(f," sys id=%d name=%s ver=%u size=%zu hash=%016llx\n", s->system_id,s->name? s->name:"?", s->version, s->size,(unsigned long long)s->hash); } }

// ---- Dependency management ----
int rogue_snapshot_dependency_add(int system_id, int depends_on){ int a=index_of(system_id), b=index_of(depends_on); if(a<0||b<0) return -1; if(a==b) return -2; g_dep[a][b]=1; return 0; }

static int topo_visit(int idx, uint8_t* temp, uint8_t* perm, int* out, size_t* out_pos){ if(temp[idx]) return -2; if(perm[idx]) return 0; temp[idx]=1; for(uint32_t j=0;j<g_reg_count;j++) if(g_dep[idx][j]){ int r=topo_visit(j,temp,perm,out,out_pos); if(r<0) return r; } perm[idx]=1; temp[idx]=0; out[(*out_pos)++]=g_descs[idx].system_id; return 0; }

int rogue_snapshot_plan_order(int* out_ids, size_t* inout_count){ if(!out_ids||!inout_count) return -1; size_t cap=*inout_count; size_t pos=0; uint8_t temp[SNAPSHOT_CAP]={0}; uint8_t perm[SNAPSHOT_CAP]={0}; for(uint32_t i=0;i<g_reg_count;i++){ int r=topo_visit(i,temp,perm,out_ids,&pos); if(r<0) return r; } if(pos>cap) return -3; *inout_count=pos; return 0; }

// ---- Replay Log ----
int rogue_snapshot_replay_log_enable(size_t capacity){ if(g_log_entries){ free(g_log_entries); g_log_entries=NULL; g_log_cap=g_log_count=g_log_head=0; } if(capacity==0) return 0; g_log_entries=calloc(capacity,sizeof(DeltaLogEntry)); if(!g_log_entries) return -1; g_log_cap=capacity; return 0; }

static void log_delta(const RogueSystemSnapshot* base, const RogueSystemSnapshot* target, const RogueSnapshotDelta* d){ if(!g_log_entries||!d) return; DeltaLogEntry* e=&g_log_entries[g_log_head]; e->rec.system_id=target->system_id; e->rec.base_version=base->version; e->rec.target_version=target->version; e->rec.timestamp=target->timestamp; e->rec.full_size=target->size; e->rec.delta_size=d->data_size; e->rec.range_count=d->range_count; e->rec.target_hash=target->hash; g_log_head=(g_log_head+1)%g_log_cap; if(g_log_count<g_log_cap) g_log_count++; }

int rogue_snapshot_replay_log_get(const RogueSnapshotDeltaRecord** out_records, size_t* out_count){ if(!out_records||!out_count) return -1; if(!g_log_entries){ *out_records=NULL; *out_count=0; return 0; } *out_records=&g_log_entries[0].rec; *out_count=g_log_count; return 0; }

int rogue_snapshot_replay_apply(size_t start_index, size_t count){ if(!g_log_entries) return -1; if(start_index+count>g_log_count) return -2; for(size_t i=0;i<count;i++){ size_t idx=(start_index+i)%g_log_cap; DeltaLogEntry* e=&g_log_entries[idx]; int sys_idx=index_of(e->rec.system_id); if(sys_idx<0) return -3; RogueSystemSnapshot* cur=&g_snaps[sys_idx]; if(cur->version!=e->rec.base_version) continue; // skip if diverged
 // simulate delta by rebuilding from existing snapshot to target hash not stored (we need stored data - for now skip)
 (void)cur; }
 return 0; }

int rogue_snapshot_reset(int system_id){ int idx=index_of(system_id); if(idx<0) return -1; RogueSystemSnapshot* s=&g_snaps[idx]; if(s->data){ free(s->data); memset(s,0,sizeof(*s)); s->system_id=g_descs[idx].system_id; s->name=g_descs[idx].name; } return 0; }
