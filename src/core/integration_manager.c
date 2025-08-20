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

#include "core/integration_manager.h"
#include "util/log.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <assert.h>

/* Global integration manager instance */
RogueIntegrationManager g_integration_manager = {0};

/* System type names for debugging (Phase 0.1) */
static const char* SYSTEM_TYPE_NAMES[ROGUE_SYSTEM_TYPE_COUNT] = {
    "Core",
    "Content", 
    "UI",
    "Infrastructure"
};

/* System priority names for debugging (Phase 0.1) */
static const char* SYSTEM_PRIORITY_NAMES[ROGUE_SYSTEM_PRIORITY_COUNT] = {
    "Critical",
    "Important",
    "Optional"
};

/* System state names for debugging (Phase 0.3.3) */
static const char* SYSTEM_STATE_NAMES[ROGUE_SYSTEM_STATE_COUNT] = {
    "Uninitialized",
    "Initializing", 
    "Running",
    "Paused",
    "Shutdown",
    "Failed"
};

/* Internal helper to get current time in milliseconds */
static double get_current_time_ms(void) {
    return (double)clock() / (double)CLOCKS_PER_SEC * 1000.0;
}

/* Validate system descriptor (Phase 0.5.7) */
static bool validate_system_descriptor(const RogueSystemDescriptor* descriptor) {
    if (!descriptor) {
        ROGUE_LOG_ERROR("System descriptor is NULL");
        return false;
    }
    
    if (!descriptor->name || strlen(descriptor->name) == 0) {
        ROGUE_LOG_ERROR("System name is required");
        return false;
    }
    
    if (descriptor->type >= ROGUE_SYSTEM_TYPE_COUNT) {
        ROGUE_LOG_ERROR("Invalid system type: %d", descriptor->type);
        return false;
    }
    
    if (descriptor->priority >= ROGUE_SYSTEM_PRIORITY_COUNT) {
        ROGUE_LOG_ERROR("Invalid system priority: %d", descriptor->priority);
        return false;
    }
    
    if (!descriptor->interface.init || !descriptor->interface.update || !descriptor->interface.shutdown) {
        ROGUE_LOG_ERROR("System '%s' missing mandatory interface methods", descriptor->name);
        return false;
    }
    
    return true;
}

/* Check for circular dependencies (Phase 0.2.3) */
static bool has_circular_dependency(uint32_t system_id, uint32_t target_id, uint32_t* visited, size_t visited_count) {
    /* Check if we've already visited this system - this indicates a cycle */
    for (size_t i = 0; i < visited_count; i++) {
        if (visited[i] == system_id) {
            return system_id == target_id; /* Cycle detected if we've returned to target */
        }
    }
    
    /* Find the system entry */
    RogueSystemEntry* entry = rogue_integration_get_system(system_id);
    if (!entry) {
        return false;
    }
    
    /* Add current system to visited list */
    visited[visited_count] = system_id;
    
    /* Check all hard dependencies recursively */
    for (uint8_t i = 0; i < entry->descriptor.hard_dep_count; i++) {
        uint32_t dep_id = entry->descriptor.hard_dependencies[i];
        /* Direct cycle check */
        if (dep_id == target_id && visited_count > 0) {
            return true; /* Found cycle back to target */
        }
        /* Recursive cycle check */
        if (has_circular_dependency(dep_id, target_id, visited, visited_count + 1)) {
            return true;
        }
    }
    
    return false;
}

/* Core API Implementation */

bool rogue_integration_manager_init(void) {
    memset(&g_integration_manager, 0, sizeof(RogueIntegrationManager));
    g_integration_manager.next_system_id = 1; /* Start from 1, 0 is invalid */
    g_integration_manager.manager_uptime_ms = get_current_time_ms();
    g_integration_manager.initialization_complete = false;
    
    ROGUE_LOG_INFO("Integration manager initialized");
    return true;
}

void rogue_integration_manager_shutdown(void) {
    /* Shutdown all systems in reverse order */
    for (int i = (int)g_integration_manager.system_count - 1; i >= 0; i--) {
        RogueSystemEntry* entry = &g_integration_manager.systems[i];
        if (entry->current_state == ROGUE_SYSTEM_STATE_RUNNING || 
            entry->current_state == ROGUE_SYSTEM_STATE_PAUSED) {
            rogue_integration_shutdown_system(entry->descriptor.system_id);
        }
    }
    
    memset(&g_integration_manager, 0, sizeof(RogueIntegrationManager));
    ROGUE_LOG_INFO("Integration manager shutdown complete");
}

void rogue_integration_manager_update(double dt_ms) {
    double frame_start_time = get_current_time_ms();
    
    /* Update all running systems (Phase 0.5.1) */
    for (uint32_t i = 0; i < g_integration_manager.system_count; i++) {
        RogueSystemEntry* entry = &g_integration_manager.systems[i];
        
        if (entry->current_state == ROGUE_SYSTEM_STATE_RUNNING) {
            double system_start_time = get_current_time_ms();
            
            /* Call system update */
            entry->descriptor.interface.update(entry->descriptor.system_data, dt_ms);
            
            /* Update performance metrics */
            double system_update_time = get_current_time_ms() - system_start_time;
            if (system_update_time > g_integration_manager.max_update_time_ms) {
                g_integration_manager.max_update_time_ms = system_update_time;
            }
            
            /* Update health indicators (Phase 0.3.4) */
            entry->health.last_update_time_ms = get_current_time_ms();
            entry->health.uptime_seconds = (uint32_t)((get_current_time_ms() - entry->last_restart_time_ms) / 1000.0);
            entry->health.is_responsive = true;
        }
    }
    
    /* Update manager performance metrics */
    double total_update_time = get_current_time_ms() - frame_start_time;
    g_integration_manager.total_update_time_ms += total_update_time;
    g_integration_manager.update_call_count++;
}

/* System Registration (Phase 0.3.2) */

uint32_t rogue_integration_register_system(const RogueSystemDescriptor* descriptor) {
    if (!validate_system_descriptor(descriptor)) {
        return 0; /* Invalid system ID */
    }
    
    if (g_integration_manager.system_count >= ROGUE_MAX_SYSTEMS) {
        ROGUE_LOG_ERROR("Maximum number of systems (%d) exceeded", ROGUE_MAX_SYSTEMS);
        return 0;
    }
    
    /* Check for duplicate names */
    if (rogue_integration_find_system_by_name(descriptor->name) != NULL) {
        ROGUE_LOG_ERROR("System with name '%s' already registered", descriptor->name);
        return 0;
    }
    
    uint32_t system_id = g_integration_manager.next_system_id++;
    RogueSystemEntry* entry = &g_integration_manager.systems[g_integration_manager.system_count];
    
    /* Copy descriptor */
    entry->descriptor = *descriptor;
    entry->descriptor.system_id = system_id;
    
    /* Initialize state */
    entry->current_state = ROGUE_SYSTEM_STATE_UNINITIALIZED;
    entry->last_restart_time_ms = get_current_time_ms();
    entry->restart_backoff_ms = 1000; /* Start with 1 second backoff */
    
    /* Initialize health */
    memset(&entry->health, 0, sizeof(RogueSystemHealth));
    entry->health.is_responsive = true;
    
    g_integration_manager.system_count++;
    
    ROGUE_LOG_INFO("Registered system '%s' (ID: %u, Type: %s, Priority: %s)", 
                   descriptor->name, system_id,
                   SYSTEM_TYPE_NAMES[descriptor->type],
                   SYSTEM_PRIORITY_NAMES[descriptor->priority]);
    
    return system_id;
}

bool rogue_integration_unregister_system(uint32_t system_id) {
    for (uint32_t i = 0; i < g_integration_manager.system_count; i++) {
        if (g_integration_manager.systems[i].descriptor.system_id == system_id) {
            RogueSystemEntry* entry = &g_integration_manager.systems[i];
            
            /* Shutdown the system first */
            if (entry->current_state == ROGUE_SYSTEM_STATE_RUNNING || 
                entry->current_state == ROGUE_SYSTEM_STATE_PAUSED) {
                rogue_integration_shutdown_system(system_id);
            }
            
            ROGUE_LOG_INFO("Unregistering system '%s' (ID: %u)", entry->descriptor.name, system_id);
            
            /* Move the last system to this position */
            if (i < g_integration_manager.system_count - 1) {
                g_integration_manager.systems[i] = g_integration_manager.systems[g_integration_manager.system_count - 1];
            }
            
            g_integration_manager.system_count--;
            return true;
        }
    }
    
    ROGUE_LOG_WARN("Attempted to unregister non-existent system ID: %u", system_id);
    return false;
}

RogueSystemEntry* rogue_integration_get_system(uint32_t system_id) {
    for (uint32_t i = 0; i < g_integration_manager.system_count; i++) {
        if (g_integration_manager.systems[i].descriptor.system_id == system_id) {
            return &g_integration_manager.systems[i];
        }
    }
    return NULL;
}

RogueSystemEntry* rogue_integration_find_system_by_name(const char* name) {
    if (!name) return NULL;
    
    for (uint32_t i = 0; i < g_integration_manager.system_count; i++) {
        if (strcmp(g_integration_manager.systems[i].descriptor.name, name) == 0) {
            return &g_integration_manager.systems[i];
        }
    }
    return NULL;
}

/* System Lifecycle Control (Phase 0.3.3) */

bool rogue_integration_initialize_system(uint32_t system_id) {
    RogueSystemEntry* entry = rogue_integration_get_system(system_id);
    if (!entry) {
        ROGUE_LOG_ERROR("Cannot initialize non-existent system ID: %u", system_id);
        return false;
    }
    
    if (entry->current_state != ROGUE_SYSTEM_STATE_UNINITIALIZED &&
        entry->current_state != ROGUE_SYSTEM_STATE_FAILED) {
        ROGUE_LOG_WARN("System '%s' is not in uninitialized or failed state (current: %s)", 
                       entry->descriptor.name, SYSTEM_STATE_NAMES[entry->current_state]);
        return false;
    }
    
    entry->current_state = ROGUE_SYSTEM_STATE_INITIALIZING;
    
    ROGUE_LOG_INFO("Initializing system '%s'...", entry->descriptor.name);
    
    bool result = entry->descriptor.interface.init(entry->descriptor.system_data);
    
    if (result) {
        entry->current_state = ROGUE_SYSTEM_STATE_RUNNING;
        entry->last_restart_time_ms = get_current_time_ms();
        entry->health.restart_count++;
        entry->restart_backoff_ms = 1000; /* Reset backoff on success */
        ROGUE_LOG_INFO("System '%s' initialized successfully", entry->descriptor.name);
    } else {
        entry->current_state = ROGUE_SYSTEM_STATE_FAILED;
        entry->health.error_count++;
        ROGUE_LOG_ERROR("System '%s' initialization failed", entry->descriptor.name);
    }
    
    return result;
}

bool rogue_integration_shutdown_system(uint32_t system_id) {
    RogueSystemEntry* entry = rogue_integration_get_system(system_id);
    if (!entry) {
        ROGUE_LOG_ERROR("Cannot shutdown non-existent system ID: %u", system_id);
        return false;
    }
    
    if (entry->current_state == ROGUE_SYSTEM_STATE_UNINITIALIZED ||
        entry->current_state == ROGUE_SYSTEM_STATE_SHUTDOWN) {
        ROGUE_LOG_WARN("System '%s' is already shut down", entry->descriptor.name);
        return true; /* Already in desired state */
    }
    
    ROGUE_LOG_INFO("Shutting down system '%s'...", entry->descriptor.name);
    
    entry->descriptor.interface.shutdown(entry->descriptor.system_data);
    entry->current_state = ROGUE_SYSTEM_STATE_SHUTDOWN;
    
    ROGUE_LOG_INFO("System '%s' shut down successfully", entry->descriptor.name);
    return true;
}

bool rogue_integration_restart_system(uint32_t system_id) {
    RogueSystemEntry* entry = rogue_integration_get_system(system_id);
    if (!entry) {
        return false;
    }
    
    /* Check restart backoff (Phase 0.3.6) */
    double time_since_restart = get_current_time_ms() - entry->last_restart_time_ms;
    if (time_since_restart < entry->restart_backoff_ms) {
        ROGUE_LOG_WARN("System '%s' restart backoff active (%u ms remaining)", 
                       entry->descriptor.name, 
                       (uint32_t)(entry->restart_backoff_ms - time_since_restart));
        return false;
    }
    
    ROGUE_LOG_INFO("Restarting system '%s'...", entry->descriptor.name);
    
    /* Shutdown first if running */
    if (entry->current_state == ROGUE_SYSTEM_STATE_RUNNING || 
        entry->current_state == ROGUE_SYSTEM_STATE_PAUSED) {
        rogue_integration_shutdown_system(system_id);
    }
    
    /* Reset to uninitialized state */
    entry->current_state = ROGUE_SYSTEM_STATE_UNINITIALIZED;
    
    /* Initialize with exponential backoff on failure */
    bool result = rogue_integration_initialize_system(system_id);
    if (!result) {
        entry->restart_backoff_ms = (uint32_t)fmin(entry->restart_backoff_ms * 2, 60000); /* Cap at 1 minute */
    }
    
    return result;
}

bool rogue_integration_pause_system(uint32_t system_id) {
    RogueSystemEntry* entry = rogue_integration_get_system(system_id);
    if (!entry) {
        return false;
    }
    
    if (entry->current_state != ROGUE_SYSTEM_STATE_RUNNING) {
        ROGUE_LOG_WARN("Cannot pause system '%s' in state %s", 
                       entry->descriptor.name, SYSTEM_STATE_NAMES[entry->current_state]);
        return false;
    }
    
    entry->current_state = ROGUE_SYSTEM_STATE_PAUSED;
    ROGUE_LOG_INFO("System '%s' paused", entry->descriptor.name);
    return true;
}

bool rogue_integration_resume_system(uint32_t system_id) {
    RogueSystemEntry* entry = rogue_integration_get_system(system_id);
    if (!entry) {
        return false;
    }
    
    if (entry->current_state != ROGUE_SYSTEM_STATE_PAUSED) {
        ROGUE_LOG_WARN("Cannot resume system '%s' in state %s", 
                       entry->descriptor.name, SYSTEM_STATE_NAMES[entry->current_state]);
        return false;
    }
    
    entry->current_state = ROGUE_SYSTEM_STATE_RUNNING;
    ROGUE_LOG_INFO("System '%s' resumed", entry->descriptor.name);
    return true;
}

/* Dependency Management (Phase 0.2) */

bool rogue_integration_build_dependency_graph(void) {
    /* Phase 0.2.4: Calculate topological ordering for safe initialization */
    uint32_t ordered_systems[ROGUE_MAX_SYSTEMS];
    uint32_t ordered_count = 0;
    bool processed[ROGUE_MAX_SYSTEMS] = {false};
    
    /* First pass: systems with no dependencies */
    for (uint32_t i = 0; i < g_integration_manager.system_count; i++) {
        RogueSystemEntry* entry = &g_integration_manager.systems[i];
        if (entry->descriptor.hard_dep_count == 0) {
            ordered_systems[ordered_count++] = entry->descriptor.system_id;
            processed[i] = true;
        }
    }
    
    /* Subsequent passes: systems whose dependencies are satisfied */
    bool made_progress = true;
    while (made_progress && ordered_count < g_integration_manager.system_count) {
        made_progress = false;
        
        for (uint32_t i = 0; i < g_integration_manager.system_count; i++) {
            if (processed[i]) continue;
            
            RogueSystemEntry* entry = &g_integration_manager.systems[i];
            bool all_deps_satisfied = true;
            
            /* Check if all hard dependencies are in ordered list */
            for (uint8_t j = 0; j < entry->descriptor.hard_dep_count; j++) {
                uint32_t dep_id = entry->descriptor.hard_dependencies[j];
                bool dep_found = false;
                
                for (uint32_t k = 0; k < ordered_count; k++) {
                    if (ordered_systems[k] == dep_id) {
                        dep_found = true;
                        break;
                    }
                }
                
                if (!dep_found) {
                    all_deps_satisfied = false;
                    break;
                }
            }
            
            if (all_deps_satisfied) {
                ordered_systems[ordered_count++] = entry->descriptor.system_id;
                processed[i] = true;
                made_progress = true;
            }
        }
    }
    
    if (ordered_count != g_integration_manager.system_count) {
        ROGUE_LOG_ERROR("Circular dependency detected in system graph");
        return false;
    }
    
    /* Store initialization order */
    for (uint32_t i = 0; i < ordered_count; i++) {
        g_integration_manager.initialization_order[i] = ordered_systems[i];
    }
    
    ROGUE_LOG_INFO("Built dependency graph with %u systems", ordered_count);
    return true;
}

bool rogue_integration_validate_dependencies(void) {
    /* Check for circular dependencies (Phase 0.2.3) */
    for (uint32_t i = 0; i < g_integration_manager.system_count; i++) {
        RogueSystemEntry* entry = &g_integration_manager.systems[i];
        uint32_t visited[ROGUE_MAX_SYSTEMS];
        
        if (has_circular_dependency(entry->descriptor.system_id, entry->descriptor.system_id, visited, 0)) {
            ROGUE_LOG_ERROR("Circular dependency detected involving system '%s'", entry->descriptor.name);
            return false;
        }
    }
    
    /* Check that all dependencies exist */
    for (uint32_t i = 0; i < g_integration_manager.system_count; i++) {
        RogueSystemEntry* entry = &g_integration_manager.systems[i];
        
        for (uint8_t j = 0; j < entry->descriptor.hard_dep_count; j++) {
            uint32_t dep_id = entry->descriptor.hard_dependencies[j];
            if (!rogue_integration_get_system(dep_id)) {
                ROGUE_LOG_ERROR("System '%s' depends on non-existent system ID: %u", 
                               entry->descriptor.name, dep_id);
                return false;
            }
        }
        
        for (uint8_t j = 0; j < entry->descriptor.soft_dep_count; j++) {
            uint32_t dep_id = entry->descriptor.soft_dependencies[j];
            if (!rogue_integration_get_system(dep_id)) {
                ROGUE_LOG_WARN("System '%s' has soft dependency on non-existent system ID: %u", 
                              entry->descriptor.name, dep_id);
            }
        }
    }
    
    return true;
}

bool rogue_integration_get_initialization_order(uint32_t* order_buffer, size_t buffer_size) {
    if (!order_buffer || buffer_size < g_integration_manager.system_count) {
        return false;
    }
    
    for (uint32_t i = 0; i < g_integration_manager.system_count; i++) {
        order_buffer[i] = g_integration_manager.initialization_order[i];
    }
    
    return true;
}

/* Health Monitoring (Phase 0.3.4) */

void rogue_integration_update_system_health(uint32_t system_id) {
    RogueSystemEntry* entry = rogue_integration_get_system(system_id);
    if (!entry) {
        return;
    }
    
    /* Health is updated automatically during update calls */
    /* This function can be extended for additional health checks */
}

bool rogue_integration_is_system_healthy(uint32_t system_id) {
    RogueSystemEntry* entry = rogue_integration_get_system(system_id);
    if (!entry) {
        return false;
    }
    
    /* Consider system healthy if it's running and responsive */
    return (entry->current_state == ROGUE_SYSTEM_STATE_RUNNING) && 
           entry->health.is_responsive;
}

void rogue_integration_get_health_report(char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size < 1) {
        return;
    }
    
    char temp[256];
    buffer[0] = '\0';
    
    snprintf(temp, sizeof(temp), "Integration Manager Health Report\n");
#ifdef _MSC_VER
    strcat_s(buffer, buffer_size, temp);
#else
    strncat(buffer, temp, buffer_size - strlen(buffer) - 1);
#endif
    
    snprintf(temp, sizeof(temp), "Systems: %u/%u registered\n", 
             g_integration_manager.system_count, ROGUE_MAX_SYSTEMS);
#ifdef _MSC_VER
    strcat_s(buffer, buffer_size, temp);
#else
    strncat(buffer, temp, buffer_size - strlen(buffer) - 1);
#endif
    
    for (uint32_t i = 0; i < g_integration_manager.system_count; i++) {
        RogueSystemEntry* entry = &g_integration_manager.systems[i];
        snprintf(temp, sizeof(temp), "  %s: %s (Errors: %u, Restarts: %u)\n",
                entry->descriptor.name,
                SYSTEM_STATE_NAMES[entry->current_state],
                entry->health.error_count,
                entry->health.restart_count);
        
        if (strlen(buffer) + strlen(temp) + 1 < buffer_size) {
#ifdef _MSC_VER
            strcat_s(buffer, buffer_size, temp);
#else
            strcat(buffer, temp);
#endif
        }
    }
}

/* System Taxonomy Utilities (Phase 0.1) */

const char* rogue_integration_system_type_name(RogueSystemType type) {
    if (type >= ROGUE_SYSTEM_TYPE_COUNT) {
        return "Unknown";
    }
    return SYSTEM_TYPE_NAMES[type];
}

const char* rogue_integration_system_priority_name(RogueSystemPriority priority) {
    if (priority >= ROGUE_SYSTEM_PRIORITY_COUNT) {
        return "Unknown";
    }
    return SYSTEM_PRIORITY_NAMES[priority];
}

const char* rogue_integration_system_state_name(RogueSystemState state) {
    if (state >= ROGUE_SYSTEM_STATE_COUNT) {
        return "Unknown";
    }
    return SYSTEM_STATE_NAMES[state];
}

bool rogue_integration_has_capability(uint32_t system_id, RogueSystemCapability capability) {
    RogueSystemEntry* entry = rogue_integration_get_system(system_id);
    if (!entry) {
        return false;
    }
    
    return (entry->descriptor.capabilities & capability) != 0;
}

/* Performance Monitoring (Phase 0.7) */

double rogue_integration_get_average_update_time_ms(void) {
    if (g_integration_manager.update_call_count == 0) {
        return 0.0;
    }
    
    return g_integration_manager.total_update_time_ms / g_integration_manager.update_call_count;
}

double rogue_integration_get_max_update_time_ms(void) {
    return g_integration_manager.max_update_time_ms;
}

void rogue_integration_reset_performance_counters(void) {
    g_integration_manager.total_update_time_ms = 0.0;
    g_integration_manager.max_update_time_ms = 0.0;
    g_integration_manager.update_call_count = 0;
}
