#include "deadlock_manager.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct ResourceWaiters {
    int id;
    int holder_tx; // -1 none
    int waiters[ROGUE_DEADLOCK_MAX_WAITERS];
    uint8_t wait_count;
} ResourceWaiters;

static ResourceWaiters g_resources[ROGUE_DEADLOCK_MAX_RESOURCES];
static uint8_t g_resource_used[ROGUE_DEADLOCK_MAX_RESOURCES];
static RogueDeadlockStats g_stats;
static uint64_t g_cycle_seq=1;

typedef struct TxState {
    int id; // 0 unused
    uint64_t hold_mask_low; // resources 0..63
    uint64_t hold_mask_high; // resources 64..127
    int waiting_for; // resource id or -1
} TxState;
static TxState g_txs[ROGUE_DEADLOCK_MAX_TX];

static RogueDeadlockCycle g_cycle_log[ROGUE_DEADLOCK_CYCLE_LOG];
static size_t g_cycle_count=0, g_cycle_head=0;

static int (*g_abort_cb)(int tx_id, const char* reason) = NULL;

static TxState* tx_get(int tx_id){
    if(tx_id<=0) return NULL;
    for(int i=0;i<ROGUE_DEADLOCK_MAX_TX;i++) if(g_txs[i].id==tx_id) return &g_txs[i];
    for(int i=0;i<ROGUE_DEADLOCK_MAX_TX;i++) if(g_txs[i].id==0){ g_txs[i].id=tx_id; g_txs[i].waiting_for=-1; return &g_txs[i]; }
    return NULL;
}

static int res_index(int resource_id){ if(resource_id<0||resource_id>=ROGUE_DEADLOCK_MAX_RESOURCES) return -1; return resource_id; }

int rogue_deadlock_register_resource(int resource_id){ int ri=res_index(resource_id); if(ri<0) return -1; if(!g_resource_used[ri]){ memset(&g_resources[ri],0,sizeof(ResourceWaiters)); g_resources[ri].id=resource_id; g_resources[ri].holder_tx=-1; g_resource_used[ri]=1; g_stats.resources_registered++; } return 0; }

static void tx_add_hold(TxState* tx,int resource_id){ if(resource_id<64) tx->hold_mask_low |= (1ULL<<resource_id); else tx->hold_mask_high |= (1ULL<<(resource_id-64)); }
static int tx_holds(TxState* tx,int resource_id){ if(resource_id<64) return (tx->hold_mask_low & (1ULL<<resource_id))?1:0; return (tx->hold_mask_high & (1ULL<<(resource_id-64)))?1:0; }
int rogue_deadlock_tx_holds(int tx_id,int resource_id){ TxState* tx=tx_get(tx_id); if(!tx) return 0; return tx_holds(tx,resource_id); }

int rogue_deadlock_acquire(int tx_id, int resource_id){ int ri=res_index(resource_id); if(ri<0||!g_resource_used[ri]) return -1; TxState* tx=tx_get(tx_id); if(!tx) return -2; ResourceWaiters* r=&g_resources[ri]; if(r->holder_tx<0){ r->holder_tx=tx_id; tx_add_hold(tx,resource_id); g_stats.acquisitions++; return 0; }
    if(r->holder_tx==tx_id) return 0; // already holds
    for(int i=0;i<r->wait_count;i++) if(r->waiters[i]==tx_id) return 1; if(r->wait_count>=ROGUE_DEADLOCK_MAX_WAITERS) return -3; r->waiters[r->wait_count++]=tx_id; tx->waiting_for=resource_id; g_stats.waits++; return 1; }

static int promote_waiter(ResourceWaiters* r){ if(r->wait_count==0) return 0; int tx_id=r->waiters[0];
    for(int i=1;i<r->wait_count;i++) r->waiters[i-1]=r->waiters[i]; r->wait_count--; r->holder_tx=tx_id; TxState* tx=tx_get(tx_id); if(tx){ tx_add_hold(tx,r->id); if(tx->waiting_for==r->id) tx->waiting_for=-1; }
    g_stats.wait_promotions++; return 1; }

int rogue_deadlock_release(int tx_id,int resource_id){ int ri=res_index(resource_id); if(ri<0||!g_resource_used[ri]) return -1; ResourceWaiters* r=&g_resources[ri]; if(r->holder_tx!=tx_id) return -2; r->holder_tx=-1; promote_waiter(r); g_stats.releases++; return 0; }

int rogue_deadlock_release_all(int tx_id){ int released=0; TxState* tx=tx_get(tx_id); if(!tx) return 0; for(int ri=0;ri<ROGUE_DEADLOCK_MAX_RESOURCES;ri++){ if(!g_resource_used[ri]) continue; if(g_resources[ri].holder_tx==tx_id){ g_resources[ri].holder_tx=-1; promote_waiter(&g_resources[ri]); released++; g_stats.releases++; }
        if(g_resources[ri].wait_count){ int w=0; for(int i=0;i<g_resources[ri].wait_count;i++){ if(g_resources[ri].waiters[i]==tx_id){ } else { g_resources[ri].waiters[w++]=g_resources[ri].waiters[i]; } } g_resources[ri].wait_count=(uint8_t)w; }
    }
    tx->hold_mask_low=tx->hold_mask_high=0; tx->waiting_for=-1; tx->id=tx_id; return released; }

static int dfs_cycle(int start_tx,int cur_tx,int* path,int depth,int* out_cycle,size_t* out_len){ if(depth>=16) return 0; path[depth]=cur_tx; TxState* cur=tx_get(cur_tx); if(!cur||cur->waiting_for<0) return 0; ResourceWaiters* r=&g_resources[cur->waiting_for]; if(r->holder_tx<0) return 0; int next_tx=r->holder_tx; if(next_tx==start_tx){ size_t len=depth+1; if(out_cycle){ for(size_t i=0;i<len && i<16;i++) out_cycle[i]=path[i]; } *out_len=len; return 1; } for(int i=0;i<=depth;i++) if(path[i]==next_tx) return 0; return dfs_cycle(start_tx,next_tx,path,depth+1,out_cycle,out_len); }

static void log_cycle(int* tx_ids,size_t len,int victim){ RogueDeadlockCycle* c=&g_cycle_log[g_cycle_head]; memset(c,0,sizeof(*c)); c->seq=g_cycle_seq++; c->tx_count=len>16?16:len; for(size_t i=0;i<c->tx_count;i++) c->tx_ids[i]=tx_ids[i]; c->victim_tx_id=victim; g_cycle_head=(g_cycle_head+1)%ROGUE_DEADLOCK_CYCLE_LOG; if(g_cycle_count<ROGUE_DEADLOCK_CYCLE_LOG) g_cycle_count++; }
static int choose_victim(int* tx_ids,size_t len){ int victim=tx_ids[0]; for(size_t i=1;i<len;i++) if(tx_ids[i]>victim) victim=tx_ids[i]; return victim; }

int rogue_deadlock_tick(uint64_t now_ms){ (void)now_ms; g_stats.ticks++; int resolved=0; int path[32]; int cyc[16]; size_t clen=0; for(int i=0;i<ROGUE_DEADLOCK_MAX_TX;i++){ if(!g_txs[i].id) continue; TxState* tx=&g_txs[i]; if(tx->waiting_for<0) continue; clen=0; if(dfs_cycle(tx->id,tx->id,path,0,cyc,&clen)){ g_stats.deadlocks_detected++; int victim=choose_victim(cyc,clen); if(g_abort_cb) g_abort_cb(victim,"deadlock victim"); rogue_deadlock_release_all(victim); g_stats.victims_aborted++; log_cycle(cyc,clen,victim); resolved++; } }
    return resolved; }

void rogue_deadlock_get_stats(RogueDeadlockStats* out){ if(!out) return; *out=g_stats; }
int rogue_deadlock_cycles_get(const RogueDeadlockCycle** out_cycles,size_t* count){ if(!out_cycles||!count) return -1; *out_cycles=g_cycle_log; *count=g_cycle_count; return 0; }
void rogue_deadlock_dump(void* fptr){ FILE* f=fptr? (FILE*)fptr: stdout; fprintf(f,"[deadlock] res=%llu acq=%llu waits=%llu dl=%llu victims=%llu rel=%llu ticks=%llu promotions=%llu cycles_logged=%zu\n",(unsigned long long)g_stats.resources_registered,(unsigned long long)g_stats.acquisitions,(unsigned long long)g_stats.waits,(unsigned long long)g_stats.deadlocks_detected,(unsigned long long)g_stats.victims_aborted,(unsigned long long)g_stats.releases,(unsigned long long)g_stats.ticks,(unsigned long long)g_stats.wait_promotions,g_cycle_count); for(size_t i=0;i<g_cycle_count;i++){ const RogueDeadlockCycle* c=&g_cycle_log[i]; fprintf(f," cycle seq=%llu count=%zu victim=%d path=",(unsigned long long)c->seq,c->tx_count,c->victim_tx_id); for(size_t j=0;j<c->tx_count;j++) fprintf(f,"%d%s",c->tx_ids[j],j+1<c->tx_count?"->":""); fprintf(f,"\n"); } }
void rogue_deadlock_reset_all(void){ memset(&g_resources,0,sizeof(g_resources)); memset(&g_resource_used,0,sizeof(g_resource_used)); memset(&g_txs,0,sizeof(g_txs)); memset(&g_stats,0,sizeof(g_stats)); memset(&g_cycle_log,0,sizeof(g_cycle_log)); g_cycle_count=g_cycle_head=0; g_cycle_seq=1; }
void rogue_deadlock_set_abort_callback(int (*fn)(int tx_id,const char* reason)){ g_abort_cb=fn; }
void rogue_deadlock_on_tx_abort(int tx_id){ rogue_deadlock_release_all(tx_id); }
void rogue_deadlock_on_tx_commit(int tx_id){ rogue_deadlock_release_all(tx_id); }
