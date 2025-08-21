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

static uint64_t fnv1a64(const void* data, size_t len){ const unsigned char* p=data; uint64_t h=1469598103934665603ull; while(len--){ h^=*p++; h*=1099511628211ull; } return h; }

int rogue_snapshot_register(const RogueSnapshotDesc* desc){ if(!desc||!desc->capture) return -1; if(g_reg_count>=SNAPSHOT_CAP) return -1; for(uint32_t i=0;i<g_reg_count;i++) if(g_descs[i].system_id==desc->system_id) return -2; g_descs[g_reg_count]=*desc; g_snaps[g_reg_count]=(RogueSystemSnapshot){ desc->system_id, desc->name, 0,0,0,NULL,0 }; g_reg_count++; g_stats.registered_systems=g_reg_count; return 0; }

static int index_of(int system_id){ for(uint32_t i=0;i<g_reg_count;i++) if(g_descs[i].system_id==system_id) return (int)i; return -1; }

int rogue_snapshot_capture(int system_id){ int idx=index_of(system_id); if(idx<0) return -1; RogueSnapshotDesc* d=&g_descs[idx]; void* data=NULL; size_t size=0; uint32_t ver=0; int rc=d->capture(d->user,&data,&size,&ver); if(rc!=0){ g_stats.total_capture_failures++; return -2; } if(d->max_size && size>d->max_size){ free(data); return -3; } RogueSystemSnapshot* s=&g_snaps[idx]; if(ver<=s->version && s->data){ free(data); return -4; } if(s->data) free(s->data); s->data=data; s->size=size; s->version=ver; s->hash=fnv1a64(data,size); s->timestamp=++g_capture_counter; g_stats.total_captures++; g_stats.total_bytes_stored += size; return 0; }

const RogueSystemSnapshot* rogue_snapshot_get(int system_id){ int idx=index_of(system_id); if(idx<0) return NULL; return &g_snaps[idx]; }

int rogue_snapshot_delta_build(const RogueSystemSnapshot* base, const RogueSystemSnapshot* target, RogueSnapshotDelta* out){ if(!base||!target||!out) return -1; if(base->system_id!=target->system_id) return -2; if(base->version>=target->version) return -3; memset(out,0,sizeof(*out)); out->system_id=base->system_id; out->base_version=base->version; out->target_version=target->version; // naive byte range diff combine adjacent
 const unsigned char* a=base->data; const unsigned char* b=target->data; size_t max = base->size<target->size? base->size: target->size; size_t cap=16; RogueSnapshotDeltaRange* ranges=malloc(sizeof(*ranges)*cap); size_t rcount=0; size_t data_cap=64; unsigned char* data_buf=malloc(data_cap); size_t data_len=0; size_t i=0; while(i<max){ if(a[i]!=b[i]){ size_t start=i; size_t off=i; while(i<max && a[i]!=b[i]){ i++; } size_t length=i-start; if(rcount==cap){ cap*=2; ranges=realloc(ranges,sizeof(*ranges)*cap); } ranges[rcount]=(RogueSnapshotDeltaRange){ off,length }; if(data_len+length>data_cap){ while(data_len+length>data_cap) data_cap*=2; data_buf=realloc(data_buf,data_cap); } memcpy(data_buf+data_len,b+off,length); data_len+=length; rcount++; } else { i++; } }
 if(target->size>base->size){ size_t extra = target->size - base->size; if(rcount==cap){ cap*=2; ranges=realloc(ranges,sizeof(*ranges)*cap); } ranges[rcount]=(RogueSnapshotDeltaRange){ base->size, extra }; if(data_len+extra>data_cap){ while(data_len+extra>data_cap) data_cap*=2; data_buf=realloc(data_buf,data_cap); } memcpy(data_buf+data_len,b+base->size,extra); data_len+=extra; rcount++; }
 out->ranges=ranges; out->range_count=rcount; out->data=data_buf; out->data_size=data_len; g_stats.total_delta_generated++; g_stats.total_delta_bytes += data_len; if(base->size==target->size) g_stats.bytes_saved_via_delta += (uint64_t)(target->size>data_len? target->size - data_len:0); return 0; }

int rogue_snapshot_delta_apply(const RogueSystemSnapshot* base, const RogueSnapshotDelta* delta, void** out_new_data, size_t* out_size, uint64_t* out_hash){ if(!base||!delta||!out_new_data||!out_size) return -1; if(base->system_id!=delta->system_id) return -2; if(base->version!=delta->base_version) return -3; size_t size=base->size; for(size_t i=0;i<delta->range_count;i++){ size_t end = delta->ranges[i].offset + delta->ranges[i].length; if(end>size) size=end; }
 unsigned char* buf=malloc(size); if(!buf) return -4; memcpy(buf,base->data,base->size); size_t data_off=0; for(size_t r=0;r<delta->range_count;r++){ RogueSnapshotDeltaRange* rg=&delta->ranges[r]; memcpy(buf+rg->offset, delta->data+data_off, rg->length); data_off += rg->length; }
 uint64_t hash = fnv1a64(buf,size); if(out_hash) *out_hash=hash; *out_new_data=buf; *out_size=size; g_stats.total_delta_applied++; return 0; }

void rogue_snapshot_delta_free(RogueSnapshotDelta* d){ if(!d) return; free(d->ranges); free(d->data); memset(d,0,sizeof(*d)); }

void rogue_snapshot_get_stats(RogueSnapshotStats* out){ if(!out) return; *out=g_stats; }

uint64_t rogue_snapshot_rehash(const RogueSystemSnapshot* snap){ if(!snap||!snap->data) return 0; return fnv1a64(snap->data,snap->size); }

void rogue_snapshot_dump(void* fptr){ FILE* f=fptr? (FILE*)fptr: stdout; fprintf(f,"[snapshots] systems=%u captures=%llu deltas=%llu bytes=%llu delta_bytes=%llu saved=%llu\n", g_stats.registered_systems,(unsigned long long)g_stats.total_captures,(unsigned long long)g_stats.total_delta_generated,(unsigned long long)g_stats.total_bytes_stored,(unsigned long long)g_stats.total_delta_bytes,(unsigned long long)g_stats.bytes_saved_via_delta); for(uint32_t i=0;i<g_reg_count;i++){ RogueSystemSnapshot* s=&g_snaps[i]; fprintf(f," sys id=%d name=%s ver=%u size=%zu hash=%016llx\n", s->system_id,s->name? s->name:"?", s->version, s->size,(unsigned long long)s->hash); } }
