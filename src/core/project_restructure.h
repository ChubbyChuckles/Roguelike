#ifndef ROGUE_PROJECT_RESTRUCTURE_H
#define ROGUE_PROJECT_RESTRUCTURE_H

/**
 * Project Restructuring Plan using Dependency Management
 * 
 * This header defines the strategy for reorganizing the roguelike project
 * into logical submodules while maintaining all dependencies and build integrity.
 * 
 * Target Structure:
 * src/core/
 *   ├── integration/      (integration infrastructure)
 *   ├── equipment/        (equipment system files)
 *   ├── loot/            (loot generation system)
 *   ├── vendor/          (vendor and economy system)
 *   ├── crafting/        (crafting and materials)
 *   ├── progression/     (player progression system)
 *   ├── vegetation/      (vegetation system)
 *   ├── enemy/           (enemy AI and difficulty)
 *   └── foundation/      (core app and game loop)
 */

#include "integration/dependency_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Structure to define file group reorganization
typedef struct {
    char name[64];
    char target_dir[128];
    char pattern[128]; // Increased size for longer patterns
    int file_count;
    char files[32][128]; // Support up to 32 files per category
} RogueFileGroup;

// Function prototypes for restructuring operations
int restructure_analyze_dependencies(RogueDependencyManager* manager);
int restructure_create_directories(void);
int restructure_move_files_safely(RogueDependencyManager* manager);
int restructure_update_cmake_files(void);
int restructure_update_test_files(void);
int restructure_validate_build(void);

// Helper functions
int populate_file_groups(RogueFileGroup groups[], int max_groups);
int move_file_group_with_deps(RogueDependencyManager* manager, const RogueFileGroup* group);
int update_include_paths_in_file(const char* file_path, const RogueFileGroup groups[], int group_count);

#endif // ROGUE_PROJECT_RESTRUCTURE_H
