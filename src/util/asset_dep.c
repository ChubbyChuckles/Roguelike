/**
 * @file asset_dep.c
 * @brief Phase M3.4 asset dependency graph & hashing implementation for tracking
 *        asset relationships and computing content-based hashes.
 */

#include "asset_dep.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ROGUE_ASSET_DEP_CAP
#define ROGUE_ASSET_DEP_CAP 256
#endif
#ifndef ROGUE_ASSET_DEP_MAX_DEPS
#define ROGUE_ASSET_DEP_MAX_DEPS 16
#endif

/**
 * @brief Represents a node in the asset dependency graph.
 */
typedef struct RogueAssetDepNode
{
    char id[64];                               /**< Asset identifier */
    char path[256];                            /**< File system path to the asset */
    int dep_indices[ROGUE_ASSET_DEP_MAX_DEPS]; /**< Indices of dependency nodes */
    int dep_count;                             /**< Number of dependencies */
    unsigned long long cached_hash;            /**< Cached computed hash (0 = unknown) */
    unsigned char visiting;                    /**< DFS cycle detection state */
} RogueAssetDepNode;

static RogueAssetDepNode g_nodes[ROGUE_ASSET_DEP_CAP];
static int g_node_count = 0;

/**
 * @brief Resets the asset dependency graph, clearing all registered assets.
 */
void rogue_asset_dep_reset(void) { g_node_count = 0; }

/**
 * @brief Finds the index of a node by its ID.
 *
 * @param id The asset identifier to search for.
 * @return The node index, or -1 if not found.
 */
static int find_node(const char* id)
{
    if (!id)
        return -1;
    for (int i = 0; i < g_node_count; i++)
    {
        if (strcmp(g_nodes[i].id, id) == 0)
            return i;
    }
    return -1;
}

/**
 * @brief Computes a hash of a file's contents using FNV-1a.
 *
 * @param path The file path to hash.
 * @return The computed hash value.
 */
static unsigned long long hash_file(const char* path)
{
    if (!path || !*path)
        return 1469598103934665603ull; /* empty virtual node base */
    FILE* f = NULL;
    unsigned long long h = 1469598103934665603ull;
#if defined(_MSC_VER)
    fopen_s(&f, path, "rb");
#else
    f = fopen(path, "rb");
#endif
    if (!f)
        return h;
    unsigned char buf[1024];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0)
    {
        for (size_t i = 0; i < n; i++)
        {
            h ^= (unsigned long long) buf[i];
            h *= 1099511628211ull;
        }
    }
    fclose(f);
    return h;
}

/**
 * @brief Combines two hash values using FNV-1a multiplication.
 *
 * @param cur The current hash value.
 * @param child The child hash value to combine.
 * @return The combined hash value.
 */
static unsigned long long combine_hash(unsigned long long cur, unsigned long long child)
{
    cur ^= child;
    cur *= 1099511628211ull;
    return cur;
}

/**
 * @brief Performs DFS cycle detection on the dependency graph.
 *
 * @param idx The node index to check.
 * @return 0 if no cycle, -1 if cycle detected.
 */
static int dfs_cycle_check(int idx)
{
    RogueAssetDepNode* n = &g_nodes[idx];
    if (n->visiting == 1)
        return -1;
    if (n->visiting == 2)
        return 0;
    n->visiting = 1;
    for (int i = 0; i < n->dep_count; i++)
    {
        if (dfs_cycle_check(n->dep_indices[i]) < 0)
            return -1;
    }
    n->visiting = 2;
    return 0;
}

/**
 * @brief Checks for path conflicts in the dependency tree.
 *
 * @param idx The node index to check from.
 * @param path The path to check for conflicts.
 * @return 1 if conflict found, 0 otherwise.
 */
static int path_conflict_in_deps(int idx, const char* path)
{
    RogueAssetDepNode* n = &g_nodes[idx];
    for (int i = 0; i < n->dep_count; i++)
    {
        RogueAssetDepNode* c = &g_nodes[n->dep_indices[i]];
        if (path && *path && c->path[0] && strcmp(c->path, path) == 0)
            return 1;
        if (path_conflict_in_deps(n->dep_indices[i], path))
            return 1;
    }
    return 0;
}

/**
 * @brief Registers a new asset in the dependency graph.
 *
 * @param id The unique identifier for the asset.
 * @param path The file system path to the asset (can be NULL).
 * @param deps Array of dependency IDs.
 * @param dep_count Number of dependencies.
 * @return 0 on success, negative values indicate errors:
 *         -1: Asset ID already exists
 *         -2: Cycle or path conflict detected
 *         -3: Invalid parameters or capacity exceeded
 */
int rogue_asset_dep_register(const char* id, const char* path, const char** deps, int dep_count)
{
    if (!id || !*id || dep_count < 0)
        return -3;
    if (find_node(id) >= 0)
        return -1;
    if (g_node_count >= ROGUE_ASSET_DEP_CAP)
        return -3;
    if (dep_count > ROGUE_ASSET_DEP_MAX_DEPS)
        dep_count = ROGUE_ASSET_DEP_MAX_DEPS;
    RogueAssetDepNode* n = &g_nodes[g_node_count];
#if defined(_MSC_VER)
    strncpy_s(n->id, sizeof n->id, id, _TRUNCATE);
    if (path)
        strncpy_s(n->path, sizeof n->path, path, _TRUNCATE);
    else
        n->path[0] = '\0';
#else
    strncpy(n->id, id, sizeof n->id - 1);
    n->id[sizeof n->id - 1] = '\0';
    if (path)
    {
        strncpy(n->path, path, sizeof n->path - 1);
        n->path[sizeof n->path - 1] = '\0';
    }
    else
        n->path[0] = '\0';
#endif
    n->dep_count = 0;
    n->cached_hash = 0;
    n->visiting = 0;
    for (int i = 0; i < dep_count; i++)
    {
        n->dep_indices[i] = -1;
    }
    g_node_count++;
    /* Resolve dependencies to indices */
    for (int i = 0; i < dep_count; i++)
    {
        int di = find_node(deps[i]);
        if (di >= 0)
        {
            n->dep_indices[n->dep_count++] = di;
        }
    }
    /* Cycle detection including this new node */
    for (int i = 0; i < g_node_count; i++)
    {
        g_nodes[i].visiting = 0;
    }
    for (int i = 0; i < g_node_count; i++)
    {
        if (dfs_cycle_check(i) < 0)
        {
            g_nodes[g_node_count - 1].id[0] = '\0';
            g_node_count--;
            return -2;
        }
    }
    /* Path conflict: if any dependency chain already references a node with the same path as this
     * new node treat as cycle-like */
    for (int i = 0; i < n->dep_count; i++)
    {
        if (path_conflict_in_deps(n->dep_indices[i], n->path))
        {
            g_nodes[g_node_count - 1].id[0] = '\0';
            g_node_count--;
            return -2;
        }
    }
    return 0;
}

/**
 * @brief Invalidates cached hashes for an asset and all its dependents.
 *
 * @param id The asset identifier to invalidate.
 * @return 0 on success, -1 if asset not found.
 */
int rogue_asset_dep_invalidate(const char* id)
{
    int idx = find_node(id);
    if (idx < 0)
        return -1;
    /* Clear target node hash */
    g_nodes[idx].cached_hash = 0;
    /* Propagate: any ancestor depending (directly or indirectly) on this id must also be cleared.
       Simplicity over optimality: clear all cached hashes to avoid maintaining reverse adjacency
       lists. */
    for (int i = 0; i < g_node_count; i++)
        g_nodes[i].cached_hash = 0;
    return 0;
}

/**
 * @brief Computes the hash for a node, including all its dependencies.
 *
 * @param idx The node index to compute hash for.
 * @return The computed hash value.
 */
static unsigned long long compute_hash(int idx)
{
    RogueAssetDepNode* n = &g_nodes[idx];
    if (n->cached_hash)
        return n->cached_hash;
    unsigned long long h = hash_file(n->path);
    for (int i = 0; i < n->dep_count; i++)
    {
        h = combine_hash(h, compute_hash(n->dep_indices[i]));
    }
    n->cached_hash = h;
    return h;
}

/**
 * @brief Retrieves the computed hash for an asset, including all dependencies.
 *
 * @param id The asset identifier.
 * @param out_hash Pointer to store the computed hash.
 * @return 0 on success, -1 if asset not found or invalid parameters.
 */
int rogue_asset_dep_hash(const char* id, unsigned long long* out_hash)
{
    int idx = find_node(id);
    if (idx < 0 || !out_hash)
        return -1;
    *out_hash = compute_hash(idx);
    return 0;
}
