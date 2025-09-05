#include "validation_wiring.h"
#include "snapshot_manager.h"
#include "state_validation_manager.h"
#include "system_taxonomy.h"
#include "tag_registry.h"

#include "../../content/schema_entities.h"
#include "../../content/schema_items.h"
#include "../../content/schema_tilesets.h"
#include "../../util/log.h"
#include "../app/app_state.h"
#include "../loot/loot_item_defs.h"
#include "../skills/skills_validate.h"

#include <stdio.h>
#include <string.h>

// Ensure we only register validators once per process to avoid duplicates when
// both the app boot and tests/tools call this helper.
static int s_validation_registered_once = 0;

// Small helpers to convert schema validation result to RogueValidationResult
static RogueValidationResult ok(void)
{
    RogueValidationResult r = {ROGUE_VALID_OK, 0u, "OK"};
    return r;
}
static RogueValidationResult warn_code(uint32_t code, const char* msg)
{
    RogueValidationResult r;
    r.severity = ROGUE_VALID_WARN;
    r.code = code;
    r.message = msg;
    return r;
}
static RogueValidationResult corrupt_code(uint32_t code, const char* msg)
{
    RogueValidationResult r;
    r.severity = ROGUE_VALID_CORRUPT;
    r.code = code;
    r.message = msg;
    return r;
}

// ----- System validators -----

// Items: validate current registry against schema
static RogueValidationResult validate_items(void* user)
{
    (void) user;
    int count = rogue_item_defs_count();
    if (count <= 0)
        return warn_code(1u, "No items registered");
    // Validate all in batch via helper
    RogueSchemaValidationResult res = {0};
    const RogueItemDef* first = rogue_item_def_at(0);
    if (!first)
        return warn_code(1u, "No items registered");
    if (!rogue_items_validate_defs(first, count, &res))
    {
        static char msg[128];
        const char* detail =
            (res.error_count > 0 && res.errors[0].message[0]) ? res.errors[0].message : "see logs";
        snprintf(msg, sizeof msg, "Items schema invalid: %s", detail);
        return corrupt_code(101u, msg);
    }
    // Build an array view over current defs (optional per-entry loop removed for performance)
    int ok_count = count;
    (void) ok_count;
    return ok();
}

// Entities: validate default assets discovered on disk
static RogueValidationResult validate_entities(void* user)
{
    (void) user;
    RogueSchemaValidationResult res = {0};
    int count = 0;
    if (!rogue_entities_validate_assets_default(&res, &count))
    {
        return corrupt_code(200u, "Entities schema validation failed");
    }
    if (count <= 0)
        return warn_code(2u, "No entities found");
    return ok();
}

// Tilesets: validate legacy -> JSON adapter and default atlas entries
static RogueValidationResult validate_tilesets(void* user)
{
    (void) user;
    RogueSchemaValidationResult res = {0};
    int count = 0;
    if (!rogue_tilesets_validate_assets_default(&res, &count))
    {
        return corrupt_code(300u, "Tilesets validation failed");
    }
    if (count <= 0)
        return warn_code(3u, "No tilesets found");
    return ok();
}

// Tags: validate tag registry JSON on disk if present
static RogueValidationResult validate_tag_registry(void* user)
{
    (void) user;
    char err[128] = {0};
    const char* paths[] = {"assets/tag_registry.json", "../assets/tag_registry.json",
                           "../../assets/tag_registry.json"};
    int tried = 0;
    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i)
    {
        int rc = rogue_tag_registry_validate_file(paths[i], err, sizeof err);
        if (rc == 0)
            return ok();
        if (rc < 0)
        {
            // Found a file but invalid
            static char msg[96];
            snprintf(msg, sizeof msg, "Tag registry invalid: %s", err);
            return corrupt_code(400u, msg);
        }
        tried++;
    }
    if (tried > 0)
        return warn_code(4u, "No tag_registry.json found");
    return ok();
}

// ----- Cross-rule validators -----
static RogueValidationResult cross_skills_rules(void* user)
{
    (void) user;
    char err[128] = {0};
    if (rogue_skills_validate_all(err, (int) sizeof err) != 0)
    {
        static char msg[128];
        // Ensure message is stable for ring buffer copy
        snprintf(msg, sizeof msg, "Skills cross-rule failed: %s", err[0] ? err : "error");
        return corrupt_code(500u, msg);
    }
    return ok();
}

// Optional repair hooks (no-op for content schemas)
static int no_repair(void* user, uint32_t code)
{
    (void) user;
    (void) code;
    return -1; // not repairable
}

void rogue_validation_register_default_checks(void)
{
    if (s_validation_registered_once)
        return;
    s_validation_registered_once = 1;
    // Register content systems with stable IDs from taxonomy
    // 6 = Loot & Item System, 5 = Skill System, 4 = Character Progression, etc.
    // We attach validators where we have schemas available.
    (void) rogue_validation_register_system(6, validate_items, no_repair, NULL);
    (void) rogue_validation_register_system(3, validate_entities, no_repair, NULL);
    (void) rogue_validation_register_system(9, validate_tilesets, no_repair, NULL);
    // Tag registry (no taxonomy id; use - choose Integration(15) or Content group; use 12 infra)
    (void) rogue_validation_register_system(12, validate_tag_registry, no_repair, NULL);

    // Cross rules
    (void) rogue_validation_register_cross_rule("skills", cross_skills_rules, NULL);

    // Reasonable default interval: run every 120 ticks (~2s at 60fps)
    rogue_validation_set_interval(120);
}
