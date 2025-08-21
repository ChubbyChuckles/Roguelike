#include "dependency_manager.h"
#include <time.h>
#include <assert.h>

#ifdef _WIN32
#include <Windows.h>
#pragma comment(lib, "kernel32.lib")
#define _CRT_SECURE_NO_WARNINGS
#pragma warning(push)
#pragma warning(disable: 4996)
#else
#include <sys/time.h>
#include <unistd.h>
#endif

// Helper function to get current time in milliseconds
static uint64_t get_current_time_ms(void) {
#ifdef _WIN32
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (uint64_t)(counter.QuadPart * 1000 / frequency.QuadPart);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
#endif
}

// Helper function to compute simple string hash
static uint32_t compute_string_hash(const char* str) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

// Core management functions
RogueDependencyManager* rogue_dependency_manager_create(void) {
    RogueDependencyManager* manager = (RogueDependencyManager*)calloc(1, sizeof(RogueDependencyManager));
    if (!manager) {
        return NULL;
    }
    
    // Initialize default configuration
    manager->auto_resolve = true;
    manager->strict_mode = false;
    manager->debug_mode = false;
    
    return manager;
}

void rogue_dependency_manager_destroy(RogueDependencyManager* manager) {
    if (!manager) return;
    
    rogue_dependency_manager_cleanup(manager);
    free(manager);
}

bool rogue_dependency_manager_initialize(RogueDependencyManager* manager) {
    if (!manager) return false;
    
    // Reset all graph data
    memset(&manager->graph, 0, sizeof(RogueDependencyGraph));
    manager->graph.is_valid = false;
    
    // Reset statistics
    manager->total_dependencies = 0;
    manager->resolved_dependencies = 0;
    manager->failed_resolutions = 0;
    manager->circular_dependencies = 0;
    manager->last_resolve_time_ms = 0;
    manager->total_resolve_time_ms = 0;
    manager->resolve_count = 0;
    
    return true;
}

void rogue_dependency_manager_cleanup(RogueDependencyManager* manager) {
    if (!manager) return;
    
    // Clear all graph data
    memset(&manager->graph, 0, sizeof(RogueDependencyGraph));
    
    // Reset statistics
    manager->total_dependencies = 0;
    manager->resolved_dependencies = 0;
    manager->failed_resolutions = 0;
    manager->circular_dependencies = 0;
}

void rogue_dependency_manager_reset(RogueDependencyManager* manager) {
    if (!manager) return;
    
    rogue_dependency_manager_cleanup(manager);
    rogue_dependency_manager_initialize(manager);
}

// Configuration functions
void rogue_dependency_manager_set_auto_resolve(RogueDependencyManager* manager, bool auto_resolve) {
    if (manager) {
        manager->auto_resolve = auto_resolve;
    }
}

void rogue_dependency_manager_set_strict_mode(RogueDependencyManager* manager, bool strict_mode) {
    if (manager) {
        manager->strict_mode = strict_mode;
    }
}

void rogue_dependency_manager_set_debug_mode(RogueDependencyManager* manager, bool debug_mode) {
    if (manager) {
        manager->debug_mode = debug_mode;
    }
}

// Node management functions
bool rogue_dependency_manager_add_file(RogueDependencyManager* manager, const char* file_path, RogueFileType file_type, int priority) {
    if (!manager || !file_path || !rogue_dependency_manager_is_valid_file_path(file_path)) {
        return false;
    }
    
    // Check if file already exists
    if (rogue_dependency_manager_find_node(manager, file_path)) {
        return false;  // File already exists
    }
    
    // Check if we have space for more files
    if (manager->graph.node_count >= ROGUE_DEP_MAX_FILES) {
        return false;
    }
    
    // Add new node
    RogueDependencyNode* node = &manager->graph.nodes[manager->graph.node_count];
    strncpy(node->file_path, file_path, ROGUE_DEP_MAX_PATH_LENGTH - 1);
    node->file_path[ROGUE_DEP_MAX_PATH_LENGTH - 1] = '\0';
    node->file_type = file_type;
    node->priority = priority;
    node->is_loaded = false;
    node->last_modified = 0;
    node->checksum[0] = '\0';
    node->dependency_count = 0;
    node->visited = false;
    node->in_path = false;
    node->discovery_time = -1;
    node->finish_time = -1;
    
    manager->graph.node_count++;
    manager->graph.is_valid = false;  // Graph needs re-validation
    
    if (manager->debug_mode) {
        printf("[DEBUG] Added file: %s (type: %s, priority: %d)\n", 
               file_path, rogue_dependency_manager_get_file_type_name(file_type), priority);
    }
    
    return true;
}

bool rogue_dependency_manager_remove_file(RogueDependencyManager* manager, const char* file_path) {
    if (!manager || !file_path) return false;
    
    // Find the node
    int node_index = -1;
    for (int i = 0; i < manager->graph.node_count; i++) {
        if (strcmp(manager->graph.nodes[i].file_path, file_path) == 0) {
            node_index = i;
            break;
        }
    }
    
    if (node_index == -1) return false;
    
    // Remove dependencies from/to this file
    for (int i = 0; i < manager->graph.node_count; i++) {
        RogueDependencyNode* node = &manager->graph.nodes[i];
        for (int j = node->dependency_count - 1; j >= 0; j--) {
            if (strcmp(node->dependencies[j].target_file, file_path) == 0 ||
                strcmp(node->dependencies[j].source_file, file_path) == 0) {
                // Remove this dependency
                memmove(&node->dependencies[j], &node->dependencies[j + 1], 
                        (node->dependency_count - j - 1) * sizeof(RogueDependency));
                node->dependency_count--;
                manager->total_dependencies--;
            }
        }
    }
    
    // Remove the node by shifting remaining nodes
    memmove(&manager->graph.nodes[node_index], &manager->graph.nodes[node_index + 1],
            (manager->graph.node_count - node_index - 1) * sizeof(RogueDependencyNode));
    manager->graph.node_count--;
    manager->graph.is_valid = false;
    
    if (manager->debug_mode) {
        printf("[DEBUG] Removed file: %s\n", file_path);
    }
    
    return true;
}

RogueDependencyNode* rogue_dependency_manager_find_node(RogueDependencyManager* manager, const char* file_path) {
    if (!manager || !file_path) return NULL;
    
    for (int i = 0; i < manager->graph.node_count; i++) {
        if (strcmp(manager->graph.nodes[i].file_path, file_path) == 0) {
            return &manager->graph.nodes[i];
        }
    }
    
    return NULL;
}

bool rogue_dependency_manager_update_file_info(RogueDependencyManager* manager, const char* file_path, uint64_t last_modified, const char* checksum) {
    if (!manager || !file_path || !checksum) return false;
    
    RogueDependencyNode* node = rogue_dependency_manager_find_node(manager, file_path);
    if (!node) return false;
    
    node->last_modified = last_modified;
    strncpy(node->checksum, checksum, sizeof(node->checksum) - 1);
    node->checksum[sizeof(node->checksum) - 1] = '\0';
    
    return true;
}

// Dependency registration functions
bool rogue_dependency_manager_add_dependency(RogueDependencyManager* manager, const char* source_file, const char* target_file, 
                                           const char* reference_key, RogueDependencyType type, int priority, const char* description) {
    if (!manager || !source_file || !target_file || !reference_key) {
        return false;
    }
    
    // Find source node
    RogueDependencyNode* source_node = rogue_dependency_manager_find_node(manager, source_file);
    if (!source_node) {
        // Auto-add source file if not found
        RogueFileType file_type = rogue_dependency_manager_get_file_type_from_path(source_file);
        if (!rogue_dependency_manager_add_file(manager, source_file, file_type, priority)) {
            return false;
        }
        source_node = rogue_dependency_manager_find_node(manager, source_file);
    }
    
    // Check if target node exists (for strong dependencies)
    RogueDependencyNode* target_node = rogue_dependency_manager_find_node(manager, target_file);
    if (!target_node && type == ROGUE_DEP_TYPE_STRONG) {
        // Auto-add target file for strong dependencies
        RogueFileType file_type = rogue_dependency_manager_get_file_type_from_path(target_file);
        if (!rogue_dependency_manager_add_file(manager, target_file, file_type, priority - 1)) {
            return false;
        }
    }
    
    // Check if dependency already exists
    for (int i = 0; i < source_node->dependency_count; i++) {
        if (strcmp(source_node->dependencies[i].target_file, target_file) == 0 &&
            strcmp(source_node->dependencies[i].reference_key, reference_key) == 0) {
            return false;  // Dependency already exists
        }
    }
    
    // Check if we have space for more dependencies
    if (source_node->dependency_count >= ROGUE_DEP_MAX_DEPENDENCIES) {
        return false;
    }
    
    // Add new dependency
    RogueDependency* dep = &source_node->dependencies[source_node->dependency_count];
    strncpy(dep->source_file, source_file, ROGUE_DEP_MAX_PATH_LENGTH - 1);
    dep->source_file[ROGUE_DEP_MAX_PATH_LENGTH - 1] = '\0';
    strncpy(dep->target_file, target_file, ROGUE_DEP_MAX_PATH_LENGTH - 1);
    dep->target_file[ROGUE_DEP_MAX_PATH_LENGTH - 1] = '\0';
    strncpy(dep->reference_key, reference_key, ROGUE_DEP_MAX_NAME_LENGTH - 1);
    dep->reference_key[ROGUE_DEP_MAX_NAME_LENGTH - 1] = '\0';
    dep->type = type;
    dep->status = ROGUE_DEP_STATUS_UNRESOLVED;
    dep->priority = priority;
    
    if (description) {
        strncpy(dep->description, description, ROGUE_DEP_MAX_NAME_LENGTH - 1);
        dep->description[ROGUE_DEP_MAX_NAME_LENGTH - 1] = '\0';
    } else {
        snprintf(dep->description, ROGUE_DEP_MAX_NAME_LENGTH, "%s->%s:%s", source_file, target_file, reference_key);
    }
    
    source_node->dependency_count++;
    manager->total_dependencies++;
    manager->graph.is_valid = false;
    
    if (manager->debug_mode) {
        printf("[DEBUG] Added dependency: %s -> %s (%s)\n", source_file, target_file, reference_key);
    }
    
    // Auto-resolve if enabled
    if (manager->auto_resolve) {
        rogue_dependency_manager_resolve_file(manager, source_file);
    }
    
    return true;
}

bool rogue_dependency_manager_remove_dependency(RogueDependencyManager* manager, const char* source_file, const char* target_file, const char* reference_key) {
    if (!manager || !source_file || !target_file || !reference_key) {
        return false;
    }
    
    RogueDependencyNode* source_node = rogue_dependency_manager_find_node(manager, source_file);
    if (!source_node) return false;
    
    // Find and remove the dependency
    for (int i = 0; i < source_node->dependency_count; i++) {
        RogueDependency* dep = &source_node->dependencies[i];
        if (strcmp(dep->target_file, target_file) == 0 &&
            strcmp(dep->reference_key, reference_key) == 0) {
            // Remove this dependency by shifting remaining dependencies
            memmove(&source_node->dependencies[i], &source_node->dependencies[i + 1],
                    (source_node->dependency_count - i - 1) * sizeof(RogueDependency));
            source_node->dependency_count--;
            manager->total_dependencies--;
            manager->graph.is_valid = false;
            
            if (manager->debug_mode) {
                printf("[DEBUG] Removed dependency: %s -> %s (%s)\n", source_file, target_file, reference_key);
            }
            
            return true;
        }
    }
    
    return false;
}

int rogue_dependency_manager_get_dependencies(RogueDependencyManager* manager, const char* source_file, RogueDependency* dependencies, int max_dependencies) {
    if (!manager || !source_file || !dependencies || max_dependencies <= 0) {
        return 0;
    }
    
    RogueDependencyNode* source_node = rogue_dependency_manager_find_node(manager, source_file);
    if (!source_node) return 0;
    
    int copy_count = (source_node->dependency_count < max_dependencies) ? source_node->dependency_count : max_dependencies;
    memcpy(dependencies, source_node->dependencies, copy_count * sizeof(RogueDependency));
    
    return copy_count;
}

// Dependency resolution functions
bool rogue_dependency_manager_resolve_all(RogueDependencyManager* manager) {
    if (!manager) return false;
    
    uint64_t start_time = get_current_time_ms();
    
    bool all_resolved = true;
    manager->resolved_dependencies = 0;
    manager->failed_resolutions = 0;
    manager->graph.unresolved_count = 0;
    manager->graph.missing_count = 0;
    
    // Reset all dependency statuses
    for (int i = 0; i < manager->graph.node_count; i++) {
        RogueDependencyNode* node = &manager->graph.nodes[i];
        for (int j = 0; j < node->dependency_count; j++) {
            node->dependencies[j].status = ROGUE_DEP_STATUS_UNRESOLVED;
        }
    }
    
    // Resolve each dependency
    for (int i = 0; i < manager->graph.node_count; i++) {
        RogueDependencyNode* node = &manager->graph.nodes[i];
        for (int j = 0; j < node->dependency_count; j++) {
            RogueDependency* dep = &node->dependencies[j];
            
            // Check if target exists
            RogueDependencyNode* target_node = rogue_dependency_manager_find_node(manager, dep->target_file);
            
            if (target_node) {
                dep->status = ROGUE_DEP_STATUS_RESOLVED;
                manager->resolved_dependencies++;
            } else {
                if (dep->type == ROGUE_DEP_TYPE_WEAK) {
                    dep->status = ROGUE_DEP_STATUS_RESOLVED;  // Weak dependencies are OK if missing
                    manager->resolved_dependencies++;
                } else {
                    dep->status = ROGUE_DEP_STATUS_MISSING;
                    manager->failed_resolutions++;
                    manager->graph.missing_count++;
                    all_resolved = false;
                    
                    if (manager->debug_mode) {
                        printf("[DEBUG] Missing dependency: %s -> %s (%s)\n", dep->source_file, dep->target_file, dep->reference_key);
                    }
                }
            }
        }
    }
    
    // Check for circular dependencies
    if (rogue_dependency_manager_detect_cycles(manager)) {
        all_resolved = false;
        manager->circular_dependencies = manager->graph.cycle_count;
        
        if (manager->debug_mode) {
            printf("[DEBUG] Circular dependencies detected: %d cycles\n", manager->graph.cycle_count);
        }
    }
    
    manager->graph.is_valid = all_resolved;
    
    uint64_t end_time = get_current_time_ms();
    manager->last_resolve_time_ms = end_time - start_time;
    manager->total_resolve_time_ms += manager->last_resolve_time_ms;
    manager->resolve_count++;
    
    if (manager->debug_mode) {
        printf("[DEBUG] Dependency resolution complete: %s (%llu ms)\n", 
               all_resolved ? "SUCCESS" : "FAILED", (unsigned long long)manager->last_resolve_time_ms);
    }
    
    return all_resolved;
}

bool rogue_dependency_manager_resolve_file(RogueDependencyManager* manager, const char* file_path) {
    if (!manager || !file_path) return false;
    
    RogueDependencyNode* node = rogue_dependency_manager_find_node(manager, file_path);
    if (!node) return false;
    
    bool all_resolved = true;
    
    // Resolve all dependencies for this file
    for (int i = 0; i < node->dependency_count; i++) {
        RogueDependency* dep = &node->dependencies[i];
        
        RogueDependencyNode* target_node = rogue_dependency_manager_find_node(manager, dep->target_file);
        if (target_node) {
            dep->status = ROGUE_DEP_STATUS_RESOLVED;
        } else {
            if (dep->type == ROGUE_DEP_TYPE_WEAK) {
                dep->status = ROGUE_DEP_STATUS_RESOLVED;
            } else {
                dep->status = ROGUE_DEP_STATUS_MISSING;
                all_resolved = false;
            }
        }
    }
    
    return all_resolved;
}

RogueDependencyStatus rogue_dependency_manager_get_dependency_status(RogueDependencyManager* manager, const char* source_file, const char* target_file, const char* reference_key) {
    if (!manager || !source_file || !target_file || !reference_key) {
        return ROGUE_DEP_STATUS_ERROR;
    }
    
    RogueDependencyNode* source_node = rogue_dependency_manager_find_node(manager, source_file);
    if (!source_node) return ROGUE_DEP_STATUS_ERROR;
    
    for (int i = 0; i < source_node->dependency_count; i++) {
        RogueDependency* dep = &source_node->dependencies[i];
        if (strcmp(dep->target_file, target_file) == 0 &&
            strcmp(dep->reference_key, reference_key) == 0) {
            return dep->status;
        }
    }
    
    return ROGUE_DEP_STATUS_ERROR;
}

// Circular dependency detection using DFS
static bool dfs_detect_cycle(RogueDependencyManager* manager, RogueDependencyNode* node, int* time_counter) {
    node->visited = true;
    node->in_path = true;
    node->discovery_time = (*time_counter)++;
    
    bool cycle_found = false;
    
    // Visit all dependencies (targets)
    for (int i = 0; i < node->dependency_count; i++) {
        RogueDependency* dep = &node->dependencies[i];
        if (dep->type == ROGUE_DEP_TYPE_CIRCULAR_BREAK) continue;  // Skip circular breakers
        
        RogueDependencyNode* target_node = rogue_dependency_manager_find_node(manager, dep->target_file);
        if (!target_node) continue;
        
        if (target_node->in_path) {
            // Back edge found - circular dependency detected
            cycle_found = true;
            dep->status = ROGUE_DEP_STATUS_CIRCULAR;
            
            if (manager->graph.cycle_count < 16) {
                snprintf(manager->graph.cycles[manager->graph.cycle_count], 512, 
                        "Cycle: %s -> %s (via %s)", dep->source_file, dep->target_file, dep->reference_key);
                manager->graph.cycle_count++;
            }
            
            if (manager->debug_mode) {
                printf("[DEBUG] Cycle detected: %s -> %s\n", dep->source_file, dep->target_file);
            }
        } else if (!target_node->visited) {
            if (dfs_detect_cycle(manager, target_node, time_counter)) {
                cycle_found = true;
            }
        }
    }
    
    node->in_path = false;
    node->finish_time = (*time_counter)++;
    
    return cycle_found;
}

bool rogue_dependency_manager_detect_cycles(RogueDependencyManager* manager) {
    if (!manager) return false;
    
    // Reset cycle detection state
    manager->graph.has_cycles = false;
    manager->graph.cycle_count = 0;
    
    // Reset all nodes
    for (int i = 0; i < manager->graph.node_count; i++) {
        manager->graph.nodes[i].visited = false;
        manager->graph.nodes[i].in_path = false;
        manager->graph.nodes[i].discovery_time = -1;
        manager->graph.nodes[i].finish_time = -1;
    }
    
    int time_counter = 0;
    bool cycles_found = false;
    
    // Run DFS from each unvisited node
    for (int i = 0; i < manager->graph.node_count; i++) {
        if (!manager->graph.nodes[i].visited) {
            if (dfs_detect_cycle(manager, &manager->graph.nodes[i], &time_counter)) {
                cycles_found = true;
            }
        }
    }
    
    manager->graph.has_cycles = cycles_found;
    return cycles_found;
}

int rogue_dependency_manager_get_cycles(RogueDependencyManager* manager, char cycles[][512], int max_cycles) {
    if (!manager || !cycles || max_cycles <= 0) return 0;
    
    int copy_count = (manager->graph.cycle_count < max_cycles) ? manager->graph.cycle_count : max_cycles;
    for (int i = 0; i < copy_count; i++) {
        strncpy(cycles[i], manager->graph.cycles[i], 511);
        cycles[i][511] = '\0';
    }
    
    return copy_count;
}

bool rogue_dependency_manager_has_circular_dependency(RogueDependencyManager* manager, const char* file_path) {
    if (!manager || !file_path) return false;
    
    RogueDependencyNode* node = rogue_dependency_manager_find_node(manager, file_path);
    if (!node) return false;
    
    for (int i = 0; i < node->dependency_count; i++) {
        if (node->dependencies[i].status == ROGUE_DEP_STATUS_CIRCULAR) {
            return true;
        }
    }
    
    return false;
}

// Topological sorting for load order generation
static void topological_sort_visit(RogueDependencyManager* manager, RogueDependencyNode* node, 
                                  char result[][ROGUE_DEP_MAX_PATH_LENGTH], int* result_count, bool* visited) {
    int node_index = (int)(node - manager->graph.nodes);
    if (visited[node_index]) return;
    
    visited[node_index] = true;
    
    // Visit all dependencies first
    for (int i = 0; i < node->dependency_count; i++) {
        RogueDependency* dep = &node->dependencies[i];
        if (dep->status == ROGUE_DEP_STATUS_RESOLVED && dep->type != ROGUE_DEP_TYPE_CIRCULAR_BREAK) {
            RogueDependencyNode* target_node = rogue_dependency_manager_find_node(manager, dep->target_file);
            if (target_node) {
                topological_sort_visit(manager, target_node, result, result_count, visited);
            }
        }
    }
    
    // Add this node to result
    strncpy(result[*result_count], node->file_path, ROGUE_DEP_MAX_PATH_LENGTH - 1);
    result[*result_count][ROGUE_DEP_MAX_PATH_LENGTH - 1] = '\0';
    (*result_count)++;
}

bool rogue_dependency_manager_generate_load_order(RogueDependencyManager* manager, RogueLoadOrder* load_order) {
    if (!manager || !load_order) return false;
    
    // Initialize load order
    memset(load_order, 0, sizeof(RogueLoadOrder));
    
    // First resolve all dependencies
    if (!rogue_dependency_manager_resolve_all(manager)) {
        if (manager->strict_mode) {
            load_order->is_valid = false;
            return false;
        }
    }
    
    // Check for cycles
    if (rogue_dependency_manager_detect_cycles(manager) && manager->strict_mode) {
        load_order->is_valid = false;
        return false;
    }
    
    // Create temporary arrays for topological sort
    bool* visited = (bool*)calloc(manager->graph.node_count, sizeof(bool));
    if (!visited) {
        load_order->is_valid = false;
        return false;
    }
    
    // Perform topological sort
    int result_count = 0;
    char temp_result[ROGUE_DEP_MAX_FILES][ROGUE_DEP_MAX_PATH_LENGTH];
    
    for (int i = 0; i < manager->graph.node_count; i++) {
        if (!visited[i] && result_count < ROGUE_DEP_MAX_FILES) {
            topological_sort_visit(manager, &manager->graph.nodes[i], temp_result, &result_count, visited);
        }
    }
    
    free(visited);
    
    // Copy results to load order (dependencies first, then dependents)
    load_order->file_count = result_count;
    for (int i = 0; i < result_count; i++) {
        strncpy(load_order->files[i], temp_result[i], ROGUE_DEP_MAX_PATH_LENGTH - 1);
        load_order->files[i][ROGUE_DEP_MAX_PATH_LENGTH - 1] = '\0';
        
        // Get priority for this file
        RogueDependencyNode* node = rogue_dependency_manager_find_node(manager, temp_result[i]);
        load_order->priorities[i] = node ? node->priority : 0;
    }
    
    load_order->is_valid = true;
    
    if (manager->debug_mode) {
        printf("[DEBUG] Generated load order with %d files\n", result_count);
    }
    
    return true;
}

bool rogue_dependency_manager_get_dependency_aware_order(RogueDependencyManager* manager, const char** file_paths, int file_count, 
                                                        char ordered_files[][ROGUE_DEP_MAX_PATH_LENGTH], int* ordered_count) {
    if (!manager || !file_paths || !ordered_files || !ordered_count || file_count <= 0) {
        return false;
    }
    
    RogueLoadOrder full_order;
    if (!rogue_dependency_manager_generate_load_order(manager, &full_order)) {
        return false;
    }
    
    *ordered_count = 0;
    
    // Filter full order to only include requested files
    for (int i = 0; i < full_order.file_count && *ordered_count < file_count; i++) {
        for (int j = 0; j < file_count; j++) {
            if (strcmp(full_order.files[i], file_paths[j]) == 0) {
                strncpy(ordered_files[*ordered_count], full_order.files[i], ROGUE_DEP_MAX_PATH_LENGTH - 1);
                ordered_files[*ordered_count][ROGUE_DEP_MAX_PATH_LENGTH - 1] = '\0';
                (*ordered_count)++;
                break;
            }
        }
    }
    
    return true;
}

// Impact analysis functions
bool rogue_dependency_manager_analyze_impact(RogueDependencyManager* manager, const char* changed_file, RogueImpactAnalysis* analysis) {
    if (!manager || !changed_file || !analysis) return false;
    
    // Initialize analysis
    memset(analysis, 0, sizeof(RogueImpactAnalysis));
    strncpy(analysis->changed_file, changed_file, ROGUE_DEP_MAX_PATH_LENGTH - 1);
    analysis->changed_file[ROGUE_DEP_MAX_PATH_LENGTH - 1] = '\0';
    
    // Find all files that depend on the changed file
    for (int i = 0; i < manager->graph.node_count; i++) {
        RogueDependencyNode* node = &manager->graph.nodes[i];
        for (int j = 0; j < node->dependency_count; j++) {
            RogueDependency* dep = &node->dependencies[j];
            if (strcmp(dep->target_file, changed_file) == 0) {
                // This file depends on the changed file
                if (analysis->reload_count < ROGUE_DEP_MAX_FILES) {
                    strncpy(analysis->reload_files[analysis->reload_count], node->file_path, ROGUE_DEP_MAX_PATH_LENGTH - 1);
                    analysis->reload_files[analysis->reload_count][ROGUE_DEP_MAX_PATH_LENGTH - 1] = '\0';
                    analysis->reload_count++;
                }
                
                // Determine affected system
                if (analysis->affected_count < ROGUE_DEP_MAX_IMPACT_SYSTEMS) {
                    const char* system_name = rogue_dependency_manager_get_file_type_name(node->file_type);
                    
                    // Check if system already in list
                    bool already_listed = false;
                    for (int k = 0; k < analysis->affected_count; k++) {
                        if (strcmp(analysis->affected_systems[k], system_name) == 0) {
                            already_listed = true;
                            break;
                        }
                    }
                    
                    if (!already_listed) {
                        strncpy(analysis->affected_systems[analysis->affected_count], system_name, ROGUE_DEP_MAX_NAME_LENGTH - 1);
                        analysis->affected_systems[analysis->affected_count][ROGUE_DEP_MAX_NAME_LENGTH - 1] = '\0';
                        analysis->affected_count++;
                    }
                }
            }
        }
    }
    
    // Check if a full reload is required (more than 50% of files affected)
    analysis->requires_full_reload = (analysis->reload_count > manager->graph.node_count / 2);
    
    return true;
}

int rogue_dependency_manager_get_affected_files(RogueDependencyManager* manager, const char* changed_file, 
                                              char affected_files[][ROGUE_DEP_MAX_PATH_LENGTH], int max_files) {
    if (!manager || !changed_file || !affected_files || max_files <= 0) {
        return 0;
    }
    
    RogueImpactAnalysis analysis;
    if (!rogue_dependency_manager_analyze_impact(manager, changed_file, &analysis)) {
        return 0;
    }
    
    int copy_count = (analysis.reload_count < max_files) ? analysis.reload_count : max_files;
    for (int i = 0; i < copy_count; i++) {
        strncpy(affected_files[i], analysis.reload_files[i], ROGUE_DEP_MAX_PATH_LENGTH - 1);
        affected_files[i][ROGUE_DEP_MAX_PATH_LENGTH - 1] = '\0';
    }
    
    return copy_count;
}

int rogue_dependency_manager_get_dependent_systems(RogueDependencyManager* manager, const char* file_path, 
                                                  char systems[][ROGUE_DEP_MAX_NAME_LENGTH], int max_systems) {
    if (!manager || !file_path || !systems || max_systems <= 0) {
        return 0;
    }
    
    RogueImpactAnalysis analysis;
    if (!rogue_dependency_manager_analyze_impact(manager, file_path, &analysis)) {
        return 0;
    }
    
    int copy_count = (analysis.affected_count < max_systems) ? analysis.affected_count : max_systems;
    for (int i = 0; i < copy_count; i++) {
        strncpy(systems[i], analysis.affected_systems[i], ROGUE_DEP_MAX_NAME_LENGTH - 1);
        systems[i][ROGUE_DEP_MAX_NAME_LENGTH - 1] = '\0';
    }
    
    return copy_count;
}

// Validation functions
bool rogue_dependency_manager_validate_graph(RogueDependencyManager* manager) {
    if (!manager) return false;
    
    // Resolve all dependencies first
    if (!rogue_dependency_manager_resolve_all(manager)) {
        return false;
    }
    
    // Check for circular dependencies
    if (rogue_dependency_manager_detect_cycles(manager)) {
        return false;
    }
    
    // Validate that we can generate a load order
    RogueLoadOrder load_order;
    if (!rogue_dependency_manager_generate_load_order(manager, &load_order)) {
        return false;
    }
    
    manager->graph.is_valid = true;
    return true;
}

bool rogue_dependency_manager_validate_file_dependencies(RogueDependencyManager* manager, const char* file_path) {
    if (!manager || !file_path) return false;
    
    return rogue_dependency_manager_resolve_file(manager, file_path);
}

int rogue_dependency_manager_get_unresolved_dependencies(RogueDependencyManager* manager, RogueDependency* unresolved, int max_unresolved) {
    if (!manager || !unresolved || max_unresolved <= 0) {
        return 0;
    }
    
    int count = 0;
    for (int i = 0; i < manager->graph.node_count && count < max_unresolved; i++) {
        RogueDependencyNode* node = &manager->graph.nodes[i];
        for (int j = 0; j < node->dependency_count && count < max_unresolved; j++) {
            if (node->dependencies[j].status == ROGUE_DEP_STATUS_UNRESOLVED) {
                memcpy(&unresolved[count], &node->dependencies[j], sizeof(RogueDependency));
                count++;
            }
        }
    }
    
    return count;
}

int rogue_dependency_manager_get_missing_dependencies(RogueDependencyManager* manager, RogueDependency* missing, int max_missing) {
    if (!manager || !missing || max_missing <= 0) {
        return 0;
    }
    
    int count = 0;
    for (int i = 0; i < manager->graph.node_count && count < max_missing; i++) {
        RogueDependencyNode* node = &manager->graph.nodes[i];
        for (int j = 0; j < node->dependency_count && count < max_missing; j++) {
            if (node->dependencies[j].status == ROGUE_DEP_STATUS_MISSING) {
                memcpy(&missing[count], &node->dependencies[j], sizeof(RogueDependency));
                count++;
            }
        }
    }
    
    return count;
}

// Weak dependency functions
bool rogue_dependency_manager_add_weak_dependency(RogueDependencyManager* manager, const char* source_file, const char* target_file, 
                                                 const char* reference_key, const char* description) {
    return rogue_dependency_manager_add_dependency(manager, source_file, target_file, reference_key, 
                                                  ROGUE_DEP_TYPE_WEAK, 0, description);
}

bool rogue_dependency_manager_is_weak_dependency(RogueDependencyManager* manager, const char* source_file, const char* target_file, const char* reference_key) {
    if (!manager || !source_file || !target_file || !reference_key) {
        return false;
    }
    
    RogueDependencyNode* source_node = rogue_dependency_manager_find_node(manager, source_file);
    if (!source_node) return false;
    
    for (int i = 0; i < source_node->dependency_count; i++) {
        RogueDependency* dep = &source_node->dependencies[i];
        if (strcmp(dep->target_file, target_file) == 0 &&
            strcmp(dep->reference_key, reference_key) == 0) {
            return dep->type == ROGUE_DEP_TYPE_WEAK;
        }
    }
    
    return false;
}

int rogue_dependency_manager_get_weak_dependencies(RogueDependencyManager* manager, const char* file_path, RogueDependency* weak_deps, int max_weak) {
    if (!manager || !file_path || !weak_deps || max_weak <= 0) {
        return 0;
    }
    
    RogueDependencyNode* node = rogue_dependency_manager_find_node(manager, file_path);
    if (!node) return 0;
    
    int count = 0;
    for (int i = 0; i < node->dependency_count && count < max_weak; i++) {
        if (node->dependencies[i].type == ROGUE_DEP_TYPE_WEAK) {
            memcpy(&weak_deps[count], &node->dependencies[i], sizeof(RogueDependency));
            count++;
        }
    }
    
    return count;
}

// Visualization and debugging functions
bool rogue_dependency_manager_export_graphviz(RogueDependencyManager* manager, const char* output_path) {
    if (!manager || !output_path) return false;
    
    FILE* f = fopen(output_path, "w");
    if (!f) return false;
    
    fprintf(f, "digraph DependencyGraph {\n");
    fprintf(f, "  rankdir=LR;\n");
    fprintf(f, "  node [shape=box];\n\n");
    
    // Add nodes
    for (int i = 0; i < manager->graph.node_count; i++) {
        RogueDependencyNode* node = &manager->graph.nodes[i];
        const char* filename = strrchr(node->file_path, '/');
        if (!filename) filename = strrchr(node->file_path, '\\');
        if (!filename) filename = node->file_path;
        else filename++;
        
        fprintf(f, "  \"%s\" [label=\"%s\\n(%s)\" style=%s];\n", 
                node->file_path, filename, rogue_dependency_manager_get_file_type_name(node->file_type),
                node->is_loaded ? "filled" : "solid");
    }
    
    fprintf(f, "\n");
    
    // Add edges
    for (int i = 0; i < manager->graph.node_count; i++) {
        RogueDependencyNode* node = &manager->graph.nodes[i];
        for (int j = 0; j < node->dependency_count; j++) {
            RogueDependency* dep = &node->dependencies[j];
            const char* color = "black";
            const char* style = "solid";
            
            switch (dep->status) {
                case ROGUE_DEP_STATUS_RESOLVED: color = "green"; break;
                case ROGUE_DEP_STATUS_MISSING: color = "red"; break;
                case ROGUE_DEP_STATUS_CIRCULAR: color = "orange"; style = "dashed"; break;
                case ROGUE_DEP_STATUS_UNRESOLVED: color = "gray"; break;
                default: break;
            }
            
            if (dep->type == ROGUE_DEP_TYPE_WEAK) {
                style = "dotted";
            }
            
            fprintf(f, "  \"%s\" -> \"%s\" [label=\"%s\" color=%s style=%s];\n",
                    dep->source_file, dep->target_file, dep->reference_key, color, style);
        }
    }
    
    fprintf(f, "}\n");
    fclose(f);
    
    return true;
}

void rogue_dependency_manager_print_graph(RogueDependencyManager* manager) {
    if (!manager) return;
    
    printf("=== Dependency Graph ===\n");
    printf("Files: %d, Dependencies: %d, Valid: %s\n", 
           manager->graph.node_count, manager->total_dependencies,
           manager->graph.is_valid ? "YES" : "NO");
    
    if (manager->graph.has_cycles) {
        printf("Cycles detected: %d\n", manager->graph.cycle_count);
        for (int i = 0; i < manager->graph.cycle_count; i++) {
            printf("  - %s\n", manager->graph.cycles[i]);
        }
    }
    
    printf("\nFiles:\n");
    for (int i = 0; i < manager->graph.node_count; i++) {
        RogueDependencyNode* node = &manager->graph.nodes[i];
        printf("  %s (%s, priority %d, deps: %d)\n",
               node->file_path, rogue_dependency_manager_get_file_type_name(node->file_type),
               node->priority, node->dependency_count);
        
        for (int j = 0; j < node->dependency_count; j++) {
            RogueDependency* dep = &node->dependencies[j];
            printf("    -> %s:%s (%s, %s)\n",
                   dep->target_file, dep->reference_key,
                   rogue_dependency_manager_get_dependency_type_name(dep->type),
                   rogue_dependency_manager_get_dependency_status_name(dep->status));
        }
    }
}

void rogue_dependency_manager_print_load_order(RogueDependencyManager* manager, const RogueLoadOrder* load_order) {
    if (!manager || !load_order) return;
    
    printf("=== Load Order ===\n");
    printf("Valid: %s, Files: %d\n", load_order->is_valid ? "YES" : "NO", load_order->file_count);
    
    for (int i = 0; i < load_order->file_count; i++) {
        printf("  %d. %s (priority: %d)\n", i + 1, load_order->files[i], load_order->priorities[i]);
    }
}

void rogue_dependency_manager_print_impact_analysis(RogueDependencyManager* manager, const RogueImpactAnalysis* analysis) {
    if (!manager || !analysis) return;
    
    printf("=== Impact Analysis ===\n");
    printf("Changed file: %s\n", analysis->changed_file);
    printf("Affected systems: %d, Files to reload: %d\n", analysis->affected_count, analysis->reload_count);
    printf("Full reload required: %s\n", analysis->requires_full_reload ? "YES" : "NO");
    
    if (analysis->affected_count > 0) {
        printf("Affected systems:\n");
        for (int i = 0; i < analysis->affected_count; i++) {
            printf("  - %s\n", analysis->affected_systems[i]);
        }
    }
    
    if (analysis->reload_count > 0) {
        printf("Files to reload:\n");
        for (int i = 0; i < analysis->reload_count; i++) {
            printf("  - %s\n", analysis->reload_files[i]);
        }
    }
}

// Statistics and performance functions
void rogue_dependency_manager_get_statistics(RogueDependencyManager* manager, int* total_deps, int* resolved_deps, int* failed_deps, int* circular_deps) {
    if (!manager) return;
    
    if (total_deps) *total_deps = manager->total_dependencies;
    if (resolved_deps) *resolved_deps = manager->resolved_dependencies;
    if (failed_deps) *failed_deps = manager->failed_resolutions;
    if (circular_deps) *circular_deps = manager->circular_dependencies;
}

uint64_t rogue_dependency_manager_get_average_resolve_time(RogueDependencyManager* manager) {
    if (!manager || manager->resolve_count == 0) return 0;
    return manager->total_resolve_time_ms / manager->resolve_count;
}

void rogue_dependency_manager_reset_statistics(RogueDependencyManager* manager) {
    if (!manager) return;
    
    manager->resolved_dependencies = 0;
    manager->failed_resolutions = 0;
    manager->circular_dependencies = 0;
    manager->last_resolve_time_ms = 0;
    manager->total_resolve_time_ms = 0;
    manager->resolve_count = 0;
}

// Utility functions
RogueFileType rogue_dependency_manager_get_file_type_from_path(const char* file_path) {
    if (!file_path) return ROGUE_FILE_TYPE_OTHER;
    
    const char* filename = strrchr(file_path, '/');
    if (!filename) filename = strrchr(file_path, '\\');
    if (!filename) filename = file_path;
    else filename++;
    
    if (strstr(filename, "items")) return ROGUE_FILE_TYPE_ITEMS;
    if (strstr(filename, "affixes")) return ROGUE_FILE_TYPE_AFFIXES;
    if (strstr(filename, "enemies")) return ROGUE_FILE_TYPE_ENEMIES;
    if (strstr(filename, "encounters")) return ROGUE_FILE_TYPE_ENCOUNTERS;
    if (strstr(filename, "loot_tables")) return ROGUE_FILE_TYPE_LOOT_TABLES;
    if (strstr(filename, "biomes")) return ROGUE_FILE_TYPE_BIOMES;
    if (strstr(filename, "skills")) return ROGUE_FILE_TYPE_SKILLS;
    
    return ROGUE_FILE_TYPE_OTHER;
}

const char* rogue_dependency_manager_get_file_type_name(RogueFileType file_type) {
    switch (file_type) {
        case ROGUE_FILE_TYPE_ITEMS: return "Items";
        case ROGUE_FILE_TYPE_AFFIXES: return "Affixes";
        case ROGUE_FILE_TYPE_ENEMIES: return "Enemies";
        case ROGUE_FILE_TYPE_ENCOUNTERS: return "Encounters";
        case ROGUE_FILE_TYPE_LOOT_TABLES: return "LootTables";
        case ROGUE_FILE_TYPE_BIOMES: return "Biomes";
        case ROGUE_FILE_TYPE_SKILLS: return "Skills";
        case ROGUE_FILE_TYPE_OTHER: return "Other";
        default: return "Unknown";
    }
}

const char* rogue_dependency_manager_get_dependency_type_name(RogueDependencyType dep_type) {
    switch (dep_type) {
        case ROGUE_DEP_TYPE_STRONG: return "Strong";
        case ROGUE_DEP_TYPE_WEAK: return "Weak";
        case ROGUE_DEP_TYPE_CIRCULAR_BREAK: return "CircularBreaker";
        default: return "Unknown";
    }
}

const char* rogue_dependency_manager_get_dependency_status_name(RogueDependencyStatus status) {
    switch (status) {
        case ROGUE_DEP_STATUS_UNRESOLVED: return "Unresolved";
        case ROGUE_DEP_STATUS_RESOLVED: return "Resolved";
        case ROGUE_DEP_STATUS_MISSING: return "Missing";
        case ROGUE_DEP_STATUS_CIRCULAR: return "Circular";
        case ROGUE_DEP_STATUS_ERROR: return "Error";
        default: return "Unknown";
    }
}

bool rogue_dependency_manager_is_valid_file_path(const char* file_path) {
    if (!file_path || strlen(file_path) == 0 || strlen(file_path) >= ROGUE_DEP_MAX_PATH_LENGTH) {
        return false;
    }
    
    // Check for invalid characters (basic validation)
    const char* invalid_chars = "<>:\"|?*";
    for (const char* c = invalid_chars; *c; c++) {
        if (strchr(file_path, *c)) {
            return false;
        }
    }
    
    return true;
}

bool rogue_dependency_manager_is_valid_reference_key(const char* reference_key) {
    if (!reference_key || strlen(reference_key) == 0 || strlen(reference_key) >= ROGUE_DEP_MAX_NAME_LENGTH) {
        return false;
    }
    
    // Reference keys should be alphanumeric with underscores and dashes
    for (const char* c = reference_key; *c; c++) {
        if (!isalnum(*c) && *c != '_' && *c != '-' && *c != '.') {
            return false;
        }
    }
    
    return true;
}

#ifdef _WIN32
#pragma warning(pop)
#endif
