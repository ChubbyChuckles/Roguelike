cmake_minimum_required(VERSION 3.20)
project(roguelike C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ROGUE_ENABLE_SDL "Build with SDL2 platform layer" ON)
option(ROGUE_ENABLE_SDL_IMAGE "Load PNG textures via SDL_image" ON)
option(ROGUE_ENABLE_SDL_MIXER "Enable audio playback (mp3/ogg) via SDL_mixer" ON)
option(ROGUE_WARNINGS_AS_ERRORS "Treat warnings as errors" ON)
option(ROGUE_CODE_COVERAGE "Enable code coverage instrumentation (GNU/Clang)" OFF)
option(ROGUE_SUPPRESS_WINDOWS_ERROR_DIALOGS "Suppress Windows/MSVC error popups in tests by routing errors to stderr" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(cmake/Warnings.cmake)
rogue_set_project_warnings(rogue_warnings)

if(ROGUE_ENABLE_SDL)
    find_package(SDL2 QUIET)
    if(ROGUE_ENABLE_SDL_IMAGE)
        find_package(SDL2_image QUIET)
        if(NOT SDL2_image_FOUND)
            message(WARNING "SDL2_image requested but not found; textures will not load unless fallback added")
        endif()
    endif()
    if(ROGUE_ENABLE_SDL_MIXER)
        find_package(SDL2_mixer QUIET)
        if(NOT SDL2_mixer_FOUND)
            message(WARNING "SDL2_mixer requested but not found; audio (mp3) disabled")
        endif()
    endif()
endif()

# Fallback/manual SDL2 discovery: allow specifying a root like C:/libs/SDL2 when CMake package config files are absent.
if(ROGUE_ENABLE_SDL AND NOT TARGET SDL2::SDL2)
    if(ROGUE_SDL2_ROOT)
        set(_SDL2_INC_HINT "${ROGUE_SDL2_ROOT}/include")
        set(_SDL2_LIB_HINT1 "${ROGUE_SDL2_ROOT}/lib")
        set(_SDL2_LIB_HINT2 "${ROGUE_SDL2_ROOT}/lib/x64")
        find_path(SDL2_INCLUDE_DIR SDL.h HINTS ${_SDL2_INC_HINT})
        find_library(SDL2_LIBRARY NAMES SDL2 SDL2-static HINTS ${_SDL2_LIB_HINT1} ${_SDL2_LIB_HINT2})
        if(SDL2_INCLUDE_DIR AND SDL2_LIBRARY)
            add_library(SDL2::SDL2 UNKNOWN IMPORTED)
            set_target_properties(SDL2::SDL2 PROPERTIES
                IMPORTED_LOCATION "${SDL2_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIR}"
            )
            message(STATUS "Using manual SDL2 root: ${ROGUE_SDL2_ROOT}")
        else()
            message(FATAL_ERROR "ROGUE_SDL2_ROOT provided (${ROGUE_SDL2_ROOT}) but SDL2 not found (looked for SDL.h and SDL2.lib)")
        endif()
    endif()
endif()

# If SDL still not found after manual root, abort with helpful message.
if(ROGUE_ENABLE_SDL AND NOT TARGET SDL2::SDL2)
    message(FATAL_ERROR "SDL2 not found. Provide -DROGUE_SDL2_ROOT=path (containing include/SDL.h and lib/SDL2.lib) or disable with -DROGUE_ENABLE_SDL=OFF")
endif()

if(ROGUE_CODE_COVERAGE)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        message(STATUS "Code coverage enabled")
        add_compile_options(--coverage -O0 -g)
        add_link_options(--coverage)
    else()
        message(WARNING "ROGUE_CODE_COVERAGE requested but compiler ${CMAKE_C_COMPILER_ID} not supported for coverage flags")
    endif()
endif()

add_library(rogue_core
    src/core/app/app_init.c
    src/core/app/app_step.c
    src/core/app/app_lifecycle.c
    src/core/app/app_test_helpers.c
    src/core/metrics.c
    src/core/app/app_state.c
    src/core/game_loop.c
    src/core/minimap/minimap.c
    src/core/minimap/minimap_loot_pings.c
    src/core/loot/loot_tooltip.c
    src/core/player/player_controller.c
    src/core/input_events.c
    src/core/player/player_render.c
    src/core/enemy/enemy_render.c
    src/core/hud/hud.c
    src/core/hud/hud_layout.c
    src/core/hud/hud_bars.c
    src/core/hud/hud_buff_belt.c
    src/core/hud/hud_overlays.c
    src/ui/core/ui_theme.c
    src/ui/core/ui_test_harness.c
    src/core/player/player_progress.c
    src/core/world_renderer.c
    src/core/animation_system.c
    # Integration plumbing
    src/core/integration/integration_manager.c
    src/core/integration/system_taxonomy.c
    src/core/integration/event_bus.c
    src/core/integration/json_schema.c
    src/core/integration/config_version.c
    src/core/integration/enemy_ai_bridge.c
    src/core/integration/combat_equip_bridge.c
    src/core/integration/combat_progression_bridge.c
    src/core/integration/worldgen_enemy_bridge.c
    src/core/integration/worldgen_resource_bridge.c
    src/core/integration/ui_integration_bridge.c
    src/core/integration/persistence_integration_bridge.c
    src/core/json_parser.c
    src/core/cfg_parser.c
    src/core/integration/cfg_migration.c
    src/core/integration/hot_reload.c
    src/core/integration/dependency_manager.c
    # persistence module split
    src/core/persistence/persistence_io.c
    src/core/persistence/persistence_world.c
    src/core/persistence/persistence_player.c
    src/core/persistence/persistence_autosave.c
    src/core/save_manager.c
    # skills module split
    src/core/skills/skills_registry.c
    src/core/skills/skills_runtime.c
    src/core/skills/skills_effects.c
    src/core/skills/skill_tree.c
    src/core/skills/skill_bar.c
    src/core/effect_spec.c
    src/core/buffs.c
    # projectiles module split
    src/core/projectiles/projectiles_spawn.c
    src/core/projectiles/projectiles_update.c
    src/core/projectiles/projectiles_render.c
    src/core/projectiles/projectiles_config.c
    src/core/damage_calc.c
    src/core/collision.c
    src/core/navigation.c
    src/core/asset_config.c
    src/core/platform.c
    src/core/tile_sprite_cache.c
    src/platform/platform_sdl.c
    src/graphics/renderer.c
    src/graphics/sprite.c
    # tile sprites module split
    src/graphics/tile_sprites_load.c
    src/graphics/tile_sprites_lookup.c
    src/graphics/tile_sprites_render.c
    src/graphics/png_wic.c
    src/input/input.c
    src/world/tilemap.c
    # world generation split for modularity
    src/world/world_gen_core.c
    src/world/world_gen_noise.c
    src/world/world_gen_biomes.c
    src/world/world_gen_features.c
    src/world/world_gen_config.c
    src/world/world_gen_biome_desc.c
    src/world/world_gen_biome_json.c
    src/world/world_gen_resource_json.c
    src/world/world_gen_biome_assets_validate.c
    src/world/world_gen_macro.c
    src/world/world_gen_local.c
    src/world/world_gen_rivers.c
    src/world/world_gen_structures.c
    src/world/world_gen_dungeon.c
    src/world/world_gen_spawns.c
    src/world/world_gen_resources.c
    src/world/world_gen_weather.c
    src/world/world_gen_stream.c
    src/world/world_gen_telemetry.c
    src/world/world_gen_optimization.c
    src/world/world_gen_modding.c
    src/world/world_gen_foundation.c
    src/world/world_gen_full.c
    src/graphics/font.c
    src/entities/player.c
    src/entities/enemy.c
    src/core/enemy/enemy_integration.c
    src/game/combat_events.c
    # maintainability phase M2: damage observer dispatch layer
    src/game/combat_observer.c
    src/game/combat_mitigation.c
    src/game/combat_player_state.c
    src/game/combat_guard.c
    src/game/combat_reaction.c
    src/game/combat_advanced.c
    src/game/combat_mobility.c
    src/game/combat_hooks.c
    src/game/combat_strike.c
    src/game/combat_attacks.c
    src/game/damage_numbers.c
    src/game/weapons.c
    src/game/armor.c
    src/core/enemy/enemy_system.c
    src/core/enemy/enemy_system_spawn.c
    src/core/enemy/enemy_system_ai.c
    src/core/enemy/enemy_ai_bt.c
    src/core/enemy/enemy_ai_intensity.c
    # AI Phase 1 scaffolding
    src/ai/core/behavior_tree.c
    src/ai/core/blackboard.c
    src/ai/nodes/basic_nodes.c
    src/ai/nodes/advanced_nodes.c
    src/ai/perception/perception.c
    src/ai/pathing/flow_field.c
    src/ai/pathing/local_avoidance.c
    src/core/start_screen.c
    src/core/localization.c
    src/core/player/player_assets.c
    src/util/log.c
    src/core/vegetation/vegetation_defs.c
    src/core/vegetation/vegetation_json.c
    src/core/vegetation/vegetation_generate.c
    src/core/vegetation/vegetation_render.c
    src/core/vegetation/vegetation_collision.c
    src/core/scene_drawlist.c
    src/core/loot/loot_item_defs.c
    src/core/loot/loot_item_defs_convert.c
    src/core/loot/loot_rebalance.c
    src/core/loot/loot_item_defs_sort.c
    src/core/loot/loot_tables.c
    src/core/loot/loot_instances.c
    src/core/inventory/inventory.c
    src/core/inventory/inventory_entries.c
    src/core/inventory/inventory_ui.c
    src/core/inventory/inventory_tags.c
    src/core/inventory/inventory_panel.c
    src/core/loot/loot_pickup.c
    src/core/loot/loot_multiplayer.c
    src/core/loot/loot_logging.c
    src/core/loot/loot_affixes.c
    src/core/loot/loot_filter.c
    src/core/path_utils.c
    src/core/loot/loot_rarity.c
        src/core/loot/loot_perf.c
    src/core/loot/loot_analytics.c
        src/core/loot/loot_vfx.c
    src/core/loot/loot_stats.c
    src/core/loot/loot_dynamic_weights.c
    src/core/loot/loot_console.c
    src/core/loot/loot_rarity_adv.c
    src/core/loot/loot_generation.c
    src/core/loot/loot_generation_affix.c
    src/core/loot/loot_drop_rates.c
    src/core/loot/loot_adaptive.c
    src/core/loot/loot_commands.c
    src/core/vendor/vendor.c
    src/core/vendor/econ_value.c
    src/core/vendor/econ_materials.c
    src/core/vendor/econ_inflow_sim.c
    src/core/vendor/vendor_registry.c
    src/core/vendor/vendor_inventory_templates.c
    src/core/vendor/vendor_pricing.c
    src/core/vendor/vendor_adaptive.c
    src/core/vendor/vendor_econ_balance.c
    src/core/vendor/vendor_perf.c
    src/core/vendor/vendor_reputation.c
    src/core/vendor/vendor_buyback.c
    src/core/vendor/vendor_tx_journal.c
    src/core/vendor/vendor_special_offers.c
    src/core/vendor/vendor_rng.c
    src/core/vendor/vendor_sinks.c
    src/core/vendor/vendor_crafting_integration.c
    src/core/vendor/economy.c
    src/core/vendor/salvage.c
    src/core/crafting/material_registry.c
    src/core/crafting/material_refine.c
    # Phase 7 determinism governance new modules
    src/core/crafting/rng_streams.c
    src/core/crafting/crafting_journal.c
    src/core/crafting/gathering.c
    src/core/crafting/crafting.c
    src/core/crafting/crafting_queue.c
    src/core/crafting/crafting_skill.c
    src/core/equipment/equipment_enhance.c
    src/core/equipment/equipment.c
    src/core/equipment/equipment_stats.c
    src/core/equipment/equipment_gems.c
    src/core/equipment/equipment_uniques.c
        src/core/equipment/equipment_enchant.c
        src/core/equipment/equipment_procs.c
        src/core/equipment/equipment_integrity.c
    src/core/equipment/equipment_content.c
    src/core/equipment/equipment_balance.c
    src/core/equipment/equipment_ui.c
    src/core/equipment/equipment_persist.c
    src/core/equipment/equipment_fuzz.c
    src/core/loadout_optimizer.c
    src/core/durability.c
    src/core/vendor/vendor_ui.c
    src/core/skills/skill_graph_runtime.c
    src/core/skills/skill_graph_assignment.c
    src/core/skills/skill_maze.c
    src/core/stat_cache.c
    src/core/progression/progression_stats.c
    src/core/progression/progression_xp.c
    src/core/progression/progression_attributes.c
    # progression phase 3 ratings & diminishing returns module
    src/core/progression/progression_ratings.c
    # progression phase 4 maze skill graph framework wrapper
    src/core/progression/progression_maze.c
    src/core/progression/progression_passives.c
    src/core/progression/progression_mastery.c
    src/core/progression/progression_specialization.c
    src/core/progression/progression_perpetual.c
    src/core/progression/progression_synergy.c
    src/core/progression/progression_persist.c
    src/core/progression/progression_award.c
    src/core/dialogue.c
    # UI Phase 1 scaffolding
    src/ui/core/ui_context.c
    # maintainability phase M3: unified kv parser utility
    src/util/kv_parser.c
    src/ai/core/ai_trace.c
        src/ai/core/ai_profiler.c
    src/ai/core/ai_scheduler.c
    src/ai/core/ai_agent_pool.c
    src/ai/core/ai_debug.c
    src/util/kv_schema.c
    src/util/hot_reload.c
    src/util/asset_dep.c
    src/util/determinism.c
    src/util/file_search.c
    src/game/hitbox.c
        src/core/equipment/equipment_budget_analyzer.c
        src/core/equipment/equipment_schema_docs.c
    src/game/hitbox_load.c
        src/game/lock_on.c
        src/game/infusions.c
        src/core/loot/loot_security.c
    src/core/loot/loot_api_doc.c
    src/core/equipment/equipment_perf.c
    src/core/equipment/equipment_modding.c
    src/core/inventory/inventory_tag_rules.c
    src/core/inventory/inventory_query.c
    src/game/hit_system.c
    src/game/hit_pixel_mask.c
    src/game/hit_feedback.c
    src/game/weapon_pose.c
    src/game/hit_debug_overlay.c
    src/core/enemy/enemy_difficulty.c
    src/core/enemy/enemy_difficulty_scaling.c
    src/core/enemy/enemy_adaptive.c
    src/core/enemy/enemy_modifiers.c
    src/core/enemy/encounter_composer.c
    # Crafting Phase 8 UI layer
    src/ui/crafting_ui.c
    src/core/crafting/crafting_automation.c
    src/core/crafting/crafting_economy.c
        src/core/crafting/crafting_analytics.c
    # Phase 4.1 unified entity id system
    src/core/integration/entity_id.c
        # Phase 4.2 shared memory pool system
        src/core/integration/memory_pool.c
        # Phase 4.3 cache management & invalidation system
        src/core/integration/cache_system.c
    # Phase 4.4 reference counting & lifecycle system
    src/core/integration/ref_count.c
    src/core/integration/cow.c
    src/core/integration/versioning.c
    src/core/integration/resource_lock.c
    # Phase 5.7 threading primitives & thread pool
    src/core/integration/threading.c
    src/core/integration/thread_pool.c
    # Phase 5.1 snapshot & 5.2 differential sync system
    src/core/integration/snapshot_manager.c
    # Phase 5.3 multi-system transaction framework
    src/core/integration/transaction_manager.c
    # Phase 5.4 rollback & recovery framework
    src/core/integration/rollback_manager.c
    # Phase 5.5 state validation & integrity checking
    src/core/integration/state_validation_manager.c
    # Phase 5.6 deadlock detection & prevention
    src/core/integration/deadlock_manager.c
)

# Suppress Windows error/abort popups in tests and tools; route to stderr instead.
if(WIN32)
    target_sources(rogue_core PRIVATE src/core/win_suppress_error_dialogs.c)
    if(ROGUE_SUPPRESS_WINDOWS_ERROR_DIALOGS)
        target_compile_definitions(rogue_core PUBLIC ROGUE_SUPPRESS_WINDOWS_ERROR_DIALOGS=1)
    endif()
endif()

target_include_directories(rogue_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(ROGUE_ENABLE_SDL)
    # Use PUBLIC to propagate include directories & compile definitions to dependents (tests, main)
    target_link_libraries(rogue_core PUBLIC SDL2::SDL2)
    target_compile_definitions(rogue_core PUBLIC ROGUE_HAVE_SDL=1)
    if(SDL2_image_FOUND)
        target_link_libraries(rogue_core PUBLIC SDL2_image::SDL2_image)
        target_compile_definitions(rogue_core PUBLIC ROGUE_HAVE_SDL_IMAGE=1)
    endif()
    if(SDL2_mixer_FOUND)
        target_link_libraries(rogue_core PUBLIC SDL2_mixer::SDL2_mixer)
        target_compile_definitions(rogue_core PUBLIC ROGUE_HAVE_SDL_MIXER=1)
    endif()
endif()

target_compile_options(rogue_core PRIVATE ${rogue_warnings})
if(ROGUE_WARNINGS_AS_ERRORS)
    target_compile_options(rogue_core PRIVATE $<$<C_COMPILER_ID:MSVC>:/WX> $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Werror>)
endif()

# Unit test: weapon pose loader
add_executable(test_weapon_pose_loader tests/unit/test_weapon_pose_loader.c)
target_link_libraries(test_weapon_pose_loader PRIVATE rogue_core)
if(ROGUE_ENABLE_SDL)
    target_link_libraries(test_weapon_pose_loader PRIVATE SDL2::SDL2)
endif()

# Phase 8 crafting UI tests
add_executable(test_crafting_phase8_ui tests/unit/test_crafting_phase8_ui.c)
target_link_libraries(test_crafting_phase8_ui PRIVATE rogue_core)
if(ROGUE_ENABLE_SDL)
    target_link_libraries(test_crafting_phase8_ui PRIVATE SDL2::SDL2)
endif()

# Phase 5.8 synchronization validation tests
add_executable(test_sync_phase5_8_snapshot tests/unit/test_sync_phase5_8_snapshot.c)
target_link_libraries(test_sync_phase5_8_snapshot PRIVATE rogue_core)
if(ROGUE_ENABLE_SDL)
    target_link_libraries(test_sync_phase5_8_snapshot PRIVATE SDL2::SDL2)
endif()

add_executable(test_sync_phase5_8_delta tests/unit/test_sync_phase5_8_delta.c)
target_link_libraries(test_sync_phase5_8_delta PRIVATE rogue_core)
if(ROGUE_ENABLE_SDL)
    target_link_libraries(test_sync_phase5_8_delta PRIVATE SDL2::SDL2)
endif()

add_executable(test_sync_phase5_8_tx tests/unit/test_sync_phase5_8_tx.c)
target_link_libraries(test_sync_phase5_8_tx PRIVATE rogue_core)
if(ROGUE_ENABLE_SDL)
    target_link_libraries(test_sync_phase5_8_tx PRIVATE SDL2::SDL2)
endif()

add_executable(test_sync_phase5_8_rollback tests/unit/test_sync_phase5_8_rollback.c)
target_link_libraries(test_sync_phase5_8_rollback PRIVATE rogue_core)
if(ROGUE_ENABLE_SDL)
    target_link_libraries(test_sync_phase5_8_rollback PRIVATE SDL2::SDL2)
endif()

add_executable(test_sync_phase5_8_integration tests/unit/test_sync_phase5_8_integration.c)
target_link_libraries(test_sync_phase5_8_integration PRIVATE rogue_core)
if(ROGUE_ENABLE_SDL)
    target_link_libraries(test_sync_phase5_8_integration PRIVATE SDL2::SDL2)
endif()

add_executable(test_crafting_phase9_automation tests/unit/test_crafting_phase9_automation.c)
target_link_libraries(test_crafting_phase9_automation PRIVATE rogue_core)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_crafting_phase9_automation PRIVATE SDL2::SDL2)
endif()

add_executable(test_crafting_phase10_economy tests/unit/test_crafting_phase10_economy.c)
target_link_libraries(test_crafting_phase10_economy PRIVATE rogue_core)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_crafting_phase10_economy PRIVATE SDL2::SDL2)
endif()

add_executable(test_crafting_phase11_analytics tests/unit/test_crafting_phase11_analytics.c)
target_link_libraries(test_crafting_phase11_analytics PRIVATE rogue_core)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_crafting_phase11_analytics PRIVATE SDL2::SDL2)
endif()

# Phase 5 snapshot & differential sync tests
add_executable(test_snapshot_manager tests/unit/test_snapshot_manager.c)
target_link_libraries(test_snapshot_manager PRIVATE rogue_core)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_snapshot_manager PRIVATE SDL2::SDL2)
endif()

# Phase 5.3 transaction framework tests
add_executable(test_transaction_manager tests/unit/test_transaction_manager.c)
target_link_libraries(test_transaction_manager PRIVATE rogue_core)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_transaction_manager PRIVATE SDL2::SDL2)
endif()

# Phase 5.4 rollback & recovery tests
add_executable(test_rollback_manager tests/unit/test_rollback_manager.c)
target_link_libraries(test_rollback_manager PRIVATE rogue_core)
target_include_directories(test_rollback_manager PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/core/integration)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_rollback_manager PRIVATE SDL2::SDL2)
endif()

add_executable(test_rollback_events tests/unit/test_rollback_events.c)
target_link_libraries(test_rollback_events PRIVATE rogue_core)
target_include_directories(test_rollback_events PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/core/integration)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_rollback_events PRIVATE SDL2::SDL2)
endif()

# Phase 5.5 state validation tests
add_executable(test_state_validation_manager tests/unit/test_state_validation_manager.c)
target_link_libraries(test_state_validation_manager PRIVATE rogue_core)
target_include_directories(test_state_validation_manager PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/core/integration)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_state_validation_manager PRIVATE SDL2::SDL2)
endif()

# Phase 5.6 deadlock detection tests
add_executable(test_deadlock_manager tests/unit/test_deadlock_manager.c)
target_link_libraries(test_deadlock_manager PRIVATE rogue_core)
target_include_directories(test_deadlock_manager PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/core/integration)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_deadlock_manager PRIVATE SDL2::SDL2)
endif()

# Phase 5.7 concurrency control tests
add_executable(test_thread_pool tests/unit/test_thread_pool.c)
target_link_libraries(test_thread_pool PRIVATE rogue_core)
target_include_directories(test_thread_pool PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/core/integration)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_thread_pool PRIVATE SDL2::SDL2)
endif()

add_executable(test_thread_barrier tests/unit/test_thread_barrier.c)
target_link_libraries(test_thread_barrier PRIVATE rogue_core)
target_include_directories(test_thread_barrier PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/core/integration)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_thread_barrier PRIVATE SDL2::SDL2)
endif()

add_executable(roguelike src/main.c)
target_link_libraries(roguelike PRIVATE rogue_core)

# Weapon pose GUI helper tool (standalone, lightweight)
add_executable(weapon_pose_tool tools/weapon_pose_tool.c)
if(ROGUE_ENABLE_SDL)
    target_link_libraries(weapon_pose_tool PRIVATE SDL2::SDL2)
    if(SDL2_image_FOUND)
        target_compile_definitions(weapon_pose_tool PRIVATE ROGUE_HAVE_SDL_IMAGE=1)
        target_link_libraries(weapon_pose_tool PRIVATE SDL2_image::SDL2_image)
    endif()
endif()
if(WIN32)
    target_link_libraries(weapon_pose_tool PRIVATE ole32 windowscodecs)
endif()

if(WIN32)
    target_link_libraries(rogue_core PUBLIC ole32 windowscodecs)
endif()

# Force 1s cooldowns for all active skills during rapid iteration/testing
target_compile_definitions(rogue_core PUBLIC ROGUE_TEST_SHORT_COOLDOWNS=1)

include(CTest)
enable_testing()
add_subdirectory(tests)

find_program(CLANG_FORMAT_EXE NAMES clang-format)
file(GLOB_RECURSE ALL_C_SOURCE_FILES src/*.c src/*.h tests/*.c tests/*.h)
if(CLANG_FORMAT_EXE)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXE} -i ${ALL_C_SOURCE_FILES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running clang-format" VERBATIM)
    add_custom_target(format-check
        COMMAND ${CLANG_FORMAT_EXE} -n -Werror ${ALL_C_SOURCE_FILES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Checking clang-format compliance" VERBATIM)
else()
    # Provide no-op targets so CI doesn't fail when clang-format missing
    add_custom_target(format
        COMMAND ${CMAKE_COMMAND} -E echo "clang-format not found; skipping format")
    add_custom_target(format-check
        COMMAND ${CMAKE_COMMAND} -E echo "clang-format not found; skipping format check")
endif()

find_program(CLANG_TIDY_EXE NAMES clang-tidy)
if(CLANG_TIDY_EXE)
    set_target_properties(rogue_core PROPERTIES C_CLANG_TIDY "${CLANG_TIDY_EXE}")
    add_custom_target(tidy
        COMMAND ${CLANG_TIDY_EXE} --version
        COMMENT "clang-tidy configured via target property (invoked automatically on build)"
    )
endif()

# Skill icon PNGs are loaded at runtime from assets/ by skill tree; ensure install step (if any) copies *.png
# (Assumes assets directory already handled elsewhere.)

# Project restructuring tool
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tools/restructure_project.c)
    add_executable(restructure_project tools/restructure_project.c)
    target_link_libraries(restructure_project PRIVATE rogue_core)
    target_include_directories(restructure_project PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

    add_custom_target(restructure
        COMMAND $<TARGET_FILE:restructure_project>
        DEPENDS restructure_project
        COMMENT "Running project restructuring tool"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()

# Project restructuring execution tool
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tools/execute_restructure.c)
    add_executable(execute_restructure tools/execute_restructure.c)
    target_link_libraries(execute_restructure PRIVATE rogue_core)
    target_include_directories(execute_restructure PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

    add_custom_target(execute_project_restructure
        COMMAND $<TARGET_FILE:execute_restructure>
        DEPENDS execute_restructure
        COMMENT "Executing project restructuring"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()

# Actual file moving tool
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tools/actually_move_files.c)
    add_executable(actually_move_files tools/actually_move_files.c)
    target_link_libraries(actually_move_files PRIVATE rogue_core)
    target_include_directories(actually_move_files PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

    add_custom_target(move_files_now
        COMMAND $<TARGET_FILE:actually_move_files>
        DEPENDS actually_move_files
        COMMENT "Actually moving project files"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()

# Structure analysis tool
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tools/analyze_structure.c)
    add_executable(analyze_structure tools/analyze_structure.c)
    target_link_libraries(analyze_structure PRIVATE rogue_core)
    target_include_directories(analyze_structure PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endif()
