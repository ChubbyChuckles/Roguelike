/* AI Agent Pool (Phase 9.4): pools per-enemy AI state blocks to avoid allocation churn. */
#ifndef ROGUE_AI_AGENT_POOL_H
#define ROGUE_AI_AGENT_POOL_H
#include <stddef.h>

struct EnemyAIBlackboard; /* opaque */

struct EnemyAIBlackboard* rogue_ai_agent_acquire(void);
void rogue_ai_agent_release(struct EnemyAIBlackboard* blk);

int rogue_ai_agent_pool_in_use(void);
int rogue_ai_agent_pool_free(void);
int rogue_ai_agent_pool_peak(void);
size_t rogue_ai_agent_pool_slab_size(void);

void rogue_ai_agent_pool_reset_for_tests(void);

#endif
