#ifndef ROGUE_DEPENDENCY_MANAGER_H
#define ROGUE_DEPENDENCY_MANAGER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Maximum limits for dependency system
#define ROGUE_DEP_MAX_FILES 256
#define ROGUE_DEP_MAX_DEPENDENCIES 64
#define ROGUE_DEP_MAX_PATH_LENGTH 512
#define ROGUE_DEP_MAX_NAME_LENGTH 128
#define ROGUE_DEP_MAX_IMPACT_SYSTEMS 32

// Forward declarations
typedef struct RogueDependencyManager RogueDependencyManager;
typedef struct RogueDependencyNode RogueDependencyNode;
typedef struct RogueDependency RogueDependency;
typedef struct RogueDependencyGraph RogueDependencyGraph;
typedef struct RogueLoadOrder RogueLoadOrder;
typedef struct RogueImpactAnalysis RogueImpactAnalysis;

// Dependency types
typedef enum {
    ROGUE_DEP_TYPE_STRONG,          // Required dependency (must exist)
    ROGUE_DEP_TYPE_WEAK,            // Optional dependency (may exist)
    ROGUE_DEP_TYPE_CIRCULAR_BREAK   // Circular dependency breaker
} RogueDependencyType;

// Dependency status
typedef enum {
    ROGUE_DEP_STATUS_UNRESOLVED,    // Not yet checked
    ROGUE_DEP_STATUS_RESOLVED,      // Successfully resolved
    ROGUE_DEP_STATUS_MISSING,       // Required dependency missing
    ROGUE_DEP_STATUS_CIRCULAR,      // Part of circular dependency
    ROGUE_DEP_STATUS_ERROR          // Error in dependency
} RogueDependencyStatus;

// File types for dependency management
typedef enum {
    ROGUE_FILE_TYPE_ITEMS,
    ROGUE_FILE_TYPE_AFFIXES, 
    ROGUE_FILE_TYPE_ENEMIES,
    ROGUE_FILE_TYPE_ENCOUNTERS,
    ROGUE_FILE_TYPE_LOOT_TABLES,
    ROGUE_FILE_TYPE_BIOMES,
    ROGUE_FILE_TYPE_SKILLS,
    ROGUE_FILE_TYPE_OTHER,
    ROGUE_FILE_TYPE_COUNT
} RogueFileType;

// Individual dependency relationship
typedef struct RogueDependency {
    char source_file[ROGUE_DEP_MAX_PATH_LENGTH];        // File that has the dependency
    char target_file[ROGUE_DEP_MAX_PATH_LENGTH];        // File being depended upon
    char reference_key[ROGUE_DEP_MAX_NAME_LENGTH];      // Key/ID being referenced
    RogueDependencyType type;                           // Strong, weak, or circular breaker
    RogueDependencyStatus status;                       // Current resolution status
    int priority;                                       // Load priority (lower = first)
    char description[ROGUE_DEP_MAX_NAME_LENGTH];        // Human-readable description
} RogueDependency;

// Node in dependency graph
typedef struct RogueDependencyNode {
    char file_path[ROGUE_DEP_MAX_PATH_LENGTH];          // Configuration file path
    RogueFileType file_type;                            // Type of configuration file
    int priority;                                       // Load priority
    bool is_loaded;                                     // Current load status
    uint64_t last_modified;                             // Last modification time
    char checksum[64];                                  // File content checksum
    
    // Dependencies
    int dependency_count;                               // Number of dependencies
    RogueDependency dependencies[ROGUE_DEP_MAX_DEPENDENCIES]; // Dependency array
    
    // Graph traversal
    bool visited;                                       // For graph algorithms
    bool in_path;                                       // For circular detection
    int discovery_time;                                 // DFS discovery time
    int finish_time;                                    // DFS finish time
} RogueDependencyNode;

// Dependency graph structure
typedef struct RogueDependencyGraph {
    int node_count;                                     // Number of nodes
    RogueDependencyNode nodes[ROGUE_DEP_MAX_FILES];     // All configuration files
    
    // Graph metadata
    bool has_cycles;                                    // Circular dependency detected
    int cycle_count;                                    // Number of cycles found
    char cycles[16][512];                               // Cycle descriptions
    
    // Validation state
    bool is_valid;                                      // Graph is valid and resolved
    int unresolved_count;                               // Number of unresolved dependencies
    int missing_count;                                  // Number of missing dependencies
} RogueDependencyGraph;

// Load order result
typedef struct RogueLoadOrder {
    int file_count;                                     // Number of files to load
    char files[ROGUE_DEP_MAX_FILES][ROGUE_DEP_MAX_PATH_LENGTH]; // Files in load order
    int priorities[ROGUE_DEP_MAX_FILES];                // Priority of each file
    bool is_valid;                                      // Load order is valid
} RogueLoadOrder;

// Impact analysis result
typedef struct RogueImpactAnalysis {
    char changed_file[ROGUE_DEP_MAX_PATH_LENGTH];       // File that changed
    int affected_count;                                 // Number of affected systems
    char affected_systems[ROGUE_DEP_MAX_IMPACT_SYSTEMS][ROGUE_DEP_MAX_NAME_LENGTH]; // Affected system names
    int reload_count;                                   // Number of files to reload
    char reload_files[ROGUE_DEP_MAX_FILES][ROGUE_DEP_MAX_PATH_LENGTH]; // Files to reload
    bool requires_full_reload;                          // Full system reload needed
} RogueImpactAnalysis;

// Dependency manager main structure
typedef struct RogueDependencyManager {
    RogueDependencyGraph graph;                         // Dependency graph
    
    // Configuration
    bool auto_resolve;                                  // Automatically resolve dependencies
    bool strict_mode;                                   // Fail on any missing dependencies
    bool debug_mode;                                    // Enable debug logging
    
    // Statistics
    int total_dependencies;                             // Total dependency count
    int resolved_dependencies;                          // Successfully resolved
    int failed_resolutions;                             // Failed resolutions
    int circular_dependencies;                          // Circular dependencies found
    
    // Performance tracking
    uint64_t last_resolve_time_ms;                      // Time for last resolution
    uint64_t total_resolve_time_ms;                     // Total resolution time
    int resolve_count;                                  // Number of resolutions performed
} RogueDependencyManager;

// Core management functions
RogueDependencyManager* rogue_dependency_manager_create(void);
void rogue_dependency_manager_destroy(RogueDependencyManager* manager);
bool rogue_dependency_manager_initialize(RogueDependencyManager* manager);
void rogue_dependency_manager_cleanup(RogueDependencyManager* manager);
void rogue_dependency_manager_reset(RogueDependencyManager* manager);

// Configuration functions
void rogue_dependency_manager_set_auto_resolve(RogueDependencyManager* manager, bool auto_resolve);
void rogue_dependency_manager_set_strict_mode(RogueDependencyManager* manager, bool strict_mode);
void rogue_dependency_manager_set_debug_mode(RogueDependencyManager* manager, bool debug_mode);

// Node management functions
bool rogue_dependency_manager_add_file(RogueDependencyManager* manager, const char* file_path, RogueFileType file_type, int priority);
bool rogue_dependency_manager_remove_file(RogueDependencyManager* manager, const char* file_path);
RogueDependencyNode* rogue_dependency_manager_find_node(RogueDependencyManager* manager, const char* file_path);
bool rogue_dependency_manager_update_file_info(RogueDependencyManager* manager, const char* file_path, uint64_t last_modified, const char* checksum);

// Dependency registration functions
bool rogue_dependency_manager_add_dependency(RogueDependencyManager* manager, const char* source_file, const char* target_file, 
                                           const char* reference_key, RogueDependencyType type, int priority, const char* description);
bool rogue_dependency_manager_remove_dependency(RogueDependencyManager* manager, const char* source_file, const char* target_file, const char* reference_key);
int rogue_dependency_manager_get_dependencies(RogueDependencyManager* manager, const char* source_file, RogueDependency* dependencies, int max_dependencies);

// Dependency resolution functions
bool rogue_dependency_manager_resolve_all(RogueDependencyManager* manager);
bool rogue_dependency_manager_resolve_file(RogueDependencyManager* manager, const char* file_path);
RogueDependencyStatus rogue_dependency_manager_get_dependency_status(RogueDependencyManager* manager, const char* source_file, const char* target_file, const char* reference_key);

// Circular dependency detection
bool rogue_dependency_manager_detect_cycles(RogueDependencyManager* manager);
int rogue_dependency_manager_get_cycles(RogueDependencyManager* manager, char cycles[][512], int max_cycles);
bool rogue_dependency_manager_has_circular_dependency(RogueDependencyManager* manager, const char* file_path);

// Load order generation
bool rogue_dependency_manager_generate_load_order(RogueDependencyManager* manager, RogueLoadOrder* load_order);
bool rogue_dependency_manager_get_dependency_aware_order(RogueDependencyManager* manager, const char** file_paths, int file_count, char ordered_files[][ROGUE_DEP_MAX_PATH_LENGTH], int* ordered_count);

// Impact analysis
bool rogue_dependency_manager_analyze_impact(RogueDependencyManager* manager, const char* changed_file, RogueImpactAnalysis* analysis);
int rogue_dependency_manager_get_affected_files(RogueDependencyManager* manager, const char* changed_file, char affected_files[][ROGUE_DEP_MAX_PATH_LENGTH], int max_files);
int rogue_dependency_manager_get_dependent_systems(RogueDependencyManager* manager, const char* file_path, char systems[][ROGUE_DEP_MAX_NAME_LENGTH], int max_systems);

// Validation functions
bool rogue_dependency_manager_validate_graph(RogueDependencyManager* manager);
bool rogue_dependency_manager_validate_file_dependencies(RogueDependencyManager* manager, const char* file_path);
int rogue_dependency_manager_get_unresolved_dependencies(RogueDependencyManager* manager, RogueDependency* unresolved, int max_unresolved);
int rogue_dependency_manager_get_missing_dependencies(RogueDependencyManager* manager, RogueDependency* missing, int max_missing);

// Weak dependency functions
bool rogue_dependency_manager_add_weak_dependency(RogueDependencyManager* manager, const char* source_file, const char* target_file, const char* reference_key, const char* description);
bool rogue_dependency_manager_is_weak_dependency(RogueDependencyManager* manager, const char* source_file, const char* target_file, const char* reference_key);
int rogue_dependency_manager_get_weak_dependencies(RogueDependencyManager* manager, const char* file_path, RogueDependency* weak_deps, int max_weak);

// Visualization and debugging
bool rogue_dependency_manager_export_graphviz(RogueDependencyManager* manager, const char* output_path);
void rogue_dependency_manager_print_graph(RogueDependencyManager* manager);
void rogue_dependency_manager_print_load_order(RogueDependencyManager* manager, const RogueLoadOrder* load_order);
void rogue_dependency_manager_print_impact_analysis(RogueDependencyManager* manager, const RogueImpactAnalysis* analysis);

// Statistics and performance
void rogue_dependency_manager_get_statistics(RogueDependencyManager* manager, int* total_deps, int* resolved_deps, int* failed_deps, int* circular_deps);
uint64_t rogue_dependency_manager_get_average_resolve_time(RogueDependencyManager* manager);
void rogue_dependency_manager_reset_statistics(RogueDependencyManager* manager);

// Utility functions
RogueFileType rogue_dependency_manager_get_file_type_from_path(const char* file_path);
const char* rogue_dependency_manager_get_file_type_name(RogueFileType file_type);
const char* rogue_dependency_manager_get_dependency_type_name(RogueDependencyType dep_type);
const char* rogue_dependency_manager_get_dependency_status_name(RogueDependencyStatus status);
bool rogue_dependency_manager_is_valid_file_path(const char* file_path);
bool rogue_dependency_manager_is_valid_reference_key(const char* reference_key);

#ifdef __cplusplus
}
#endif

#endif // ROGUE_DEPENDENCY_MANAGER_H
