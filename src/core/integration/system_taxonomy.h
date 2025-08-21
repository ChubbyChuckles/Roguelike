/*
MIT License

Copyright (c) 2025 ChubbyChuckles

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef ROGUE_SYSTEM_TAXONOMY_H
#define ROGUE_SYSTEM_TAXONOMY_H

#include "core/integration/integration_manager.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum number of known systems in taxonomy */
#define ROGUE_TAXONOMY_MAX_SYSTEMS 64

/* System identification structure (Phase 0.1.1) */
typedef struct {
    uint32_t system_id;
    const char* name;
    const char* description;
    RogueSystemType type;
    RogueSystemPriority priority;
    uint32_t capabilities;
    bool is_implemented;
    const char* implementation_status;
    const char* version;
} RogueSystemInfo;

/* System taxonomy database */
typedef struct {
    RogueSystemInfo systems[ROGUE_TAXONOMY_MAX_SYSTEMS];
    uint32_t system_count;
    bool initialized;
} RogueSystemTaxonomy;

/* Global taxonomy instance */
extern RogueSystemTaxonomy g_system_taxonomy;

/* Core API */
bool rogue_system_taxonomy_init(void);
void rogue_system_taxonomy_shutdown(void);

/* System enumeration (Phase 0.1.1) */
bool rogue_system_taxonomy_add_system(const RogueSystemInfo* system_info);
const RogueSystemInfo* rogue_system_taxonomy_get_system(uint32_t system_id);
const RogueSystemInfo* rogue_system_taxonomy_find_system_by_name(const char* name);
uint32_t rogue_system_taxonomy_get_system_count(void);

/* System classification utilities (Phase 0.1.2-0.1.3) */
uint32_t rogue_system_taxonomy_count_by_type(RogueSystemType type);
uint32_t rogue_system_taxonomy_count_by_priority(RogueSystemPriority priority);
uint32_t rogue_system_taxonomy_count_implemented(void);
uint32_t rogue_system_taxonomy_count_by_capability(RogueSystemCapability capability);

/* Capability matrix generation (Phase 0.1.4) */
void rogue_system_taxonomy_generate_capability_matrix(char* buffer, size_t buffer_size);

/* Resource usage analysis (Phase 0.1.6) */
void rogue_system_taxonomy_analyze_resource_usage(char* buffer, size_t buffer_size);

/* Initialization sequence analysis (Phase 0.1.5) */
void rogue_system_taxonomy_generate_init_report(char* buffer, size_t buffer_size);

/* Populate taxonomy with known systems */
void rogue_system_taxonomy_populate_known_systems(void);

/* Validation and integrity checks */
bool rogue_system_taxonomy_validate(void);
void rogue_system_taxonomy_generate_report(char* buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_SYSTEM_TAXONOMY_H */
