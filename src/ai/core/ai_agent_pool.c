#include "ai/core/ai_agent_pool.h"
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

typedef struct PoolNode {
    struct PoolNode* next;
    unsigned char payload[2048]; /* large enough for EnemyAIBlackboard (~1KB) */
} PoolNode;

static PoolNode* g_free_list = NULL;
static int g_in_use = 0;
static int g_peak_created = 0;
static int g_total_created = 0;

struct EnemyAIBlackboard* rogue_ai_agent_acquire(void){
    PoolNode* n = g_free_list;
    if(n){ g_free_list = n->next; }
    else {
        n = (PoolNode*)malloc(sizeof(PoolNode));
        if(!n) return NULL;
        g_total_created++; if(g_total_created > g_peak_created) g_peak_created = g_total_created;
    }
    g_in_use++;
    memset(n->payload,0,sizeof n->payload);
    return (struct EnemyAIBlackboard*)n->payload;
}

void rogue_ai_agent_release(struct EnemyAIBlackboard* blk){
    if(!blk) return;
    PoolNode* n = (PoolNode*)((unsigned char*)blk - offsetof(PoolNode,payload));
    n->next = g_free_list;
    g_free_list = n;
    if(g_in_use>0) g_in_use--;
}

int rogue_ai_agent_pool_in_use(void){ return g_in_use; }
int rogue_ai_agent_pool_free(void){ int c=0; for(PoolNode* n=g_free_list;n;n=n->next) c++; return c; }
int rogue_ai_agent_pool_peak(void){ return g_peak_created; }

size_t rogue_ai_agent_pool_slab_size(void){ return sizeof(((PoolNode*)0)->payload); }

void rogue_ai_agent_pool_reset_for_tests(void){
    PoolNode* n=g_free_list; g_free_list=NULL; while(n){ PoolNode* nxt=n->next; free(n); n=nxt; }
    g_in_use=0; g_peak_created=0; g_total_created=0;
}
