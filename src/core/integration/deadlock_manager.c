/**
 * @file deadlock_manager.c
 * @brief Implementation of deadlock detection and prevention system for resource management.
 *
 * This module provides a comprehensive deadlock detection and resolution system that:
 * - Tracks resource dependencies between transactions
 * - Detects deadlock cycles using depth-first search
 * - Automatically resolves deadlocks by aborting victim transactions
 * - Maintains detailed statistics and cycle logs for monitoring
 * - Supports configurable resource limits and callback mechanisms
 *
 * The system uses a wait-for graph approach with periodic cycle detection
 * during tick operations. When deadlocks are detected, victims are selected
 * and aborted to break the cycle, with all resources released for redistribution.
 *
 * @author Rogue Game Development Team
 * @date 2025
 * @version 1.0
 *
 * @note This implementation is part of Phase 5.6 of the roguelike game development.
 * @see deadlock_manager.h for public API declarations
 */

#include "deadlock_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Internal structure tracking resource ownership and waiting transactions.
 *
 * Maintains the current holder of a resource and the queue of transactions
 * waiting to acquire it. Uses a simple array-based queue for waiters.
 */
typedef struct ResourceWaiters
{
    int id;                                  /**< Resource identifier */
    int holder_tx;                           /**< Current holder transaction ID (-1 for none) */
    int waiters[ROGUE_DEADLOCK_MAX_WAITERS]; /**< Queue of waiting transaction IDs */
    uint8_t wait_count;                      /**< Number of transactions waiting */
} ResourceWaiters;

/**
 * @brief Global array of resource waiter structures.
 *
 * Tracks the ownership and waiting queue for each registered resource.
 * Indexed by resource ID for O(1) access.
 */
static ResourceWaiters g_resources[ROGUE_DEADLOCK_MAX_RESOURCES];

/**
 * @brief Bitmap tracking which resources are currently registered.
 *
 * Used to distinguish between unused resource slots and registered resources.
 * Provides O(1) lookup for resource validity.
 */
static uint8_t g_resource_used[ROGUE_DEADLOCK_MAX_RESOURCES];

/**
 * @brief Global statistics structure for deadlock manager operations.
 *
 * Accumulates counters for all deadlock manager activities including
 * acquisitions, waits, deadlocks detected, and resolutions.
 */
static RogueDeadlockStats g_stats;

/**
 * @brief Sequence counter for deadlock cycle logging.
 *
 * Provides unique identifiers for each detected deadlock cycle.
 * Incremented for each new cycle logged.
 */
static uint64_t g_cycle_seq = 1;

/**
 * @brief Internal structure representing the state of a transaction.
 *
 * Tracks which resources a transaction currently holds and which resource
 * it is waiting to acquire. Uses bitmasks for efficient resource ownership
 * tracking across up to 128 resources.
 */
typedef struct TxState
{
    int id;                  /**< Transaction identifier (0 = unused slot) */
    uint64_t hold_mask_low;  /**< Bitmask for resources 0-63 */
    uint64_t hold_mask_high; /**< Bitmask for resources 64-127 */
    int waiting_for;         /**< Resource ID being waited for (-1 = not waiting) */
} TxState;

/**
 * @brief Global array of transaction states.
 *
 * Maintains the current state of all active transactions in the system.
 * Provides O(n) lookup where n is the number of active transactions.
 */
static TxState g_txs[ROGUE_DEADLOCK_MAX_TX];

/**
 * @brief Circular buffer for logging detected deadlock cycles.
 *
 * Stores the most recent deadlock cycles for debugging and analysis.
 * Uses a circular buffer to maintain a fixed history size.
 */
static RogueDeadlockCycle g_cycle_log[ROGUE_DEADLOCK_CYCLE_LOG];

/**
 * @brief Current count of logged cycles (up to ROGUE_DEADLOCK_CYCLE_LOG).
 */
static size_t g_cycle_count = 0;

/**
 * @brief Head index for the cycle log circular buffer.
 */
static size_t g_cycle_head = 0;

/**
 * @brief Callback function for transaction abortion.
 *
 * Called when a transaction is selected as a deadlock victim.
 * Allows the application to perform cleanup or notification.
 */
static int (*g_abort_cb)(int tx_id, const char* reason) = NULL;

/**
 * @brief Retrieves or creates a transaction state entry.
 *
 * Looks up an existing transaction state by ID, or creates a new one if the
 * transaction doesn't exist yet. Uses linear search through the transaction array.
 *
 * @param tx_id The transaction identifier to look up (must be > 0)
 * @return Pointer to the transaction state, or NULL if tx_id is invalid or no slots available
 *
 * @note Creates new transaction entries on first access
 * @note O(n) lookup where n is the number of active transactions
 */
static TxState* tx_get(int tx_id)
{
    if (tx_id <= 0)
        return NULL;
    for (int i = 0; i < ROGUE_DEADLOCK_MAX_TX; i++)
        if (g_txs[i].id == tx_id)
            return &g_txs[i];
    for (int i = 0; i < ROGUE_DEADLOCK_MAX_TX; i++)
        if (g_txs[i].id == 0)
        {
            g_txs[i].id = tx_id;
            g_txs[i].waiting_for = -1;
            return &g_txs[i];
        }
    return NULL;
}

/**
 * @brief Validates and converts a resource ID to array index.
 *
 * Checks if the resource ID is within valid bounds and returns the corresponding
 * array index for the global resource arrays.
 *
 * @param resource_id The resource identifier to validate
 * @return Array index for the resource, or -1 if invalid
 *
 * @note Resource IDs must be in range [0, ROGUE_DEADLOCK_MAX_RESOURCES)
 */
static int res_index(int resource_id)
{
    if (resource_id < 0 || resource_id >= ROGUE_DEADLOCK_MAX_RESOURCES)
        return -1;
    return resource_id;
}

/**
 * @brief Registers a resource for deadlock tracking.
 *
 * Initializes a resource slot for the given resource ID, making it available
 * for acquisition and deadlock detection. Resources must be registered before
 * they can be acquired by transactions.
 *
 * @param resource_id The resource identifier to register (must be valid index)
 * @return 0 on success, -1 if resource_id is out of bounds
 *
 * @note Safe to call multiple times on the same resource
 * @note Updates the resources_registered statistic
 */
int rogue_deadlock_register_resource(int resource_id)
{
    int ri = res_index(resource_id);
    if (ri < 0)
        return -1;
    if (!g_resource_used[ri])
    {
        memset(&g_resources[ri], 0, sizeof(ResourceWaiters));
        g_resources[ri].id = resource_id;
        g_resources[ri].holder_tx = -1;
        g_resource_used[ri] = 1;
        g_stats.resources_registered++;
    }
    return 0;
}

/**
 * @brief Adds a resource to a transaction's hold mask.
 *
 * Marks the specified resource as being held by the given transaction
 * by setting the appropriate bit in the transaction's hold mask.
 *
 * @param tx Pointer to the transaction state
 * @param resource_id The resource ID to add to the hold mask
 *
 * @note Uses bit manipulation for O(1) operation
 */
static void tx_add_hold(TxState* tx, int resource_id)
{
    if (resource_id < 64)
        tx->hold_mask_low |= (1ULL << resource_id);
    else
        tx->hold_mask_high |= (1ULL << (resource_id - 64));
}

/**
 * @brief Checks if a transaction holds a specific resource.
 *
 * Tests whether the given resource is currently held by the transaction
 * by checking the appropriate bit in the hold mask.
 *
 * @param tx Pointer to the transaction state
 * @param resource_id The resource ID to check
 * @return 1 if the transaction holds the resource, 0 otherwise
 *
 * @note Uses bit manipulation for O(1) operation
 */
static int tx_holds(TxState* tx, int resource_id)
{
    if (resource_id < 64)
        return (tx->hold_mask_low & (1ULL << resource_id)) ? 1 : 0;
    return (tx->hold_mask_high & (1ULL << (resource_id - 64))) ? 1 : 0;
}
/**
 * @brief Public API to check if a transaction holds a resource.
 *
 * Determines whether the specified transaction currently holds the given resource.
 * This is a public interface to the internal tx_holds function.
 *
 * @param tx_id The transaction identifier
 * @param resource_id The resource identifier
 * @return 1 if the transaction holds the resource, 0 otherwise
 *
 * @note Returns 0 if transaction doesn't exist
 */
int rogue_deadlock_tx_holds(int tx_id, int resource_id)
{
    TxState* tx = tx_get(tx_id);
    if (!tx)
        return 0;
    return tx_holds(tx, resource_id);
}

/**
 * @brief Attempts to acquire a resource for a transaction.
 *
 * Tries to acquire the specified resource for the given transaction. If the
 * resource is available, it is immediately granted. If held by another transaction,
 * the requesting transaction is added to the wait queue.
 *
 * @param tx_id The transaction identifier requesting the resource
 * @param resource_id The resource identifier to acquire
 * @return 0 if acquired immediately, 1 if queued to wait, -1 if invalid resource,
 *         -2 if invalid transaction, -3 if wait queue is full
 *
 * @note Updates acquisitions or waits statistics accordingly
 */
int rogue_deadlock_acquire(int tx_id, int resource_id)
{
    int ri = res_index(resource_id);
    if (ri < 0 || !g_resource_used[ri])
        return -1;
    TxState* tx = tx_get(tx_id);
    if (!tx)
        return -2;
    ResourceWaiters* r = &g_resources[ri];
    if (r->holder_tx < 0)
    {
        r->holder_tx = tx_id;
        tx_add_hold(tx, resource_id);
        g_stats.acquisitions++;
        return 0;
    }
    if (r->holder_tx == tx_id)
        return 0; // already holds
    for (int i = 0; i < r->wait_count; i++)
        if (r->waiters[i] == tx_id)
            return 1;
    if (r->wait_count >= ROGUE_DEADLOCK_MAX_WAITERS)
        return -3;
    r->waiters[r->wait_count++] = tx_id;
    tx->waiting_for = resource_id;
    g_stats.waits++;
    return 1;
}

/**
 * @brief Promotes the next waiting transaction to resource holder.
 *
 * Removes the first transaction from the wait queue and makes it the new holder
 * of the resource. Updates the transaction's hold mask and waiting status.
 *
 * @param r Pointer to the resource waiter structure
 * @return 1 if a waiter was promoted, 0 if no waiters in queue
 *
 * @note Updates wait_promotions statistic
 */
static int promote_waiter(ResourceWaiters* r)
{
    if (r->wait_count == 0)
        return 0;
    int tx_id = r->waiters[0];
    for (int i = 1; i < r->wait_count; i++)
        r->waiters[i - 1] = r->waiters[i];
    r->wait_count--;
    r->holder_tx = tx_id;
    TxState* tx = tx_get(tx_id);
    if (tx)
    {
        tx_add_hold(tx, r->id);
        if (tx->waiting_for == r->id)
            tx->waiting_for = -1;
    }
    g_stats.wait_promotions++;
    return 1;
}

/**
 * @brief Releases a specific resource held by a transaction.
 *
 * Releases the specified resource if it is currently held by the given transaction.
 * If there are waiting transactions, the next waiter is promoted to holder.
 *
 * @param tx_id The transaction identifier releasing the resource
 * @param resource_id The resource identifier to release
 * @return 0 on success, -1 if invalid resource, -2 if transaction doesn't hold the resource
 *
 * @note Updates releases statistic
 * @note Automatically promotes next waiter if queue is not empty
 */
int rogue_deadlock_release(int tx_id, int resource_id)
{
    int ri = res_index(resource_id);
    if (ri < 0 || !g_resource_used[ri])
        return -1;
    ResourceWaiters* r = &g_resources[ri];
    if (r->holder_tx != tx_id)
        return -2;
    r->holder_tx = -1;
    promote_waiter(r);
    g_stats.releases++;
    return 0;
}

/**
 * @brief Releases all resources held by a transaction.
 *
 * Releases all resources currently held by the specified transaction and
 * removes it from all wait queues. Used during transaction abort or commit.
 *
 * @param tx_id The transaction identifier to release all resources for
 * @return Number of resources released
 *
 * @note Updates releases statistic for each resource released
 * @note Also removes transaction from wait queues of resources it was waiting for
 * @note Resets transaction state to clean slate
 */
int rogue_deadlock_release_all(int tx_id)
{
    int released = 0;
    TxState* tx = tx_get(tx_id);
    if (!tx)
        return 0;
    for (int ri = 0; ri < ROGUE_DEADLOCK_MAX_RESOURCES; ri++)
    {
        if (!g_resource_used[ri])
            continue;
        if (g_resources[ri].holder_tx == tx_id)
        {
            g_resources[ri].holder_tx = -1;
            promote_waiter(&g_resources[ri]);
            released++;
            g_stats.releases++;
        }
        if (g_resources[ri].wait_count)
        {
            int w = 0;
            for (int i = 0; i < g_resources[ri].wait_count; i++)
            {
                if (g_resources[ri].waiters[i] == tx_id)
                {
                }
                else
                {
                    g_resources[ri].waiters[w++] = g_resources[ri].waiters[i];
                }
            }
            g_resources[ri].wait_count = (uint8_t) w;
        }
    }
    tx->hold_mask_low = tx->hold_mask_high = 0;
    tx->waiting_for = -1;
    tx->id = tx_id;
    return released;
}

/**
 * @brief Performs depth-first search to detect deadlock cycles.
 *
 * Recursively traverses the wait-for graph starting from start_tx to detect
 * cycles that indicate deadlocks. Uses a path array to track the current
 * traversal and avoid infinite loops.
 *
 * @param start_tx The transaction where the search started (cycle detection point)
 * @param cur_tx The current transaction being examined
 * @param path Array to track the current path (prevents cycles in search)
 * @param depth Current depth in the search tree
 * @param out_cycle Output array to store the detected cycle (if found)
 * @param out_len Output parameter for the length of the detected cycle
 * @return 1 if a cycle is detected, 0 otherwise
 *
 * @note Limited to maximum depth of 16 to prevent stack overflow
 * @note Uses the wait-for relationship: tx waits for resource held by another tx
 */
static int dfs_cycle(int start_tx, int cur_tx, int* path, int depth, int* out_cycle,
                     size_t* out_len)
{
    if (depth >= 16)
        return 0;
    path[depth] = cur_tx;
    TxState* cur = tx_get(cur_tx);
    if (!cur || cur->waiting_for < 0)
        return 0;
    ResourceWaiters* r = &g_resources[cur->waiting_for];
    if (r->holder_tx < 0)
        return 0;
    int next_tx = r->holder_tx;
    if (next_tx == start_tx)
    {
        size_t len = depth + 1;
        if (out_cycle)
        {
            for (size_t i = 0; i < len && i < 16; i++)
                out_cycle[i] = path[i];
        }
        *out_len = len;
        return 1;
    }
    for (int i = 0; i <= depth; i++)
        if (path[i] == next_tx)
            return 0;
    return dfs_cycle(start_tx, next_tx, path, depth + 1, out_cycle, out_len);
}

/**
 * @brief Logs a detected deadlock cycle for debugging and analysis.
 *
 * Records the deadlock cycle information in the circular log buffer,
 * including the sequence number, participating transactions, and victim.
 *
 * @param tx_ids Array of transaction IDs in the cycle
 * @param len Number of transactions in the cycle
 * @param victim The transaction ID selected as victim
 *
 * @note Uses circular buffer - older entries are overwritten when full
 * @note Updates g_cycle_count and g_cycle_head indices
 */
static void log_cycle(int* tx_ids, size_t len, int victim)
{
    RogueDeadlockCycle* c = &g_cycle_log[g_cycle_head];
    memset(c, 0, sizeof(*c));
    c->seq = g_cycle_seq++;
    c->tx_count = len > 16 ? 16 : len;
    for (size_t i = 0; i < c->tx_count; i++)
        c->tx_ids[i] = tx_ids[i];
    c->victim_tx_id = victim;
    g_cycle_head = (g_cycle_head + 1) % ROGUE_DEADLOCK_CYCLE_LOG;
    if (g_cycle_count < ROGUE_DEADLOCK_CYCLE_LOG)
        g_cycle_count++;
}
/**
 * @brief Selects a victim transaction from a deadlock cycle.
 *
 * Chooses which transaction to abort to break the deadlock cycle.
 * Uses a simple heuristic: selects the transaction with the highest ID.
 *
 * @param tx_ids Array of transaction IDs in the cycle
 * @param len Number of transactions in the cycle
 * @return The transaction ID selected as victim
 *
 * @note Current implementation uses highest ID as tiebreaker
 * @note Could be extended with more sophisticated victim selection algorithms
 */
static int choose_victim(int* tx_ids, size_t len)
{
    int victim = tx_ids[0];
    for (size_t i = 1; i < len; i++)
        if (tx_ids[i] > victim)
            victim = tx_ids[i];
    return victim;
}

/**
 * @brief Performs periodic deadlock detection and resolution.
 *
 * Scans all active transactions to detect deadlock cycles using DFS.
 * When a deadlock is found, selects a victim transaction, aborts it,
 * and releases all its resources. This should be called periodically
 * to maintain system liveness.
 *
 * @param now_ms Current timestamp in milliseconds (currently unused)
 * @return Number of deadlocks resolved in this tick
 *
 * @note Updates ticks, deadlocks_detected, and victims_aborted statistics
 * @note Calls the abort callback if registered
 * @note O(n*m) complexity where n=active transactions, m=average cycle depth
 */
int rogue_deadlock_tick(uint64_t now_ms)
{
    (void) now_ms;
    g_stats.ticks++;
    int resolved = 0;
    int path[32];
    int cyc[16];
    size_t clen = 0;
    for (int i = 0; i < ROGUE_DEADLOCK_MAX_TX; i++)
    {
        if (!g_txs[i].id)
            continue;
        TxState* tx = &g_txs[i];
        if (tx->waiting_for < 0)
            continue;
        clen = 0;
        if (dfs_cycle(tx->id, tx->id, path, 0, cyc, &clen))
        {
            g_stats.deadlocks_detected++;
            int victim = choose_victim(cyc, clen);
            if (g_abort_cb)
                g_abort_cb(victim, "deadlock victim");
            rogue_deadlock_release_all(victim);
            g_stats.victims_aborted++;
            log_cycle(cyc, clen, victim);
            resolved++;
        }
    }
    return resolved;
}

/**
 * @brief Retrieves a snapshot of deadlock manager statistics.
 *
 * Copies the current global statistics into the provided structure
 * for monitoring and analysis purposes.
 *
 * @param out Pointer to output statistics structure (must not be NULL)
 *
 * @see RogueDeadlockStats for the structure definition
 */
void rogue_deadlock_get_stats(RogueDeadlockStats* out)
{
    if (!out)
        return;
    *out = g_stats;
}
/**
 * @brief Retrieves logged deadlock cycle information.
 *
 * Provides access to the circular buffer of logged deadlock cycles
 * for debugging and analysis. Returns the current log entries and count.
 *
 * @param out_cycles Output pointer to receive the cycle log array
 * @param count Output pointer to receive the number of logged cycles
 * @return 0 on success, -1 if parameters are NULL
 *
 * @note The returned array is the internal circular buffer
 * @note Cycles are logged in chronological order within the buffer
 */
int rogue_deadlock_cycles_get(const RogueDeadlockCycle** out_cycles, size_t* count)
{
    if (!out_cycles || !count)
        return -1;
    *out_cycles = g_cycle_log;
    *count = g_cycle_count;
    return 0;
}
/**
 * @brief Dumps comprehensive deadlock manager state to a file stream.
 *
 * Outputs detailed information about the current state of the deadlock manager,
 * including global statistics, logged deadlock cycles, and their transaction paths.
 * Useful for debugging and monitoring system health.
 *
 * @param fptr Pointer to FILE stream (cast to void* for C compatibility), NULL uses stdout
 *
 * @note Output format is human-readable for debugging purposes
 * @note Shows cycle paths in A->B->C format indicating wait-for relationships
 */
void rogue_deadlock_dump(void* fptr)
{
    FILE* f = fptr ? (FILE*) fptr : stdout;
    fprintf(f,
            "[deadlock] res=%llu acq=%llu waits=%llu dl=%llu victims=%llu rel=%llu ticks=%llu "
            "promotions=%llu cycles_logged=%zu\n",
            (unsigned long long) g_stats.resources_registered,
            (unsigned long long) g_stats.acquisitions, (unsigned long long) g_stats.waits,
            (unsigned long long) g_stats.deadlocks_detected,
            (unsigned long long) g_stats.victims_aborted, (unsigned long long) g_stats.releases,
            (unsigned long long) g_stats.ticks, (unsigned long long) g_stats.wait_promotions,
            g_cycle_count);
    for (size_t i = 0; i < g_cycle_count; i++)
    {
        const RogueDeadlockCycle* c = &g_cycle_log[i];
        fprintf(f, " cycle seq=%llu count=%zu victim=%d path=", (unsigned long long) c->seq,
                c->tx_count, c->victim_tx_id);
        for (size_t j = 0; j < c->tx_count; j++)
            fprintf(f, "%d%s", c->tx_ids[j], j + 1 < c->tx_count ? "->" : "");
        fprintf(f, "\n");
    }
}
/**
 * @brief Resets all deadlock manager state to initial conditions.
 *
 * Clears all resources, transactions, statistics, and cycle logs.
 * Returns the system to a clean slate as if just initialized.
 *
 * @note This is a destructive operation - all state is lost
 * @note Useful for testing or system reset scenarios
 */
void rogue_deadlock_reset_all(void)
{
    memset(&g_resources, 0, sizeof(g_resources));
    memset(&g_resource_used, 0, sizeof(g_resource_used));
    memset(&g_txs, 0, sizeof(g_txs));
    memset(&g_stats, 0, sizeof(g_stats));
    memset(&g_cycle_log, 0, sizeof(g_cycle_log));
    g_cycle_count = g_cycle_head = 0;
    g_cycle_seq = 1;
}

/**
 * @brief Sets the callback function for transaction abortion.
 *
 * Registers a callback that will be invoked when a transaction is selected
 * as a deadlock victim and needs to be aborted. The callback allows the
 * application to perform cleanup or notification.
 *
 * @param fn Callback function pointer, or NULL to disable callbacks
 *
 * @note The callback receives the transaction ID and a reason string
 * @note Return value from callback is currently ignored
 */
void rogue_deadlock_set_abort_callback(int (*fn)(int tx_id, const char* reason))
{
    g_abort_cb = fn;
}

/**
 * @brief Handles transaction abortion cleanup.
 *
 * Releases all resources held by the specified transaction.
 * Called when a transaction is being aborted (either by deadlock resolution
 * or explicit abort).
 *
 * @param tx_id The transaction identifier being aborted
 *
 * @see rogue_deadlock_release_all for implementation details
 */
void rogue_deadlock_on_tx_abort(int tx_id) { rogue_deadlock_release_all(tx_id); }

/**
 * @brief Handles transaction commit cleanup.
 *
 * Releases all resources held by the specified transaction.
 * Called when a transaction is committing to ensure clean resource state.
 *
 * @param tx_id The transaction identifier being committed
 *
 * @note Same implementation as abort - releases all held resources
 * @see rogue_deadlock_release_all for implementation details
 */
void rogue_deadlock_on_tx_commit(int tx_id) { rogue_deadlock_release_all(tx_id); }
