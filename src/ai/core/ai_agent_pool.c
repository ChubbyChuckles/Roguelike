/**
 * @file ai_agent_pool.c
 * @brief Simple fixed-slab free-list pool for EnemyAIBlackboard allocations.
 *
 * This module provides a minimal, non-thread-safe allocator that returns
 * zeroed, fixed-size slabs large enough to hold an EnemyAIBlackboard. It
 * maintains a singly-linked free list of pool nodes and lazily allocates
 * additional nodes on demand via malloc. Freed nodes are returned to the
 * free list for reuse. The allocator also exposes light statistics for tests
 * and diagnostics (in-use count, free count, peak/total created, slab size),
 * plus a hard reset helper intended solely for tests.
 *
 * Contract
 * - Inputs: acquire() takes no input; release() accepts a pointer previously
 *   returned by acquire(); query functions expose pool statistics.
 * - Outputs: acquire() returns a zero-initialized EnemyAIBlackboard pointer
 *   or NULL on allocation failure; release() has no output.
 * - Error modes: malloc failure on first creation of a node -> acquire() == NULL.
 * - Success criteria: Returned block is zeroed and sized to fit EnemyAIBlackboard.
 *
 * Thread-safety
 * - NOT thread-safe. Callers must provide external synchronization if used
 *   concurrently. Typical integration uses a higher-level AI system mutex.
 *
 * Performance & complexity
 * - acquire(): O(1). release(): O(1). Query functions: O(1) except
 *   rogue_ai_agent_pool_free(), which is O(N_free) to count the list.
 *
 * Implementation notes
 * - The slab payload size is 2048 bytes, which is large enough for the
 *   current EnemyAIBlackboard (~1 KB) with headroom. See rogue_ai_agent_pool_slab_size().
 * - release() uses offsetof(PoolNode, payload) to recover the node pointer
 *   from the client payload address; this relies on contiguous layout and
 *   is portable in standard C when applied to the exact object returned.
 */
#include "ai_agent_pool.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Internal pool node stored on the free list.
 *
 * Each node contains a next pointer and a fixed-size payload buffer used as
 * the client-visible block for EnemyAIBlackboard. Nodes are allocated with
 * malloc and recycled via the free list. This type is private to the module.
 */
typedef struct PoolNode
{
    struct PoolNode* next;
    unsigned char payload[2048]; /**< large enough for EnemyAIBlackboard (~1KB) */
} PoolNode;

/**< Head of the singly-linked free list. */
static PoolNode* g_free_list = NULL;
/**< Count of currently acquired (checked-out) blocks. */
static int g_in_use = 0;
/**< Peak number of nodes ever created during lifetime (monotonic). */
static int g_peak_created = 0;
/**< Total number of nodes created so far (tracks growth; may exceed free+in_use if reset in tests).
 */
static int g_total_created = 0;

/**
 * @brief Acquire a zeroed EnemyAIBlackboard slab from the pool.
 *
 * Reuses an existing node from the free list when available; otherwise
 * allocates a new node via malloc. The payload is memset to zeros before
 * being returned to the caller.
 *
 * @return struct EnemyAIBlackboard* Pointer to a zero-initialized slab on success,
 *         or NULL if memory allocation fails when growing the pool.
 * @note Not thread-safe. Callers must synchronize externally in concurrent contexts.
 */
struct EnemyAIBlackboard* rogue_ai_agent_acquire(void)
{
    PoolNode* n = g_free_list;
    if (n)
    {
        g_free_list = n->next;
    }
    else
    {
        n = (PoolNode*) malloc(sizeof(PoolNode));
        if (!n)
            return NULL;
        g_total_created++;
        if (g_total_created > g_peak_created)
            g_peak_created = g_total_created;
    }
    g_in_use++;
    memset(n->payload, 0, sizeof n->payload);
    return (struct EnemyAIBlackboard*) n->payload;
}

/**
 * @brief Release a previously acquired EnemyAIBlackboard back to the pool.
 *
 * The pointer must be one returned by rogue_ai_agent_acquire(). The block is
 * not freed to the system allocator; instead, its owning node is pushed back
 * onto the free list for reuse.
 *
 * @param blk Pointer previously returned by rogue_ai_agent_acquire(); NULL is ignored.
 * @note Safe to pass NULL (no-op). Not thread-safe.
 */
void rogue_ai_agent_release(struct EnemyAIBlackboard* blk)
{
    if (!blk)
        return;
    PoolNode* n = (PoolNode*) ((unsigned char*) blk - offsetof(PoolNode, payload));
    n->next = g_free_list;
    g_free_list = n;
    if (g_in_use > 0)
        g_in_use--;
}

/**
 * @brief Current number of checked-out (in-use) blocks.
 * @return int Count of in-use blocks.
 */
int rogue_ai_agent_pool_in_use(void) { return g_in_use; }

/**
 * @brief Current number of available blocks on the free list.
 *
 * This incurs an O(N) traversal of the free list to compute the count.
 *
 * @return int Count of free nodes available for reuse.
 */
int rogue_ai_agent_pool_free(void)
{
    int c = 0;
    for (PoolNode* n = g_free_list; n; n = n->next)
        c++;
    return c;
}
/**
 * @brief Peak number of nodes ever created during the allocator lifetime.
 * @return int Peak created node count (monotonic non-decreasing).
 */
int rogue_ai_agent_pool_peak(void) { return g_peak_created; }

/**
 * @brief Size in bytes of each slab payload returned by acquire().
 * @return size_t Payload size (currently 2048 bytes).
 */
size_t rogue_ai_agent_pool_slab_size(void) { return sizeof(((PoolNode*) 0)->payload); }

/**
 * @brief Test-only hard reset: frees all cached nodes and clears stats.
 *
 * Empties the free list by freeing its nodes, and resets in-use and
 * statistics counters to zero. Intended exclusively for test isolation.
 */
void rogue_ai_agent_pool_reset_for_tests(void)
{
    PoolNode* n = g_free_list;
    g_free_list = NULL;
    while (n)
    {
        PoolNode* nxt = n->next;
        free(n);
        n = nxt;
    }
    g_in_use = 0;
    g_peak_created = 0;
    g_total_created = 0;
}
