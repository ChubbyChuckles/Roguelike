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

#include "system_taxonomy.h"
#include "../../util/log.h"
#include <stdio.h>
#include <string.h>

/* Global taxonomy instance */
RogueSystemTaxonomy g_system_taxonomy = {0};

/* Core API Implementation */

bool rogue_system_taxonomy_init(void)
{
    memset(&g_system_taxonomy, 0, sizeof(RogueSystemTaxonomy));
    g_system_taxonomy.initialized = true;

    /* Populate with known systems from roadmap analysis */
    rogue_system_taxonomy_populate_known_systems();

    ROGUE_LOG_INFO("System taxonomy initialized with %u known systems",
                   g_system_taxonomy.system_count);
    return true;
}

void rogue_system_taxonomy_shutdown(void)
{
    memset(&g_system_taxonomy, 0, sizeof(RogueSystemTaxonomy));
    ROGUE_LOG_INFO("System taxonomy shutdown complete");
}

/* System enumeration (Phase 0.1.1) */

bool rogue_system_taxonomy_add_system(const RogueSystemInfo* system_info)
{
    if (!system_info || !g_system_taxonomy.initialized)
    {
        return false;
    }

    if (g_system_taxonomy.system_count >= ROGUE_TAXONOMY_MAX_SYSTEMS)
    {
        ROGUE_LOG_ERROR("Cannot add system '%s': taxonomy database full", system_info->name);
        return false;
    }

    /* Check for duplicate names */
    if (rogue_system_taxonomy_find_system_by_name(system_info->name) != NULL)
    {
        ROGUE_LOG_ERROR("System '%s' already exists in taxonomy", system_info->name);
        return false;
    }

    /* Add system to taxonomy */
    g_system_taxonomy.systems[g_system_taxonomy.system_count] = *system_info;
    g_system_taxonomy.system_count++;

    ROGUE_LOG_INFO("Added system '%s' to taxonomy (Type: %s, Status: %s)", system_info->name,
                   rogue_integration_system_type_name(system_info->type),
                   system_info->implementation_status);

    return true;
}

const RogueSystemInfo* rogue_system_taxonomy_get_system(uint32_t system_id)
{
    for (uint32_t i = 0; i < g_system_taxonomy.system_count; i++)
    {
        if (g_system_taxonomy.systems[i].system_id == system_id)
        {
            return &g_system_taxonomy.systems[i];
        }
    }
    return NULL;
}

const RogueSystemInfo* rogue_system_taxonomy_find_system_by_name(const char* name)
{
    if (!name)
        return NULL;

    for (uint32_t i = 0; i < g_system_taxonomy.system_count; i++)
    {
        if (strcmp(g_system_taxonomy.systems[i].name, name) == 0)
        {
            return &g_system_taxonomy.systems[i];
        }
    }
    return NULL;
}

uint32_t rogue_system_taxonomy_get_system_count(void) { return g_system_taxonomy.system_count; }

/* System classification utilities (Phase 0.1.2-0.1.3) */

uint32_t rogue_system_taxonomy_count_by_type(RogueSystemType type)
{
    uint32_t count = 0;
    for (uint32_t i = 0; i < g_system_taxonomy.system_count; i++)
    {
        if (g_system_taxonomy.systems[i].type == type)
        {
            count++;
        }
    }
    return count;
}

uint32_t rogue_system_taxonomy_count_by_priority(RogueSystemPriority priority)
{
    uint32_t count = 0;
    for (uint32_t i = 0; i < g_system_taxonomy.system_count; i++)
    {
        if (g_system_taxonomy.systems[i].priority == priority)
        {
            count++;
        }
    }
    return count;
}

uint32_t rogue_system_taxonomy_count_implemented(void)
{
    uint32_t count = 0;
    for (uint32_t i = 0; i < g_system_taxonomy.system_count; i++)
    {
        if (g_system_taxonomy.systems[i].is_implemented)
        {
            count++;
        }
    }
    return count;
}

uint32_t rogue_system_taxonomy_count_by_capability(RogueSystemCapability capability)
{
    uint32_t count = 0;
    for (uint32_t i = 0; i < g_system_taxonomy.system_count; i++)
    {
        if (g_system_taxonomy.systems[i].capabilities & capability)
        {
            count++;
        }
    }
    return count;
}

/* Capability matrix generation (Phase 0.1.4) */

void rogue_system_taxonomy_generate_capability_matrix(char* buffer, size_t buffer_size)
{
    if (!buffer || buffer_size < 1)
        return;

    char temp[512];
    buffer[0] = '\0';

    snprintf(temp, sizeof(temp), "System Capability Matrix\n");
    snprintf(temp + strlen(temp), sizeof(temp) - strlen(temp), "========================\n");

    /* Ensure we have room for the header */
    if (strlen(temp) + 1 < buffer_size)
    {
#ifdef _MSC_VER
        strcat_s(buffer, buffer_size, temp);
#else
        strncat(buffer, temp, buffer_size - strlen(buffer) - 1);
#endif
    }
    else
    {
        /* Buffer too small, just return empty buffer */
        return;
    }

    const char* capability_names[] = {"Provides Entities",  "Consumes Events", "Produces Events",
                                      "Requires Rendering", "Requires Update", "Configurable",
                                      "Serializable",       "Hot Reloadable"};
    RogueSystemCapability capabilities[] = {
        ROGUE_SYSTEM_CAP_PROVIDES_ENTITIES, ROGUE_SYSTEM_CAP_CONSUMES_EVENTS,
        ROGUE_SYSTEM_CAP_PRODUCES_EVENTS,   ROGUE_SYSTEM_CAP_REQUIRES_RENDERING,
        ROGUE_SYSTEM_CAP_REQUIRES_UPDATE,   ROGUE_SYSTEM_CAP_CONFIGURABLE,
        ROGUE_SYSTEM_CAP_SERIALIZABLE,      ROGUE_SYSTEM_CAP_HOT_RELOADABLE};

    for (int cap = 0; cap < 8; cap++)
    {
        uint32_t count = rogue_system_taxonomy_count_by_capability(capabilities[cap]);
        snprintf(temp, sizeof(temp), "%-18s: %2u systems\n", capability_names[cap], count);

        if (strlen(buffer) + strlen(temp) + 1 < buffer_size)
        {
#ifdef _MSC_VER
            strcat_s(buffer, buffer_size, temp);
#else
            strncat(buffer, temp, buffer_size - strlen(buffer) - 1);
#endif
        }
    }
}

/* Resource usage analysis (Phase 0.1.6) */

void rogue_system_taxonomy_analyze_resource_usage(char* buffer, size_t buffer_size)
{
    if (!buffer || buffer_size < 1)
        return;

    char temp[512];
    buffer[0] = '\0';

    snprintf(temp, sizeof(temp), "System Resource Usage Analysis\n");
    snprintf(temp + strlen(temp), sizeof(temp) - strlen(temp), "==============================\n");

    /* Ensure we have room for the header */
    if (strlen(temp) + 1 < buffer_size)
    {
#ifdef _MSC_VER
        strcat_s(buffer, buffer_size, temp);
#else
        strncat(buffer, temp, buffer_size - strlen(buffer) - 1);
#endif
    }
    else
    {
        /* Buffer too small, just return empty buffer */
        return;
    }

    uint32_t total_systems = g_system_taxonomy.system_count;
    uint32_t high_cpu_systems = 0;
    uint32_t high_memory_systems = 0;
    uint32_t io_intensive_systems = 0;

    for (uint32_t i = 0; i < g_system_taxonomy.system_count; i++)
    {
        /* Note: Resource usage would be filled from actual system descriptors when registered */
        /* This is a placeholder analysis based on system types */
        switch (g_system_taxonomy.systems[i].type)
        {
        case ROGUE_SYSTEM_TYPE_CORE:
            high_cpu_systems++;
            break;
        case ROGUE_SYSTEM_TYPE_CONTENT:
            high_memory_systems++;
            break;
        case ROGUE_SYSTEM_TYPE_UI:
            /* UI systems typically moderate resource usage */
            break;
        case ROGUE_SYSTEM_TYPE_INFRASTRUCTURE:
            io_intensive_systems++;
            break;
        default:
            break;
        }
    }

    snprintf(temp, sizeof(temp), "Total Systems: %u\n", total_systems);
    snprintf(temp + strlen(temp), sizeof(temp) - strlen(temp), "High CPU Usage: %u (%.1f%%)\n",
             high_cpu_systems, (float) high_cpu_systems * 100.0f / total_systems);
    snprintf(temp + strlen(temp), sizeof(temp) - strlen(temp), "High Memory Usage: %u (%.1f%%)\n",
             high_memory_systems, (float) high_memory_systems * 100.0f / total_systems);
    snprintf(temp + strlen(temp), sizeof(temp) - strlen(temp), "I/O Intensive: %u (%.1f%%)\n",
             io_intensive_systems, (float) io_intensive_systems * 100.0f / total_systems);

    if (strlen(buffer) + strlen(temp) + 1 < buffer_size)
    {
#ifdef _MSC_VER
        strcat_s(buffer, buffer_size, temp);
#else
        strcat(buffer, temp);
#endif
    }
}

/* Initialization sequence analysis (Phase 0.1.5) */

void rogue_system_taxonomy_generate_init_report(char* buffer, size_t buffer_size)
{
    if (!buffer || buffer_size < 1)
        return;

    char temp[512];
    buffer[0] = '\0';

    snprintf(temp, sizeof(temp), "System Initialization Requirements\n");
    snprintf(temp + strlen(temp), sizeof(temp) - strlen(temp),
             "==================================\n");
#ifdef _MSC_VER
    strcat_s(buffer, buffer_size, temp);
#else
    strncat(buffer, temp, buffer_size - strlen(buffer) - 1);
#endif

    /* Group systems by priority for initialization order */
    for (int priority = 0; priority < ROGUE_SYSTEM_PRIORITY_COUNT; priority++)
    {
        uint32_t count = rogue_system_taxonomy_count_by_priority((RogueSystemPriority) priority);
        if (count > 0)
        {
            snprintf(temp, sizeof(temp), "\n%s Priority Systems (%u):\n",
                     rogue_integration_system_priority_name((RogueSystemPriority) priority), count);

            if (strlen(buffer) + strlen(temp) + 1 < buffer_size)
            {
#ifdef _MSC_VER
                strcat_s(buffer, buffer_size, temp);
#else
                strcat(buffer, temp);
#endif
            }

            for (uint32_t i = 0; i < g_system_taxonomy.system_count; i++)
            {
                const RogueSystemInfo* system = &g_system_taxonomy.systems[i];
                if (system->priority == priority)
                {
                    snprintf(temp, sizeof(temp), "  - %s (%s) [%s]\n", system->name,
                             rogue_integration_system_type_name(system->type),
                             system->implementation_status);

                    if (strlen(buffer) + strlen(temp) + 1 < buffer_size)
                    {
#ifdef _MSC_VER
                        strcat_s(buffer, buffer_size, temp);
#else
                        strcat(buffer, temp);
#endif
                    }
                }
            }
        }
    }
}

/* Populate taxonomy with known systems based on roadmap analysis */

void rogue_system_taxonomy_populate_known_systems(void)
{
    /* Core Systems (Phase 0.1.2) */

    RogueSystemInfo ai_system = {
        .system_id = 1,
        .name = "AI System",
        .description = "Enemy AI behavior trees, pathfinding, and decision making",
        .type = ROGUE_SYSTEM_TYPE_CORE,
        .priority = ROGUE_SYSTEM_PRIORITY_CRITICAL,
        .capabilities = ROGUE_SYSTEM_CAP_REQUIRES_UPDATE | ROGUE_SYSTEM_CAP_CONFIGURABLE |
                        ROGUE_SYSTEM_CAP_SERIALIZABLE,
        .is_implemented = true,
        .implementation_status = "Done (phases 0-11)",
        .version = "11.0"};
    rogue_system_taxonomy_add_system(&ai_system);

    RogueSystemInfo combat_system = {
        .system_id = 2,
        .name = "Combat System",
        .description = "Damage calculation, hit detection, status effects, and combat mechanics",
        .type = ROGUE_SYSTEM_TYPE_CORE,
        .priority = ROGUE_SYSTEM_PRIORITY_CRITICAL,
        .capabilities = ROGUE_SYSTEM_CAP_REQUIRES_UPDATE | ROGUE_SYSTEM_CAP_PRODUCES_EVENTS |
                        ROGUE_SYSTEM_CAP_CONFIGURABLE,
        .is_implemented = true,
        .implementation_status = "Done (most phases)",
        .version = "7.0"};
    rogue_system_taxonomy_add_system(&combat_system);

    RogueSystemInfo enemy_integration = {
        .system_id = 3,
        .name = "Enemy Integration",
        .description = "Enemy spawning, lifecycle management, and world integration",
        .type = ROGUE_SYSTEM_TYPE_CORE,
        .priority = ROGUE_SYSTEM_PRIORITY_CRITICAL,
        .capabilities = ROGUE_SYSTEM_CAP_PROVIDES_ENTITIES | ROGUE_SYSTEM_CAP_REQUIRES_UPDATE |
                        ROGUE_SYSTEM_CAP_CONFIGURABLE,
        .is_implemented = true,
        .implementation_status = "Done (phases 0-6)",
        .version = "6.0"};
    rogue_system_taxonomy_add_system(&enemy_integration);

    /* Content Systems */

    RogueSystemInfo character_progression = {
        .system_id = 4,
        .name = "Character Progression",
        .description = "Experience, leveling, skill trees, and character advancement",
        .type = ROGUE_SYSTEM_TYPE_CONTENT,
        .priority = ROGUE_SYSTEM_PRIORITY_IMPORTANT,
        .capabilities = ROGUE_SYSTEM_CAP_REQUIRES_UPDATE | ROGUE_SYSTEM_CAP_SERIALIZABLE |
                        ROGUE_SYSTEM_CAP_PRODUCES_EVENTS,
        .is_implemented = true,
        .implementation_status = "Done (phases 0-12)",
        .version = "12.0"};
    rogue_system_taxonomy_add_system(&character_progression);

    RogueSystemInfo skill_system = {
        .system_id = 5,
        .name = "Skill System",
        .description = "Active and passive skills, skill trees, and ability management",
        .type = ROGUE_SYSTEM_TYPE_CONTENT,
        .priority = ROGUE_SYSTEM_PRIORITY_IMPORTANT,
        .capabilities = ROGUE_SYSTEM_CAP_REQUIRES_UPDATE | ROGUE_SYSTEM_CAP_CONFIGURABLE |
                        ROGUE_SYSTEM_CAP_SERIALIZABLE,
        .is_implemented = true,
        .implementation_status = "Partial (phases 0-1 done)",
        .version = "1.0"};
    rogue_system_taxonomy_add_system(&skill_system);

    RogueSystemInfo loot_system = {
        .system_id = 6,
        .name = "Loot & Item System",
        .description = "Item generation, affixes, rarity, and loot tables",
        .type = ROGUE_SYSTEM_TYPE_CONTENT,
        .priority = ROGUE_SYSTEM_PRIORITY_IMPORTANT,
        .capabilities = ROGUE_SYSTEM_CAP_PROVIDES_ENTITIES | ROGUE_SYSTEM_CAP_CONFIGURABLE |
                        ROGUE_SYSTEM_CAP_SERIALIZABLE,
        .is_implemented = true,
        .implementation_status = "Done (phases 1-8)",
        .version = "8.0"};
    rogue_system_taxonomy_add_system(&loot_system);

    RogueSystemInfo equipment_system = {
        .system_id = 7,
        .name = "Equipment System",
        .description = "Equipment slots, stat bonuses, durability, and equipment management",
        .type = ROGUE_SYSTEM_TYPE_CONTENT,
        .priority = ROGUE_SYSTEM_PRIORITY_IMPORTANT,
        .capabilities = ROGUE_SYSTEM_CAP_REQUIRES_UPDATE | ROGUE_SYSTEM_CAP_SERIALIZABLE |
                        ROGUE_SYSTEM_CAP_CONFIGURABLE,
        .is_implemented = true,
        .implementation_status = "Done (most phases)",
        .version = "18.0"};
    rogue_system_taxonomy_add_system(&equipment_system);

    RogueSystemInfo inventory_system = {
        .system_id = 8,
        .name = "Inventory System",
        .description = "Item storage, sorting, filtering, and inventory management",
        .type = ROGUE_SYSTEM_TYPE_CONTENT,
        .priority = ROGUE_SYSTEM_PRIORITY_IMPORTANT,
        .capabilities = ROGUE_SYSTEM_CAP_SERIALIZABLE | ROGUE_SYSTEM_CAP_CONFIGURABLE,
        .is_implemented = true,
        .implementation_status = "Done (most phases)",
        .version = "13.0"};
    rogue_system_taxonomy_add_system(&inventory_system);

    RogueSystemInfo crafting_system = {
        .system_id = 9,
        .name = "Crafting & Gathering",
        .description = "Resource gathering, crafting recipes, and material processing",
        .type = ROGUE_SYSTEM_TYPE_CONTENT,
        .priority = ROGUE_SYSTEM_PRIORITY_OPTIONAL,
        .capabilities = ROGUE_SYSTEM_CAP_REQUIRES_UPDATE | ROGUE_SYSTEM_CAP_CONFIGURABLE |
                        ROGUE_SYSTEM_CAP_SERIALIZABLE,
        .is_implemented = true,
        .implementation_status = "Done (phases 0-8)",
        .version = "8.0"};
    rogue_system_taxonomy_add_system(&crafting_system);

    RogueSystemInfo vendor_system = {
        .system_id = 10,
        .name = "Vendor System",
        .description = "NPC vendors, trading, economy, and commerce mechanics",
        .type = ROGUE_SYSTEM_TYPE_CONTENT,
        .priority = ROGUE_SYSTEM_PRIORITY_OPTIONAL,
        .capabilities = ROGUE_SYSTEM_CAP_REQUIRES_UPDATE | ROGUE_SYSTEM_CAP_SERIALIZABLE |
                        ROGUE_SYSTEM_CAP_CONFIGURABLE,
        .is_implemented = true,
        .implementation_status = "Done (most phases)",
        .version = "13.0"};
    rogue_system_taxonomy_add_system(&vendor_system);

    /* UI Systems */

    RogueSystemInfo ui_system = {
        .system_id = 11,
        .name = "UI System",
        .description = "User interface rendering, input handling, and UI management",
        .type = ROGUE_SYSTEM_TYPE_UI,
        .priority = ROGUE_SYSTEM_PRIORITY_IMPORTANT,
        .capabilities = ROGUE_SYSTEM_CAP_REQUIRES_RENDERING | ROGUE_SYSTEM_CAP_REQUIRES_UPDATE |
                        ROGUE_SYSTEM_CAP_CONFIGURABLE,
        .is_implemented = true,
        .implementation_status = "Done (phases 0-7)",
        .version = "7.0"};
    rogue_system_taxonomy_add_system(&ui_system);

    /* Infrastructure Systems */

    RogueSystemInfo persistence_system = {
        .system_id = 12,
        .name = "Persistence & Migration",
        .description = "Save/load system, data migration, and persistent storage",
        .type = ROGUE_SYSTEM_TYPE_INFRASTRUCTURE,
        .priority = ROGUE_SYSTEM_PRIORITY_CRITICAL,
        .capabilities = ROGUE_SYSTEM_CAP_SERIALIZABLE | ROGUE_SYSTEM_CAP_CONFIGURABLE,
        .is_implemented = true,
        .implementation_status = "Done (all phases)",
        .version = "9.0"};
    rogue_system_taxonomy_add_system(&persistence_system);

    RogueSystemInfo world_generation = {
        .system_id = 13,
        .name = "World Generation",
        .description = "Procedural world generation, biomes, and terrain creation",
        .type = ROGUE_SYSTEM_TYPE_INFRASTRUCTURE,
        .priority = ROGUE_SYSTEM_PRIORITY_CRITICAL,
        .capabilities = ROGUE_SYSTEM_CAP_PROVIDES_ENTITIES | ROGUE_SYSTEM_CAP_CONFIGURABLE,
        .is_implemented = true,
        .implementation_status = "Done (most phases)",
        .version = "14.0"};
    rogue_system_taxonomy_add_system(&world_generation);

    RogueSystemInfo dialogue_system = {
        .system_id = 14,
        .name = "Dialogue System",
        .description = "NPC dialogue, conversation trees, and narrative content",
        .type = ROGUE_SYSTEM_TYPE_CONTENT,
        .priority = ROGUE_SYSTEM_PRIORITY_OPTIONAL,
        .capabilities = ROGUE_SYSTEM_CAP_CONFIGURABLE | ROGUE_SYSTEM_CAP_SERIALIZABLE |
                        ROGUE_SYSTEM_CAP_HOT_RELOADABLE,
        .is_implemented = true,
        .implementation_status = "Done (all phases)",
        .version = "7.0"};
    rogue_system_taxonomy_add_system(&dialogue_system);

    /* Integration Plumbing (this system) */

    RogueSystemInfo integration_system = {
        .system_id = 15,
        .name = "Integration Plumbing",
        .description = "Cross-system communication, event buses, and system coordination",
        .type = ROGUE_SYSTEM_TYPE_INFRASTRUCTURE,
        .priority = ROGUE_SYSTEM_PRIORITY_CRITICAL,
        .capabilities = ROGUE_SYSTEM_CAP_REQUIRES_UPDATE | ROGUE_SYSTEM_CAP_PRODUCES_EVENTS |
                        ROGUE_SYSTEM_CAP_CONSUMES_EVENTS,
        .is_implemented = false,
        .implementation_status = "Partial (Phase 0.1 in progress)",
        .version = "0.1"};
    rogue_system_taxonomy_add_system(&integration_system);
}

/* Validation and integrity checks */

bool rogue_system_taxonomy_validate(void)
{
    if (!g_system_taxonomy.initialized)
    {
        ROGUE_LOG_ERROR("System taxonomy not initialized");
        return false;
    }

    /* Check for duplicate system IDs */
    for (uint32_t i = 0; i < g_system_taxonomy.system_count; i++)
    {
        for (uint32_t j = i + 1; j < g_system_taxonomy.system_count; j++)
        {
            if (g_system_taxonomy.systems[i].system_id == g_system_taxonomy.systems[j].system_id)
            {
                ROGUE_LOG_ERROR("Duplicate system ID %u found: '%s' and '%s'",
                                g_system_taxonomy.systems[i].system_id,
                                g_system_taxonomy.systems[i].name,
                                g_system_taxonomy.systems[j].name);
                return false;
            }
        }
    }

    /* Validate system data integrity */
    for (uint32_t i = 0; i < g_system_taxonomy.system_count; i++)
    {
        const RogueSystemInfo* system = &g_system_taxonomy.systems[i];

        if (!system->name || strlen(system->name) == 0)
        {
            ROGUE_LOG_ERROR("System at index %u has invalid name", i);
            return false;
        }

        if (system->type >= ROGUE_SYSTEM_TYPE_COUNT)
        {
            ROGUE_LOG_ERROR("System '%s' has invalid type %d", system->name, system->type);
            return false;
        }

        if (system->priority >= ROGUE_SYSTEM_PRIORITY_COUNT)
        {
            ROGUE_LOG_ERROR("System '%s' has invalid priority %d", system->name, system->priority);
            return false;
        }
    }

    ROGUE_LOG_INFO("System taxonomy validation passed");
    return true;
}

void rogue_system_taxonomy_generate_report(char* buffer, size_t buffer_size)
{
    if (!buffer || buffer_size < 1)
        return;

    char temp[512];
    buffer[0] = '\0';

    snprintf(temp, sizeof(temp), "System Taxonomy Report\n");
    snprintf(temp + strlen(temp), sizeof(temp) - strlen(temp), "======================\n\n");
#ifdef _MSC_VER
    strcat_s(buffer, buffer_size, temp);
#else
    strncat(buffer, temp, buffer_size - strlen(buffer) - 1);
#endif

    /* Summary statistics */
    uint32_t total = g_system_taxonomy.system_count;
    uint32_t implemented = rogue_system_taxonomy_count_implemented();
    uint32_t core_systems = rogue_system_taxonomy_count_by_type(ROGUE_SYSTEM_TYPE_CORE);
    uint32_t content_systems = rogue_system_taxonomy_count_by_type(ROGUE_SYSTEM_TYPE_CONTENT);
    uint32_t ui_systems = rogue_system_taxonomy_count_by_type(ROGUE_SYSTEM_TYPE_UI);
    uint32_t infrastructure_systems =
        rogue_system_taxonomy_count_by_type(ROGUE_SYSTEM_TYPE_INFRASTRUCTURE);

    snprintf(temp, sizeof(temp), "Summary Statistics:\n");
    snprintf(temp + strlen(temp), sizeof(temp) - strlen(temp), "  Total Systems: %u\n", total);
    snprintf(temp + strlen(temp), sizeof(temp) - strlen(temp), "  Implemented: %u (%.1f%%)\n",
             implemented, (float) implemented * 100.0f / total);
    snprintf(temp + strlen(temp), sizeof(temp) - strlen(temp),
             "  Core: %u, Content: %u, UI: %u, Infrastructure: %u\n\n", core_systems,
             content_systems, ui_systems, infrastructure_systems);

    if (strlen(buffer) + strlen(temp) + 1 < buffer_size)
    {
#ifdef _MSC_VER
        strcat_s(buffer, buffer_size, temp);
#else
        strcat(buffer, temp);
#endif
    }

    /* Implementation status breakdown */
    snprintf(temp, sizeof(temp), "Implementation Status:\n");
    for (uint32_t i = 0; i < g_system_taxonomy.system_count && strlen(buffer) + 200 < buffer_size;
         i++)
    {
        const RogueSystemInfo* system = &g_system_taxonomy.systems[i];
        snprintf(temp + strlen(temp), sizeof(temp) - strlen(temp), "  %-20s: %s\n", system->name,
                 system->implementation_status);
    }

    if (strlen(buffer) + strlen(temp) + 1 < buffer_size)
    {
#ifdef _MSC_VER
        strcat_s(buffer, buffer_size, temp);
#else
        strcat(buffer, temp);
#endif
    }
}
