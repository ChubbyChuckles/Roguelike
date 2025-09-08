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
option(ROGUE_ENABLE_DEBUG_OVERLAY "Enable the unified in-game debug overlay system" ON)
option(ROGUE_ENABLE_JSON_CONTENT "Enable content JSON system (schemas, IO)" ON)
option(ROGUE_BUILD_DOCS "Enable building Doxygen documentation" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(cmake/Warnings.cmake)
rogue_set_project_warnings(rogue_warnings)
include(cmake/PathUtils.cmake)

# Build metadata (git hash/branch and build time) for Credits/Legal overlay
# Try to obtain from git; fall back to "unknown" if unavailable.
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE ROGUE_GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
if(ROGUE_GIT_HASH STREQUAL "")
    set(ROGUE_GIT_HASH "unknown")
endif()
execute_process(
    COMMAND git rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE ROGUE_GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
if(ROGUE_GIT_BRANCH STREQUAL "")
    set(ROGUE_GIT_BRANCH "unknown")
endif()
string(TIMESTAMP ROGUE_BUILD_TIME "%Y-%m-%d %H:%M:%S")

if(ROGUE_ENABLE_SDL)
    find_package(SDL2 QUIET)
    if(SDL2_FOUND)
        # Attempt to print SDL2 version if headers are discoverable. SDL2_INCLUDE_DIRS may be a list.
        if(DEFINED SDL2_INCLUDE_DIRS)
            list(GET SDL2_INCLUDE_DIRS 0 _SDL2_INC_DIR)
            set(_SDL2_HDR_CANDIDATES
                "${_SDL2_INC_DIR}/SDL2/SDL_version.h"
                "${_SDL2_INC_DIR}/SDL_version.h")
            set(_sdlver_hdr "")
            foreach(_cand IN LISTS _SDL2_HDR_CANDIDATES)
                if(EXISTS "${_cand}")
                    file(READ "${_cand}" _sdlver_hdr LIMIT 4096 ERROR_QUIET)
                    if(_sdlver_hdr)
                        break()
                    endif()
                endif()
            endforeach()
            if(_sdlver_hdr)
                string(REGEX MATCH "#define SDL_MAJOR_VERSION ([0-9]+)" _m "${_sdlver_hdr}")
                string(REGEX REPLACE ".* ([0-9]+)" "\\1" _maj "${_m}")
                string(REGEX MATCH "#define SDL_MINOR_VERSION ([0-9]+)" _n "${_sdlver_hdr}")
                string(REGEX REPLACE ".* ([0-9]+)" "\\1" _min "${_n}")
                string(REGEX MATCH "#define SDL_PATCHLEVEL ([0-9]+)" _p "${_sdlver_hdr}")
                string(REGEX REPLACE ".* ([0-9]+)" "\\1" _pat "${_p}")
                if(_maj AND _min AND _pat)
                    set(ROGUE_SDL2_VERSION "${_maj}.${_min}.${_pat}")
                    message(STATUS "Detected SDL2 version ${ROGUE_SDL2_VERSION}")
                    # Optional version validation: set -DROGUE_MIN_SDL2_VERSION=X.Y.Z to request a minimum
                    if(DEFINED ROGUE_MIN_SDL2_VERSION AND NOT ROGUE_MIN_SDL2_VERSION STREQUAL "")
                        if(ROGUE_SDL2_VERSION VERSION_LESS ROGUE_MIN_SDL2_VERSION)
                            message(WARNING "SDL2 ${ROGUE_SDL2_VERSION} is below requested minimum ${ROGUE_MIN_SDL2_VERSION}. Proceeding (non-fatal).")
                        else()
                            message(STATUS "SDL2 ${ROGUE_SDL2_VERSION} meets minimum ${ROGUE_MIN_SDL2_VERSION}.")
                        endif()
                    endif()
                endif()
            endif()
        endif()
    endif()
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
    # Normalize manual paths for cross-platform safety
    rogue_normalize_path(_SDL2_ROOT_NORM "${ROGUE_SDL2_ROOT}")
    set(_SDL2_INC_HINT "${_SDL2_ROOT_NORM}/include")
    set(_SDL2_LIB_HINT1 "${_SDL2_ROOT_NORM}/lib")
    set(_SDL2_LIB_HINT2 "${_SDL2_ROOT_NORM}/lib/x64")
    # Support both traditional layout (include/SDL.h) and vcpkg layout (include/SDL2/SDL.h)
    find_path(SDL2_INCLUDE_DIR SDL.h HINTS ${_SDL2_INC_HINT} PATH_SUFFIXES SDL2)
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
    elseif(UNIX)
        # pkg-config fallback commonly available on Linux/macOS
        find_package(PkgConfig QUIET)
        if(PKG_CONFIG_FOUND)
            pkg_check_modules(PC_SDL2 QUIET sdl2)
            if(PC_SDL2_FOUND)
                add_library(SDL2::SDL2 INTERFACE IMPORTED)
                set_target_properties(SDL2::SDL2 PROPERTIES
                    INTERFACE_INCLUDE_DIRECTORIES "${PC_SDL2_INCLUDE_DIRS}"
                    INTERFACE_LINK_LIBRARIES "${PC_SDL2_LIBRARIES}"
                )
                message(STATUS "Using SDL2 via pkg-config: includes=${PC_SDL2_INCLUDE_DIRS}")
            endif()
        endif()
    endif()
endif()

# If SDL still not found after manual root, abort with helpful message.
if(ROGUE_ENABLE_SDL AND NOT TARGET SDL2::SDL2)
    message(FATAL_ERROR "SDL2 not found. Provide -DROGUE_SDL2_ROOT=path (containing include/SDL.h and lib/SDL2.lib) or disable with -DROGUE_ENABLE_SDL=OFF")
endif()

# Optional SDL2_image and SDL2_mixer fallbacks via pkg-config on Unix
if(UNIX AND ROGUE_ENABLE_SDL)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        # SDL2_image
        if(ROGUE_ENABLE_SDL_IMAGE AND NOT TARGET SDL2_image::SDL2_image)
            # Try both modern and legacy pkg-config names
            pkg_check_modules(PC_SDL2_IMAGE QUIET SDL2_image sdl2_image)
            if(PC_SDL2_IMAGE_FOUND)
                add_library(SDL2_image::SDL2_image INTERFACE IMPORTED)
                set_target_properties(SDL2_image::SDL2_image PROPERTIES
                    INTERFACE_INCLUDE_DIRECTORIES "${PC_SDL2_IMAGE_INCLUDE_DIRS}"
                    INTERFACE_LINK_LIBRARIES "${PC_SDL2_IMAGE_LIBRARIES}"
                )
                # Ensure downstream conditionals see this as found
                set(SDL2_image_FOUND TRUE CACHE INTERNAL "SDL2_image provided via pkg-config fallback")
                message(STATUS "Using SDL2_image via pkg-config: includes=${PC_SDL2_IMAGE_INCLUDE_DIRS}")
            endif()
        endif()

        # SDL2_mixer
        if(ROGUE_ENABLE_SDL_MIXER AND NOT TARGET SDL2_mixer::SDL2_mixer)
            pkg_check_modules(PC_SDL2_MIXER QUIET SDL2_mixer sdl2_mixer)
            if(PC_SDL2_MIXER_FOUND)
                add_library(SDL2_mixer::SDL2_mixer INTERFACE IMPORTED)
                set_target_properties(SDL2_mixer::SDL2_mixer PROPERTIES
                    INTERFACE_INCLUDE_DIRECTORIES "${PC_SDL2_MIXER_INCLUDE_DIRS}"
                    INTERFACE_LINK_LIBRARIES "${PC_SDL2_MIXER_LIBRARIES}"
                )
                set(SDL2_mixer_FOUND TRUE CACHE INTERNAL "SDL2_mixer provided via pkg-config fallback")
                message(STATUS "Using SDL2_mixer via pkg-config: includes=${PC_SDL2_MIXER_INCLUDE_DIRS}")
            endif()
        endif()
    endif()
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
    src/util/metrics.c
    src/core/app/app_state.c
    src/game/game_loop.c
    src/core/minimap/minimap.c
    src/core/minimap/minimap_loot_pings.c
    # moved to rogue_systems_loot: src/core/loot/loot_tooltip.c
    src/core/player/player_controller.c
    src/core/player/player_debug.c
    src/input/input_events.c
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
    src/world/world_renderer.c
    src/graphics/animation_system.c
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
    src/core/integration/tag_registry.c
    src/util/json_parser.c
    src/util/cfg_parser.c
    src/core/integration/cfg_migration.c
    src/util/formula_eval.c
    src/core/integration/hot_reload.c
    src/core/integration/dependency_manager.c
    src/util/kv_parser.c
    # Asset system (Phase 3 initial slice)
    src/asset/asset_manager.c
    src/asset/path_utils.c
    src/asset/asset_validation.c
    # persistence module split
    src/core/persistence/persistence_io.c
    src/core/persistence/persistence_world.c
    src/core/persistence/persistence_player.c
    src/core/persistence/persistence_autosave.c
    src/core/persistence/save_manager.c
    # new modularized save system files
    src/core/persistence/save_utils.c
    src/core/persistence/save_globals.c
    src/core/persistence/save_paths.c
    src/core/persistence/save_intern.c
    src/core/persistence/save_manager_core.c
    src/core/persistence/save_autosave.c
    src/core/persistence/save_security.c
    src/core/persistence/save_incremental.c
    src/core/persistence/save_replay.c
    src/core/persistence/save_migrations.c
    src/core/persistence/save_components.c
    # skills module split
    src/core/skills/skills_registry.c
    src/core/skills/skills_runtime.c
    src/core/skills/skills_effects.c
    src/core/skills/skills_coeffs.c
    src/core/skills/skills_coeffs_load.c
    src/core/skills/skill_debug.c
    src/core/skills/skills_api_doc.c
    src/core/skills/skills_procs.c
    src/core/skills/skills_validate.c
    src/core/skills/skill_tree.c
    src/core/skills/skill_bar.c
    src/core/skills/skill_talents.c
    src/core/skills/skill_assets.c
    src/core/skills/skill_sprite_loader.c
    src/core/skills/skill_asset_validation.c
    # EffectSpec parser
    src/graphics/effect_spec_parser.c
    src/graphics/effect_spec_load.c
    src/graphics/effect_spec.c
    src/game/buffs.c
    # projectiles module split
    src/core/projectiles/projectiles_spawn.c
    src/core/projectiles/projectiles_update.c
    src/core/projectiles/projectiles_render.c
    src/core/projectiles/projectiles_config.c
    src/game/damage_calc.c
    src/game/collision.c
    src/game/navigation.c
    src/util/asset_config.c
    src/util/asset_placeholder.c
    src/game/platform.c
    src/world/tile_sprite_cache.c
    src/core/world/map_debug.c
    src/platform/platform_sdl.c
    # Platform helpers (portable file picker replacing Win32 common dialog)
    src/platform/file_dialog_portable.c
    src/graphics/renderer.c
    src/graphics/sprite.c
    src/entities/spawn_entities.c
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
    src/world/world_gen_dungeon_kernel.c
    src/world/world_gen_dungeon_taxonomy.c
    src/world/world_gen_dungeon_theme.c
    src/world/world_gen_dungeon_encounters.c
    src/world/world_gen_dungeon_objectives.c
    # Phase 6: puzzle & traversal subsystem (minimal scaffolding)
    src/world/world_gen_dungeon_puzzle.c
    # Phase 7: hazards & traps subsystem (initial helpers)
    src/world/world_gen_dungeon_traps.c
    src/world/world_gen_mutators.c
    src/world/world_gen_dungeon_loot.c
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
    src/world/world_gen_room_templates.c
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
    src/ai/nodes/advanced_nodes_control.c
    src/ai/nodes/advanced_nodes_decorators.c
    src/ai/nodes/advanced_nodes.c
    src/ai/perception/perception.c
    src/ai/pathing/flow_field.c
    src/ai/pathing/local_avoidance.c
    # debug entity APIs for overlay/tests
    src/core/entities/entity_debug.c
    src/game/start_screen.c
    src/game/localization.c
    src/core/player/player_assets.c
    src/util/log.c
    src/core/vegetation/vegetation_defs.c
    src/core/vegetation/vegetation_json.c
    src/core/vegetation/vegetation_generate.c
    src/core/vegetation/vegetation_render.c
    src/core/vegetation/vegetation_collision.c
    src/graphics/scene_drawlist.c
    # moved to rogue_systems_loot: src/core/loot/loot_item_defs.c
    # moved to rogue_systems_loot: src/core/loot/loot_item_defs_convert.c
    # moved to rogue_systems_loot: src/core/loot/loot_rebalance.c
    # moved to rogue_systems_loot: src/core/loot/loot_item_defs_sort.c
    # moved to rogue_systems_loot: src/core/loot/loot_tables.c
    # moved to rogue_systems_loot: src/core/loot/loot_instances.c
    src/core/inventory/inventory.c
    src/core/inventory/inventory_entries.c
    src/core/inventory/inventory_ui.c
    src/core/inventory/inventory_tags.c
    src/core/inventory/inventory_panel.c
    # moved to rogue_systems_loot: src/core/loot/loot_pickup.c
    # moved to rogue_systems_loot: src/core/loot/loot_multiplayer.c
    # moved to rogue_systems_loot: src/core/loot/loot_logging.c
    # moved to rogue_systems_loot: src/core/loot/loot_affixes.c
    # moved to rogue_systems_loot: src/core/loot/loot_filter.c
    src/util/path_utils.c
    src/util/asset_classification.c
    # moved to rogue_systems_loot: src/core/loot/loot_rarity.c
    # moved to rogue_systems_loot: src/core/loot/loot_perf.c
    # moved to rogue_systems_loot: src/core/loot/loot_analytics.c
    # moved to rogue_systems_loot: src/core/loot/loot_vfx.c
    # moved to rogue_systems_loot: src/core/loot/loot_stats.c
    # moved to rogue_systems_loot: src/core/loot/loot_dynamic_weights.c
    # moved to rogue_systems_loot: src/core/loot/loot_console.c
    # moved to rogue_systems_loot: src/core/loot/loot_rarity_adv.c
    # moved to rogue_systems_loot: src/core/loot/loot_generation.c
    # moved to rogue_systems_loot: src/core/loot/loot_generation_affix.c
    # moved to rogue_systems_loot: src/core/loot/loot_drop_rates.c
    # moved to rogue_systems_loot: src/core/loot/loot_adaptive.c
    # moved to rogue_systems_loot: src/core/loot/loot_commands.c
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
    src/core/vendor/vendor_run_summary_consumer.c
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
    src/util/loadout_optimizer.c
    src/game/durability.c
    src/core/vendor/vendor_ui.c
    src/core/skills/skill_graph_runtime.c
    src/core/skills/skill_graph_assignment.c
    src/core/skills/skill_maze.c
    src/game/stat_cache.c
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
    src/game/dialogue.c
    src/game/dialogue_style.c
    src/game/dialogue_avatars.c
    src/game/dialogue_locale.c
    src/game/dialogue_util.c
    # UI Phase 1 scaffolding (refactored modular sources)
    src/ui/core/ui_context_core.c
    src/ui/core/ui_context_interaction.c
    src/ui/core/ui_context_navigation.c
    src/ui/core/ui_context_animation.c
    src/ui/core/ui_context_perf.c
    src/ui/core/ui_context_inventory.c
    src/ui/core/ui_context_misc.c
    src/ui/core/ui_skillgraph.c
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
    # moved to rogue_systems_loot: src/core/loot/loot_security.c
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
    src/game/skill_area_debug_overlay.c
    src/core/audio_vfx/audiovfx_debug.c
    src/core/enemy/enemy_difficulty.c
    src/core/enemy/enemy_difficulty_scaling.c
    src/core/enemy/enemy_adaptive.c
    src/core/enemy/enemy_modifiers.c
    src/core/enemy/encounter_composer.c
    # Audio & VFX moved into separate module target (rogue_audio_vfx)
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
    # Phase 13.1 validation wiring (default checks registration)
    src/core/integration/validation_wiring.c
    # Phase 5.6 deadlock detection & prevention
    src/core/integration/deadlock_manager.c
)

# -----------------------------
# Phase 5: Asset validation CLI tool
# -----------------------------
add_executable(asset_validate
    src/asset/asset_validate_cli.c)
target_link_libraries(asset_validate PRIVATE rogue_core)
target_include_directories(asset_validate PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Phase 7: asset_tool multi-command CLI
add_executable(asset_tool
    src/asset/asset_tool_cli.c)
target_link_libraries(asset_tool PRIVATE rogue_core)
target_include_directories(asset_tool PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(TARGET SDL2::SDL2)
    target_link_libraries(asset_tool PRIVATE SDL2::SDL2)
endif()

if (EXISTS ${CMAKE_SOURCE_DIR}/assets)
    if (WIN32)
        # Ensure dependent runtime DLLs (e.g. SDL2[d].dll, SDL2_mixer[d].dll) are discoverable.
        set(ROGUE_VCPKG_BIN_RELEASE "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows/bin")
        set(ROGUE_VCPKG_BIN_DEBUG   "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows/debug/bin")
        # Use full path to the executable so the loader searches its directory first (contains copied/matching DLLs).
        add_custom_target(asset_manifest ALL
            COMMAND ${CMAKE_COMMAND} -E env "PATH=${ROGUE_VCPKG_BIN_DEBUG};${ROGUE_VCPKG_BIN_RELEASE};$ENV{PATH}" $<TARGET_FILE:asset_validate> --manifest ${CMAKE_BINARY_DIR}/asset_manifest.txt --optimize
            DEPENDS asset_validate
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Generating asset manifest (Phase 5)")
    else()
        add_custom_target(asset_manifest ALL
            COMMAND $<TARGET_FILE:asset_validate> --manifest ${CMAKE_BINARY_DIR}/asset_manifest.txt --optimize
            DEPENDS asset_validate
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Generating asset manifest (Phase 5)")
    endif()
else()
    message(WARNING "assets directory missing - skipping asset_manifest target generation")
endif()

install(FILES ${CMAKE_BINARY_DIR}/asset_manifest.txt DESTINATION meta OPTIONAL)

# Ensure rogue_add_test helper is defined early for unified per-test environment
if(NOT COMMAND rogue_add_test)
function(rogue_add_test)
    set(options)
    set(oneValueArgs NAME)
    set(multiValueArgs COMMAND)
    cmake_parse_arguments(RAT "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    # Allow positional form: rogue_add_test(test_name COMMAND exe ...)
    if(NOT RAT_NAME AND RAT_UNPARSED_ARGUMENTS)
        list(GET RAT_UNPARSED_ARGUMENTS 0 RAT_NAME)
    endif()
    if(NOT RAT_NAME OR NOT RAT_COMMAND)
        message(FATAL_ERROR "rogue_add_test requires NAME <name> COMMAND <cmd>... (or positional <name> COMMAND ...)")
    endif()
    add_test(NAME ${RAT_NAME} COMMAND ${RAT_COMMAND})
    # Track wrapped tests for enforcement later
    list(APPEND ROGUE_WRAPPED_TESTS ${RAT_NAME})
    set(ROGUE_WRAPPED_TESTS "${ROGUE_WRAPPED_TESTS}" CACHE INTERNAL "Tests registered via rogue_add_test")
    if(WIN32 AND ROGUE_ENABLE_SDL)
        if(NOT DEFINED ROGUE_CACHED_TEST_PATH_FRAGMENT)
            set(_rogue_runtime_paths "${CMAKE_BINARY_DIR}/runtimes")
            if(EXISTS "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows/bin")
                list(APPEND _rogue_runtime_paths "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows/bin")
            endif()
            if(EXISTS "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows/debug/bin")
                list(APPEND _rogue_runtime_paths "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows/debug/bin")
            endif()
            foreach(_cfg Debug Release RelWithDebInfo MinSizeRel)
                if(EXISTS "${CMAKE_BINARY_DIR}/tests/${_cfg}")
                    list(APPEND _rogue_runtime_paths "${CMAKE_BINARY_DIR}/tests/${_cfg}")
                endif()
            endforeach()
            list(REMOVE_DUPLICATES _rogue_runtime_paths)
            string(JOIN ";" ROGUE_CACHED_TEST_PATH_FRAGMENT ${_rogue_runtime_paths})
            set(ROGUE_CACHED_TEST_PATH_FRAGMENT "${ROGUE_CACHED_TEST_PATH_FRAGMENT}" CACHE INTERNAL "Unified test PATH fragment")
            message(STATUS "[rogue_add_test] Computed unified test PATH fragment: ${ROGUE_CACHED_TEST_PATH_FRAGMENT}")
        endif()
        set_tests_properties(${RAT_NAME} PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;PATH=${ROGUE_CACHED_TEST_PATH_FRAGMENT};$ENV{PATH}")
    endif()
endfunction()
endif()

rogue_add_test(NAME asset_validation_cli
    COMMAND asset_validate --manifest ${CMAKE_BINARY_DIR}/asset_manifest_ctest.txt --optimize)
# Run from project root so the CLI finds the assets/ directory (it expects CWD with assets/)
set_tests_properties(asset_validation_cli PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

rogue_add_test(NAME asset_tool_stats_cli COMMAND asset_tool stats)

# MSVC: Avoid parallel PDB write contention (fatal error C1041) when building many translation units in parallel.
if (MSVC)
    # Mitigate parallel PDB (C1041) by enabling safe writes and embedding debug info in objects.
    target_compile_options(rogue_core PRIVATE /FS /Z7)
    # Also replace default /Zi with /Z7 for all Debug builds to avoid a single monolithic PDB hot spot
    foreach(lang C CXX)
        if (CMAKE_${lang}_FLAGS_DEBUG)
            string(REPLACE "/Zi" "/Z7" CMAKE_${lang}_FLAGS_DEBUG "${CMAKE_${lang}_FLAGS_DEBUG}")
            # Re-cache the modified flags (previous line had a typo that cleared them)
            set(CMAKE_${lang}_FLAGS_DEBUG "${CMAKE_${lang}_FLAGS_DEBUG}" CACHE STRING "MSVC ${lang} flags" FORCE)
        endif()
    endforeach()
endif()

# Make overlay feature macro visible to all TUs so headers don't fallback to stubs
target_compile_definitions(rogue_core PUBLIC ROGUE_ENABLE_DEBUG_OVERLAY=$<IF:$<BOOL:${ROGUE_ENABLE_DEBUG_OVERLAY}>,1,0>)
target_compile_definitions(rogue_core PUBLIC ROGUE_ENABLE_JSON_CONTENT=$<IF:$<BOOL:${ROGUE_ENABLE_JSON_CONTENT}>,1,0>)

# ---- Debug Overlay and Content JSON object libraries ----
# Overlay core and widgets grouped for cleaner build graph; compiled only when enabled
add_library(rogue_debug_overlay OBJECT
    src/debug_overlay/overlay_core.c
    src/debug_overlay/overlay_input.c
    src/debug_overlay/overlay_prefs.c
    src/debug_overlay/overlay_theme.c
    src/debug_overlay/overlay_toast.c
    src/debug_overlay/overlay_tooltip.c
    src/debug_overlay/overlay_commands.c
    src/debug_overlay/overlay_search.c
    src/debug_overlay/widgets/overlay_widgets.c
    # Split widget implementation units
    src/debug_overlay/widgets/overlay_widgets_core.c
    src/debug_overlay/overlay_icon.c
    # Modularized control widgets (refactored from overlay_widgets_controls.c)
    src/debug_overlay/widgets/controls/overlay_button.c
    src/debug_overlay/widgets/controls/overlay_checkbox.c
    src/debug_overlay/widgets/controls/overlay_sliders.c
    src/debug_overlay/widgets/controls/overlay_text_input.c
    src/debug_overlay/widgets/controls/overlay_combo_tree_color.c
    src/debug_overlay/widgets/controls/overlay_columns_splitter.c
    src/debug_overlay/widgets/controls/overlay_multiselect_list_vec.c
    src/debug_overlay/widgets/controls/overlay_complex_editors.c
    src/debug_overlay/widgets/overlay_widgets_table.c
    # Aggregated panel registrar and per-panel implementations
    src/debug_overlay/overlay_panels.c
    src/debug_overlay/panel_system/panels_system.c
    src/debug_overlay/panel_system/panels_asset_metrics.c
    # Phase 7: Asset Browser panel (added after metrics panel)
    # Asset Browser refactor modules (state + directory logic extracted from monolith)
    src/debug_overlay/asset_browser/asset_browser_state.c
    src/debug_overlay/asset_browser/asset_browser_dir.c
    src/debug_overlay/panel_system/panels_asset_browser.c
    src/debug_overlay/panel_player/panels_player.c
    src/debug_overlay/panel_skills/panels_skills.c
    # Skills panel refactor modules
    src/debug_overlay/panel_skills/shared/panel_skills_shared.c
    src/debug_overlay/panel_skills/overview/panel_skills_overview.c
    # Effects tab modularized (was single large panel_skills_effects.c)
    src/debug_overlay/panel_skills/effects/panel_skills_effects.c
    src/debug_overlay/panel_skills/effects/effects_tree_editor.c
    src/debug_overlay/panel_skills/effects/effects_validation.c
    src/debug_overlay/panel_skills/effects/effects_palette.c
    src/debug_overlay/panel_skills/effects/effects_node_graph.c
    src/debug_overlay/panel_skills/effects/effects_node_list.c
    src/debug_overlay/panel_skills/visuals/panel_skills_visuals.c
    src/debug_overlay/panel_skills/visuals/visuals_internal.h
    src/debug_overlay/panel_skills/visuals/visuals_import_wizard.c
    src/debug_overlay/panel_skills/visuals/visuals_browser.c
    src/debug_overlay/panel_skills/visuals/visuals_asset_picker.c
    src/debug_overlay/panel_skills/visuals/visuals_core_fields.c
    src/debug_overlay/panel_skills/visuals/visuals_prototypes.c
    src/debug_overlay/panel_skills/visuals/visuals_sheet_editor.c
    src/debug_overlay/panel_skills/visuals/visuals_preview.c
    src/debug_overlay/panel_skills/visuals/visuals_dependency_viewer.c
    src/debug_overlay/panel_skills/visuals/visuals_validation_panel.c
    # Skills panel legacy audio & testing tabs (added after visuals refactor to restore linkage)
    src/debug_overlay/panel_skills/audio/panel_skills_audio.c
    src/debug_overlay/panel_skills/testing/panel_skills_testing.c
    src/debug_overlay/panel_entities/panels_entities.c
    src/debug_overlay/panel_map/panels_map.c
    src/debug_overlay/panel_audio_vfx/panels_audiovfx.c
    src/debug_overlay/panel_dialogue/panels_dialogue.c
    src/debug_overlay/panel_shortcuts/panels_shortcuts.c
    src/debug_overlay/panel_items/panels_items.c
    src/debug_overlay/panel_validation/panels_validation.c
    src/debug_overlay/panel_content_graph/panels_content_graph.c
    src/debug_overlay/panel_content_graph/overlay_cg_helpers.c
    src/debug_overlay/panel_content_graph/overlay_cg_snapshot_diff.c
    src/debug_overlay/panels_selector.c
)
target_compile_definitions(rogue_debug_overlay PUBLIC ROGUE_ENABLE_DEBUG_OVERLAY=$<IF:$<BOOL:${ROGUE_ENABLE_DEBUG_OVERLAY}>,1,0>)
target_include_directories(rogue_debug_overlay PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(ROGUE_ENABLE_SDL)
    # Ensure overlay compiles with SDL types/APIs available and same struct layout guards
    target_link_libraries(rogue_debug_overlay PUBLIC SDL2::SDL2)
    target_compile_definitions(rogue_debug_overlay PUBLIC ROGUE_HAVE_SDL=1)
endif()

# Content JSON helpers grouped here; always compile when JSON content is enabled
add_library(rogue_content_json OBJECT
    src/content/json_io.c
    src/content/json_envelope.c
    src/content/schema_items.c
    src/content/schema_skills.c
    src/content/schema_entities.c
    src/content/schema_tilesets.c
    src/content/schema_sprites.c
    src/content/schema_audio.c
    src/content/schema_levels.c
    src/content/schema_theme.c
)
target_include_directories(rogue_content_json PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(rogue_content_json PUBLIC ROGUE_ENABLE_JSON_CONTENT=$<IF:$<BOOL:${ROGUE_ENABLE_JSON_CONTENT}>,1,0>)
if(ROGUE_ENABLE_SDL)
    # Ensure consistent struct layouts across TUs that include SDL-conditional types (e.g., RogueTexture)
    target_compile_definitions(rogue_content_json PUBLIC ROGUE_HAVE_SDL=1)
    # Propagate SDL include dirs/defines so SDL.h is available to this OBJECT lib during compilation
    target_link_libraries(rogue_content_json PUBLIC SDL2::SDL2)
endif()

# Vendored cJSON stub/library (replace with real cJSON later)
add_library(rogue_thirdparty_cjson STATIC
    third_party/cjson/cJSON.c
)
target_include_directories(rogue_thirdparty_cjson PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cjson)
target_compile_definitions(rogue_thirdparty_cjson PUBLIC ROGUE_CJSON_STUB=1)

# Link overlay objects conditionally into rogue_core
target_sources(rogue_core PRIVATE
    $<$<BOOL:${ROGUE_ENABLE_DEBUG_OVERLAY}>:$<TARGET_OBJECTS:rogue_debug_overlay>>
)

# Ensure the core library is explicitly rebuilt after overlay object list changes.
# (Generator expressions with TARGET_OBJECTS generally handle this, but an explicit
# dependency avoids stale archives when sources are added incrementally.)
add_dependencies(rogue_core rogue_debug_overlay)

# Link content JSON objects and cJSON when enabled
target_sources(rogue_core PRIVATE
    $<$<BOOL:${ROGUE_ENABLE_JSON_CONTENT}>:$<TARGET_OBJECTS:rogue_content_json>>
)
target_link_libraries(rogue_core PUBLIC $<$<BOOL:${ROGUE_ENABLE_JSON_CONTENT}>:rogue_thirdparty_cjson>)

# Suppress Windows error/abort popups in tests and tools; route to stderr instead.
if(WIN32)
    target_sources(rogue_core PRIVATE src/util/win_suppress_error_dialogs.c)
    if(ROGUE_SUPPRESS_WINDOWS_ERROR_DIALOGS)
        target_compile_definitions(rogue_core PUBLIC ROGUE_SUPPRESS_WINDOWS_ERROR_DIALOGS=1)
    endif()
endif()

target_include_directories(rogue_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/world
)
target_compile_definitions(rogue_core PUBLIC
    ROGUE_BUILD_GIT_HASH="${ROGUE_GIT_HASH}"
    ROGUE_BUILD_GIT_BRANCH="${ROGUE_GIT_BRANCH}"
    ROGUE_BUILD_TIME="${ROGUE_BUILD_TIME}"
    ROGUE_ENABLE_DEBUG_OVERLAY=$<IF:$<BOOL:${ROGUE_ENABLE_DEBUG_OVERLAY}>,1,0>
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

# Phase 1 — First modular STATIC library: audio_vfx
add_library(rogue_audio_vfx STATIC
    src/audio_vfx/fx_bus.c
    src/audio_vfx/fx_audio.c
    src/audio_vfx/fx_vfx.c
    src/audio_vfx/fx_rng.c
    src/audio_vfx/vfx_config.c
)
target_include_directories(rogue_audio_vfx PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_options(rogue_audio_vfx PRIVATE ${rogue_warnings})
if(ROGUE_WARNINGS_AS_ERRORS)
    target_compile_options(rogue_audio_vfx PRIVATE $<$<C_COMPILER_ID:MSVC>:/WX> $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Werror>)
endif()
if(ROGUE_ENABLE_SDL)
    # Inherit SDL usage requirements for any optional includes in this module
    target_link_libraries(rogue_audio_vfx PRIVATE SDL2::SDL2)
    if(SDL2_mixer_FOUND)
        target_link_libraries(rogue_audio_vfx PRIVATE SDL2_mixer::SDL2_mixer)
    endif()
endif()

# Link core against the new module so existing executables and tests remain unchanged
target_link_libraries(rogue_core PUBLIC rogue_audio_vfx)

# Phase 1 — Second modular STATIC library: systems_loot
add_library(rogue_systems_loot STATIC
    src/core/loot/loot_tooltip.c
    src/core/loot/loot_item_defs.c
    src/core/loot/loot_item_defs_convert.c
    src/core/loot/loot_rebalance.c
    src/core/loot/loot_item_defs_sort.c
    src/core/loot/item_debug.c
    src/core/loot/loot_tables.c
    src/core/loot/loot_instances.c
    src/core/loot/loot_pickup.c
    src/core/loot/loot_multiplayer.c
    src/core/loot/loot_logging.c
    src/core/loot/loot_affixes.c
    src/core/loot/loot_filter.c
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
    src/core/loot/loot_security.c
)
target_include_directories(rogue_systems_loot PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_options(rogue_systems_loot PRIVATE ${rogue_warnings})
if(ROGUE_WARNINGS_AS_ERRORS)
    target_compile_options(rogue_systems_loot PRIVATE $<$<C_COMPILER_ID:MSVC>:/WX> $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Werror>)
endif()
if(ROGUE_ENABLE_SDL)
    # Ensure the same SDL feature macros are defined so headers (like app_state.h)
    # have identical layouts across all modules that include them.
    target_compile_definitions(rogue_systems_loot PUBLIC ROGUE_HAVE_SDL=1)
    if(SDL2_mixer_FOUND)
        target_compile_definitions(rogue_systems_loot PUBLIC ROGUE_HAVE_SDL_MIXER=1)
    endif()
    target_link_libraries(rogue_systems_loot PRIVATE SDL2::SDL2)
endif()

# Link core against loot module to preserve existing linkage surface
target_link_libraries(rogue_core PUBLIC rogue_systems_loot)

# Ensure all objects in rogue_systems_loot are retained (static registries, init tables)
# MSVC uses /WHOLEARCHIVE; GNU/Clang use --whole-archive around the specific archive
if(MSVC)
    target_link_options(rogue_core PRIVATE /WHOLEARCHIVE:rogue_systems_loot)
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    # Use the full path to the archive between the flags
    target_link_options(rogue_core PRIVATE -Wl,--whole-archive $<TARGET_FILE:rogue_systems_loot> -Wl,--no-whole-archive)
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

add_executable(test_skills_phase2_api tests/unit/test_skills_phase2_api.c)
target_link_libraries(test_skills_phase2_api PRIVATE rogue_core)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_skills_phase2_api PRIVATE SDL2::SDL2)
endif()
rogue_add_test(NAME test_skills_phase2_api COMMAND test_skills_phase2_api)

# Phase 1.3 Enhanced Execution Pathways test
add_executable(test_skills_phase1_3_execution_pathways tests/unit/test_skills_phase1_3_execution_pathways.c)
target_link_libraries(test_skills_phase1_3_execution_pathways PRIVATE rogue_core)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_skills_phase1_3_execution_pathways PRIVATE SDL2::SDL2)
endif()
rogue_add_test(NAME test_skills_phase1_3_execution_pathways COMMAND test_skills_phase1_3_execution_pathways)

add_executable(test_skills_rotation_sim tests/unit/test_skills_rotation_sim.c)
target_link_libraries(test_skills_rotation_sim PRIVATE rogue_core)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_skills_rotation_sim PRIVATE SDL2::SDL2)
endif()
rogue_add_test(NAME test_skills_rotation_sim COMMAND test_skills_rotation_sim)

add_executable(test_tag_registry_validate tests/unit/test_tag_registry_validate.c)

# Debug overlay core unit test
add_executable(test_overlay_core tests/unit/test_overlay_core.c)
target_link_libraries(test_overlay_core PRIVATE rogue_core)
rogue_add_test(NAME test_overlay_core COMMAND test_overlay_core)

# Debug overlay widgets unit test (headless-safe)
add_executable(test_overlay_widgets tests/unit/test_overlay_widgets.c)
target_link_libraries(test_overlay_widgets PRIVATE rogue_core)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_overlay_widgets PRIVATE SDL2::SDL2)
endif()
rogue_add_test(NAME test_overlay_widgets COMMAND test_overlay_widgets)
add_executable(test_overlay_widgets_advanced tests/unit/test_overlay_widgets_advanced.c)
rogue_add_test(NAME test_overlay_widgets_advanced COMMAND test_overlay_widgets_advanced)
add_executable(test_overlay_multiselect_list tests/unit/test_overlay_multiselect_list.c)
target_link_libraries(test_overlay_multiselect_list PRIVATE rogue_core)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_overlay_multiselect_list PRIVATE SDL2::SDL2)
endif()
rogue_add_test(NAME test_overlay_multiselect_list COMMAND test_overlay_multiselect_list)
add_executable(test_overlay_table_widget tests/unit/test_overlay_table_widget.c)
target_link_libraries(test_overlay_table_widget PRIVATE rogue_core)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_overlay_table_widget PRIVATE SDL2::SDL2)
endif()
rogue_add_test(NAME test_overlay_table_widget COMMAND test_overlay_table_widget)
# Debug overlay table perf smoke test (headless-friendly, prints avg ms/frame)
add_executable(test_overlay_table_perf_smoke tests/unit/test_overlay_table_perf_smoke.c)
target_link_libraries(test_overlay_table_perf_smoke PRIVATE rogue_core)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_overlay_table_perf_smoke PRIVATE SDL2::SDL2)
endif()
rogue_add_test(NAME test_overlay_table_perf_smoke COMMAND test_overlay_table_perf_smoke)
add_executable(test_skills_phase10_coeffs_loader tests/unit/test_skills_phase10_coeffs_loader.c)
target_link_libraries(test_tag_registry_validate PRIVATE rogue_core)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_tag_registry_validate PRIVATE SDL2::SDL2)
endif()
rogue_add_test(NAME test_tag_registry_validate COMMAND test_tag_registry_validate)

# JSON IO foundational tests
add_executable(test_json_io tests/test_json_io.c)
target_link_libraries(test_json_io PRIVATE rogue_core)
rogue_add_test(NAME test_json_io COMMAND test_json_io)

# JSON envelope tests
add_executable(test_json_envelope tests/test_json_envelope.c)
target_link_libraries(test_json_envelope PRIVATE rogue_core)
rogue_add_test(NAME test_json_envelope COMMAND test_json_envelope)

# Entities schema validation test
add_executable(test_entity_schema tests/unit/test_entity_schema.c)
target_link_libraries(test_entity_schema PRIVATE rogue_core)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_entity_schema PRIVATE SDL2::SDL2)
endif()
rogue_add_test(NAME test_entity_schema COMMAND test_entity_schema)

# Tilesets schema validation test
add_executable(test_tilesets_schema tests/unit/test_tilesets_schema.c)
target_link_libraries(test_tilesets_schema PRIVATE rogue_core)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_tilesets_schema PRIVATE SDL2::SDL2)
endif()
rogue_add_test(NAME test_tilesets_schema COMMAND test_tilesets_schema)

# Items schema validation test
add_executable(test_items_schema tests/unit/test_items_schema.c)
target_link_libraries(test_items_schema PRIVATE rogue_core)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_items_schema PRIVATE SDL2::SDL2)
endif()
rogue_add_test(NAME test_items_schema COMMAND test_items_schema)

# Sprite + Audio schema validation test (Phase 2)
add_executable(test_sprite_audio_schema tests/unit/test_sprite_audio_schema.c)
target_link_libraries(test_sprite_audio_schema PRIVATE rogue_core)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_sprite_audio_schema PRIVATE SDL2::SDL2)
endif()
rogue_add_test(NAME test_sprite_audio_schema COMMAND test_sprite_audio_schema)
add_executable(test_asset_dep_rejects tests/unit/test_asset_dep_rejects.c)
target_link_libraries(test_asset_dep_rejects PRIVATE rogue_core)
## Prevent SDL from redefining main to SDL_main on Windows for this console test
target_compile_definitions(test_asset_dep_rejects PRIVATE SDL_MAIN_HANDLED=1)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_asset_dep_rejects PRIVATE SDL2::SDL2)
endif()
rogue_add_test(NAME test_asset_dep_rejects COMMAND test_asset_dep_rejects)

# Asset classification heuristics test
add_executable(test_asset_classification tests/unit/test_asset_classification.c)
target_link_libraries(test_asset_classification PRIVATE rogue_core)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_asset_classification PRIVATE SDL2::SDL2)
endif()
rogue_add_test(NAME test_asset_classification COMMAND test_asset_classification)

# Asset manager basic test (Phase 3)
add_executable(test_asset_manager_basic tests/unit/test_asset_manager_basic.c)
target_link_libraries(test_asset_manager_basic PRIVATE rogue_core)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_asset_manager_basic PRIVATE SDL2::SDL2)
endif()
rogue_add_test(NAME test_asset_manager_basic COMMAND test_asset_manager_basic)
set_tests_properties(test_asset_manager_basic PROPERTIES LABELS "asset")

# Asset manager streaming + metrics test (Phase 6)
add_executable(test_asset_manager_phase6_streaming tests/unit/test_asset_manager_phase6_streaming.c)
target_link_libraries(test_asset_manager_phase6_streaming PRIVATE rogue_core)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_asset_manager_phase6_streaming PRIVATE SDL2::SDL2)
endif()
rogue_add_test(NAME test_asset_manager_phase6_streaming COMMAND test_asset_manager_phase6_streaming)
set_tests_properties(test_asset_manager_phase6_streaming PROPERTIES LABELS "asset;perf")

## (rogue_add_test already defined earlier)

# Phase 6: compressed texture preference test (headless safe)
add_executable(test_asset_manager_phase6_compressed tests/unit/test_asset_manager_phase6_compressed.c)
target_link_libraries(test_asset_manager_phase6_compressed PRIVATE rogue_core)
rogue_add_test(NAME test_asset_manager_phase6_compressed COMMAND test_asset_manager_phase6_compressed)
set_tests_properties(test_asset_manager_phase6_compressed PROPERTIES LABELS "asset")

# Phase 6: atlas builder UV integrity (expected headless failure path coverage)
add_executable(test_asset_manager_phase6_atlas tests/unit/test_asset_manager_phase6_atlas.c)
target_link_libraries(test_asset_manager_phase6_atlas PRIVATE rogue_core)
rogue_add_test(NAME test_asset_manager_phase6_atlas COMMAND test_asset_manager_phase6_atlas)
set_tests_properties(test_asset_manager_phase6_atlas PROPERTIES LABELS "asset")

# Phase 3 (remaining tasks): headless resize/export negative-path smoke test
add_executable(test_asset_manager_resize_export tests/unit/test_asset_manager_resize_export.c)
target_link_libraries(test_asset_manager_resize_export PRIVATE rogue_core)
rogue_add_test(NAME test_asset_manager_resize_export COMMAND test_asset_manager_resize_export)
set_tests_properties(test_asset_manager_resize_export PROPERTIES LABELS "asset")

# Phase 3 (remaining tasks): positive-path resize/export with renderer (BMP + conditional PNG)
add_executable(test_asset_manager_resize_export_positive tests/unit/test_asset_manager_resize_export_positive.c)
target_link_libraries(test_asset_manager_resize_export_positive PRIVATE rogue_core)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_asset_manager_resize_export_positive PRIVATE SDL2::SDL2)
endif()
rogue_add_test(NAME test_asset_manager_resize_export_positive COMMAND test_asset_manager_resize_export_positive)
set_tests_properties(test_asset_manager_resize_export_positive PROPERTIES LABELS "asset")

# Phase 7 usage analytics test
add_executable(test_asset_phase7_usage_stats tests/unit/test_asset_phase7_usage_stats.c)
target_link_libraries(test_asset_phase7_usage_stats PRIVATE rogue_core)
rogue_add_test(NAME test_asset_phase7_usage_stats COMMAND test_asset_phase7_usage_stats)
set_tests_properties(test_asset_phase7_usage_stats PROPERTIES LABELS "asset")

# Phase 7: asset tool CLI smoke test (invokes asset_tool_main stats)
add_executable(test_asset_phase7_tool_cli
    tests/unit/test_asset_phase7_tool_cli.c
    src/asset/asset_tool_cli.c
)
target_link_libraries(test_asset_phase7_tool_cli PRIVATE rogue_core)
# Suppress standalone main() inside asset_tool_cli.c when this test links that TU directly
# (Future-proof: if asset_tool_main implementation is refactored into a shared object)
target_compile_definitions(test_asset_phase7_tool_cli PRIVATE ASSET_TOOL_NO_STANDALONE)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_asset_phase7_tool_cli PRIVATE SDL2::SDL2)
endif()
rogue_add_test(NAME test_asset_phase7_tool_cli COMMAND test_asset_phase7_tool_cli)
set_tests_properties(test_asset_phase7_tool_cli PROPERTIES LABELS "asset")

# Hot-reload polling smoke test
add_executable(test_asset_manager_reload tests/unit/test_asset_manager_reload.c)
target_link_libraries(test_asset_manager_reload PRIVATE rogue_core)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_asset_manager_reload PRIVATE SDL2::SDL2)
endif()
add_test(NAME test_asset_manager_reload COMMAND test_asset_manager_reload)
set_tests_properties(test_asset_manager_reload PROPERTIES LABELS "asset")

# Placeholder asset enforcement test
add_executable(test_asset_placeholder_enforcement tests/unit/test_asset_placeholder_enforcement.c)
target_link_libraries(test_asset_placeholder_enforcement PRIVATE rogue_core)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_asset_placeholder_enforcement PRIVATE SDL2::SDL2)
endif()
add_test(NAME test_asset_placeholder_enforcement COMMAND test_asset_placeholder_enforcement)

add_executable(test_level_schema tests/unit/test_level_schema.c)
target_link_libraries(test_level_schema PRIVATE rogue_core)
if(ROGUE_ENABLE_SDL)
    target_link_libraries(test_level_schema PRIVATE SDL2::SDL2)
endif()
add_test(NAME test_level_schema COMMAND test_level_schema)

# UI Theme JSON schema validation test (Asset Structure Phase 2)
add_executable(test_ui_theme_schema tests/unit/test_ui_theme_schema.c)

# Basic schema versioning/migration scaffold test
add_executable(test_schema_versioning_basic tests/unit/test_schema_versioning_basic.c)
target_link_libraries(test_ui_theme_schema PRIVATE rogue_core)
target_link_libraries(test_schema_versioning_basic PRIVATE rogue_core)
target_link_libraries(test_schema_versioning_basic PRIVATE rogue_thirdparty_cjson)
if(ROGUE_ENABLE_JSON_CONTENT)
    target_sources(test_schema_versioning_basic PRIVATE $<TARGET_OBJECTS:rogue_content_json>)
endif()
add_dependencies(test_schema_versioning_basic rogue_core)
# Ensure the JSON content code path is compiled even if global toggle is off so migrations resolve symbols
target_compile_definitions(test_schema_versioning_basic PRIVATE ROGUE_ENABLE_JSON_CONTENT=1)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_ui_theme_schema PRIVATE SDL2::SDL2)
    target_link_libraries(test_schema_versioning_basic PRIVATE SDL2::SDL2)
endif()
add_test(NAME test_ui_theme_schema COMMAND test_ui_theme_schema)
add_test(NAME test_schema_versioning_basic COMMAND test_schema_versioning_basic)

# Skills schema roundtrip (enveloped JSON) test
add_executable(test_skills_roundtrip_schema tests/unit/test_skills_roundtrip_schema.c)
target_link_libraries(test_skills_roundtrip_schema PRIVATE rogue_core)
## Prevent SDL from redefining main to SDL_main on Windows for this console test
target_compile_definitions(test_skills_roundtrip_schema PRIVATE SDL_MAIN_HANDLED=1)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_skills_roundtrip_schema PRIVATE SDL2::SDL2)
endif()
add_test(NAME test_skills_roundtrip_schema COMMAND test_skills_roundtrip_schema)

# Skills live-reload (auto-reload) unit test
add_executable(test_skills_live_reload tests/unit/test_skills_live_reload.c)
target_link_libraries(test_skills_live_reload PRIVATE rogue_core)
target_compile_definitions(test_skills_live_reload PRIVATE SDL_MAIN_HANDLED=1)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_skills_live_reload PRIVATE SDL2::SDL2)
endif()
add_test(NAME test_skills_live_reload COMMAND test_skills_live_reload)

# Map editing debug API test
add_executable(test_map_debug tests/unit/test_map_debug.c)
target_link_libraries(test_map_debug PRIVATE rogue_core)
# Prevent SDL from redefining main to SDL_main for this unit test
target_compile_definitions(test_map_debug PRIVATE SDL_MAIN_HANDLED=1)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_map_debug PRIVATE SDL2::SDL2)
endif()
add_test(NAME test_map_debug COMMAND test_map_debug)

add_executable(test_skills_ordering_invariance tests/unit/test_skills_ordering_invariance.c)
target_link_libraries(test_skills_ordering_invariance PRIVATE rogue_core)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_skills_ordering_invariance PRIVATE SDL2::SDL2)
endif()
rogue_add_test(NAME test_skills_ordering_invariance COMMAND test_skills_ordering_invariance)

# Phase 10.1 skills coefficients loader test
target_link_libraries(test_skills_phase10_coeffs_loader PRIVATE rogue_core)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_skills_phase10_coeffs_loader PRIVATE SDL2::SDL2)
endif()
rogue_add_test(NAME test_skills_phase10_coeffs_loader COMMAND test_skills_phase10_coeffs_loader)

# Phase 10.2 EffectSpec JSON loader test
add_executable(test_effectspec_json_loader tests/unit/test_effectspec_json_loader.c)
target_link_libraries(test_effectspec_json_loader PRIVATE rogue_core)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_effectspec_json_loader PRIVATE SDL2::SDL2)
endif()
rogue_add_test(NAME test_effectspec_json_loader COMMAND test_effectspec_json_loader)

# Phase 1.2: EffectSpec SPAWN_PROJECTILE unit test
add_executable(test_effectspec_spawn_projectile tests/unit/test_effectspec_spawn_projectile.c)
target_link_libraries(test_effectspec_spawn_projectile PRIVATE rogue_core)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_effectspec_spawn_projectile PRIVATE SDL2::SDL2)
endif()
rogue_add_test(NAME test_effectspec_spawn_projectile COMMAND test_effectspec_spawn_projectile)
# Phase 1.2: EffectSpec timing and conditionals unit test
add_executable(test_effectspec_timing_and_conditionals tests/unit/test_effectspec_timing_and_conditionals.c)
target_link_libraries(test_effectspec_timing_and_conditionals PRIVATE rogue_core)
if (TARGET SDL2::SDL2)
    target_link_libraries(test_effectspec_timing_and_conditionals PRIVATE SDL2::SDL2)
endif()
rogue_add_test(NAME test_effectspec_timing_and_conditionals COMMAND test_effectspec_timing_and_conditionals)


# Phase 10.3: skills validator unit test
add_executable(test_skills_phase10_3_validator tests/unit/test_skills_phase10_3_validator.c)
target_link_libraries(test_skills_phase10_3_validator PRIVATE rogue_core)
if(SDL2_FOUND)
    target_link_libraries(test_skills_phase10_3_validator PRIVATE SDL2::SDL2)
endif()

rogue_add_test(NAME test_skills_phase10_3_validator COMMAND test_skills_phase10_3_validator)

## Extended cross-reference validation coverage for skills (Phase 4.5 tests)
add_executable(test_skills_crossref_validation_ext tests/unit/test_skills_crossref_validation_ext.c)
target_link_libraries(test_skills_crossref_validation_ext PRIVATE rogue_core)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_skills_crossref_validation_ext PRIVATE SDL2::SDL2)
endif()
rogue_add_test(NAME test_skills_crossref_validation_ext COMMAND test_skills_crossref_validation_ext)

add_executable(test_audio_vfx_phase1_2 tests/unit/test_audio_vfx_phase1_2.c)
target_link_libraries(test_audio_vfx_phase1_2 PRIVATE rogue_core)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_audio_vfx_phase1_2 PRIVATE SDL2::SDL2)
endif()

add_executable(test_audio_vfx_phase1_5_compaction tests/unit/test_audio_vfx_phase1_5_compaction.c)
target_link_libraries(test_audio_vfx_phase1_5_compaction PRIVATE rogue_core)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_audio_vfx_phase1_5_compaction PRIVATE SDL2::SDL2)
endif()

add_executable(test_audio_vfx_phase2_2_mixer_gain tests/unit/test_audio_vfx_phase2_2_mixer_gain.c)
target_link_libraries(test_audio_vfx_phase2_2_mixer_gain PRIVATE rogue_core)
rogue_add_test(test_audio_vfx_phase2_2_mixer_gain COMMAND test_audio_vfx_phase2_2_mixer_gain)

add_executable(test_audio_vfx_phase2_3_mute tests/unit/test_audio_vfx_phase2_3_mute.c)
target_link_libraries(test_audio_vfx_phase2_3_mute PRIVATE rogue_core)
rogue_add_test(test_audio_vfx_phase2_3_mute COMMAND test_audio_vfx_phase2_3_mute)

add_executable(test_audio_vfx_phase2_6_positional tests/unit/test_audio_vfx_phase2_6_positional.c)
target_link_libraries(test_audio_vfx_phase2_6_positional PRIVATE rogue_core)
rogue_add_test(test_audio_vfx_phase2_6_positional COMMAND test_audio_vfx_phase2_6_positional)

add_executable(test_audio_vfx_phase2_1_registry_clear tests/unit/test_audio_vfx_phase2_1_registry_clear.c)
target_link_libraries(test_audio_vfx_phase2_1_registry_clear PRIVATE rogue_core)
rogue_add_test(test_audio_vfx_phase2_1_registry_clear COMMAND test_audio_vfx_phase2_1_registry_clear)

add_executable(test_audio_vfx_phase3_1_vfx_foundations tests/unit/test_audio_vfx_phase3_1_vfx_foundations.c)
target_link_libraries(test_audio_vfx_phase3_1_vfx_foundations PRIVATE rogue_core)
rogue_add_test(test_audio_vfx_phase3_1_vfx_foundations COMMAND test_audio_vfx_phase3_1_vfx_foundations)

add_executable(test_audio_vfx_phase3_2_particles_core tests/unit/test_audio_vfx_phase3_2_particles_core.c)
target_link_libraries(test_audio_vfx_phase3_2_particles_core PRIVATE rogue_core)
rogue_add_test(test_audio_vfx_phase3_2_particles_core COMMAND test_audio_vfx_phase3_2_particles_core)

add_executable(test_audio_vfx_phase3_4_particle_layer_order tests/unit/test_audio_vfx_phase3_4_particle_layer_order.c)
target_link_libraries(test_audio_vfx_phase3_4_particle_layer_order PRIVATE rogue_core)
rogue_add_test(test_audio_vfx_phase3_4_particle_layer_order COMMAND test_audio_vfx_phase3_4_particle_layer_order)

add_executable(test_audio_vfx_phase3_6_screen_vs_world tests/unit/test_audio_vfx_phase3_6_screen_vs_world.c)
target_link_libraries(test_audio_vfx_phase3_6_screen_vs_world PRIVATE rogue_core)
rogue_add_test(test_audio_vfx_phase3_6_screen_vs_world COMMAND test_audio_vfx_phase3_6_screen_vs_world)

add_executable(test_audio_vfx_phase4_1_vfx_config_loader tests/unit/test_audio_vfx_phase4_1_vfx_config_loader.c)
target_link_libraries(test_audio_vfx_phase4_1_vfx_config_loader PRIVATE rogue_core)
rogue_add_test(test_audio_vfx_phase4_1_vfx_config_loader COMMAND test_audio_vfx_phase4_1_vfx_config_loader)

add_executable(test_audio_vfx_phase4_2_hot_reload_and_validation tests/unit/test_audio_vfx_phase4_2_hot_reload_and_validation.c)
target_link_libraries(test_audio_vfx_phase4_2_hot_reload_and_validation PRIVATE rogue_core)
rogue_add_test(test_audio_vfx_phase4_2_hot_reload_and_validation COMMAND test_audio_vfx_phase4_2_hot_reload_and_validation)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_audio_vfx_phase4_2_hot_reload_and_validation PRIVATE SDL2::SDL2)
endif()

if(TARGET SDL2::SDL2)
    target_link_libraries(test_audio_vfx_phase4_1_vfx_config_loader PRIVATE SDL2::SDL2)
endif()

if(TARGET SDL2::SDL2)
    target_link_libraries(test_audio_vfx_phase3_6_screen_vs_world PRIVATE SDL2::SDL2)
endif()

if(TARGET SDL2::SDL2)
    target_link_libraries(test_audio_vfx_phase3_4_particle_layer_order PRIVATE SDL2::SDL2)
endif()

if(TARGET SDL2::SDL2)
    target_link_libraries(test_audio_vfx_phase3_2_particles_core PRIVATE SDL2::SDL2)
endif()

if(TARGET SDL2::SDL2)
    target_link_libraries(test_audio_vfx_phase3_1_vfx_foundations PRIVATE SDL2::SDL2)
endif()

if(TARGET SDL2::SDL2)
    target_link_libraries(test_audio_vfx_phase2_1_registry_clear PRIVATE SDL2::SDL2)
endif()

if(TARGET SDL2::SDL2)
    target_link_libraries(test_audio_vfx_phase2_6_positional PRIVATE SDL2::SDL2)
endif()

if(TARGET SDL2::SDL2)
    target_link_libraries(test_audio_vfx_phase2_3_mute PRIVATE SDL2::SDL2)
endif()

if(TARGET SDL2::SDL2)
    target_link_libraries(test_audio_vfx_phase2_2_mixer_gain PRIVATE SDL2::SDL2)
endif()

add_executable(test_audio_vfx_phase4_3_composition tests/unit/test_audio_vfx_phase4_3_composition.c)
target_link_libraries(test_audio_vfx_phase4_3_composition PRIVATE rogue_core)
rogue_add_test(test_audio_vfx_phase4_3_composition COMMAND test_audio_vfx_phase4_3_composition)

add_executable(test_audio_vfx_phase4_4_parameter_overrides tests/unit/test_audio_vfx_phase4_4_parameter_overrides.c)
target_link_libraries(test_audio_vfx_phase4_4_parameter_overrides PRIVATE rogue_core)
rogue_add_test(test_audio_vfx_phase4_4_parameter_overrides COMMAND test_audio_vfx_phase4_4_parameter_overrides)
add_executable(test_audio_vfx_phase4_5_random_distributions tests/unit/test_audio_vfx_phase4_5_random_distributions.c)
target_link_libraries(test_audio_vfx_phase4_5_random_distributions PRIVATE rogue_core)
rogue_add_test(test_audio_vfx_phase4_5_random_distributions COMMAND test_audio_vfx_phase4_5_random_distributions)

# Items debug API unit test
add_executable(test_item_debug_api tests/unit/test_item_debug_api.c)
target_link_libraries(test_item_debug_api PRIVATE rogue_core)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_item_debug_api PRIVATE SDL2::SDL2)
endif()
rogue_add_test(test_item_debug_api COMMAND test_item_debug_api)

# Phase 5.1 mapping test
add_executable(test_audio_vfx_phase5_1_mapping tests/unit/test_audio_vfx_phase5_1_mapping.c)
target_link_libraries(test_audio_vfx_phase5_1_mapping PRIVATE rogue_core)
rogue_add_test(test_audio_vfx_phase5_1_mapping COMMAND test_audio_vfx_phase5_1_mapping)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_audio_vfx_phase5_1_mapping PRIVATE SDL2::SDL2)
endif()

# Phase 5.2 damage hook test
add_executable(test_audio_vfx_phase5_2_damage_hook tests/unit/test_audio_vfx_phase5_2_damage_hook.c)
target_link_libraries(test_audio_vfx_phase5_2_damage_hook PRIVATE rogue_core)
rogue_add_test(test_audio_vfx_phase5_2_damage_hook COMMAND test_audio_vfx_phase5_2_damage_hook)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_audio_vfx_phase5_2_damage_hook PRIVATE SDL2::SDL2)
endif()

# Phase 5.3 skill cues test
add_executable(test_audio_vfx_phase5_3_skill_cues tests/unit/test_audio_vfx_phase5_3_skill_cues.c)
target_link_libraries(test_audio_vfx_phase5_3_skill_cues PRIVATE rogue_core)
rogue_add_test(test_audio_vfx_phase5_3_skill_cues COMMAND test_audio_vfx_phase5_3_skill_cues)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_audio_vfx_phase5_3_skill_cues PRIVATE SDL2::SDL2)
endif()

# Phase 5.4 buff triggers test
add_executable(test_audio_vfx_phase5_4_buff_triggers tests/unit/test_audio_vfx_phase5_4_buff_triggers.c)
target_link_libraries(test_audio_vfx_phase5_4_buff_triggers PRIVATE rogue_core)
rogue_add_test(test_audio_vfx_phase5_4_buff_triggers COMMAND test_audio_vfx_phase5_4_buff_triggers)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_audio_vfx_phase5_4_buff_triggers PRIVATE SDL2::SDL2)
endif()

# Phase 5.5 loot drop cue test
add_executable(test_audio_vfx_phase5_5_loot_drop tests/unit/test_audio_vfx_phase5_5_loot_drop.c)
target_link_libraries(test_audio_vfx_phase5_5_loot_drop PRIVATE rogue_core)
rogue_add_test(test_audio_vfx_phase5_5_loot_drop COMMAND test_audio_vfx_phase5_5_loot_drop)
if(TARGET SDL2::SDL2)
    target_link_libraries(test_audio_vfx_phase5_5_loot_drop PRIVATE SDL2::SDL2)
endif()

add_executable(roguelike src/main.c)
target_link_libraries(roguelike PRIVATE rogue_core)
target_compile_definitions(roguelike PRIVATE
    ROGUE_BUILD_GIT_HASH="${ROGUE_GIT_HASH}"
    ROGUE_BUILD_GIT_BRANCH="${ROGUE_GIT_BRANCH}"
    ROGUE_BUILD_TIME="${ROGUE_BUILD_TIME}"
)

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

# Items migration/export tool (schema envelope JSON)
add_executable(items_migrate tools/items_migrate.c)
target_link_libraries(items_migrate PRIVATE rogue_core)
if(ROGUE_ENABLE_SDL)
    target_compile_definitions(items_migrate PRIVATE ROGUE_HAVE_SDL=1)
endif()

# Batch content validator (headless-friendly)
add_executable(validate_content tools/validate_content.c)
target_link_libraries(validate_content PRIVATE rogue_core)
if(ROGUE_ENABLE_SDL)
    target_compile_definitions(validate_content PRIVATE ROGUE_HAVE_SDL=1)
endif()

if(WIN32)
    target_link_libraries(rogue_core PUBLIC ole32 windowscodecs)
endif()

# Force 1s cooldowns for all active skills during rapid iteration/testing
target_compile_definitions(rogue_core PUBLIC ROGUE_TEST_SHORT_COOLDOWNS=1)

include(CTest)
enable_testing()

add_subdirectory(tests)

rogue_add_test(test_audio_vfx_phase1_2 COMMAND test_audio_vfx_phase1_2)
rogue_add_test(test_audio_vfx_phase1_5_compaction COMMAND test_audio_vfx_phase1_5_compaction)

## Legacy global PATH/SDL injection logic retained (commented) for reference; superseded by rogue_add_test.
# if(WIN32 AND ROGUE_ENABLE_SDL)
#     set(ROGUE_SDL2_DLL_DIR "${ROGUE_SDL2_DLL_DIR}" CACHE PATH "Directory containing SDL2.dll for test runtime")
#     if(NOT ROGUE_SDL2_DLL_DIR)
#         set(_SDL2_DLL_HINTS
#             "$ENV{ROGUE_SDL2_ROOT}/bin"
#             "$ENV{ROGUE_SDL2_ROOT}/lib/x64"
#             "${ROGUE_SDL2_ROOT}/bin"
#             "${ROGUE_SDL2_ROOT}/lib/x64"
#             "C:/libs/SDL2/bin"
#         )
#         foreach(_hint IN LISTS _SDL2_DLL_HINTS)
#             if(EXISTS "${_hint}/SDL2.dll")
#                 set(ROGUE_SDL2_DLL_DIR ${_hint})
#                 break()
#             endif()
#         endforeach()
#     endif()
#     if(ROGUE_SDL2_DLL_DIR)
#         message(STATUS "(legacy) Tests will add DLL directories to PATH: ${ROGUE_SDL2_DLL_DIR}")
#     else()
#         message(WARNING "(legacy) SDL2.dll directory not found automatically; tests may fail to start.")
#     endif()
# endif()
if(WIN32 AND ROGUE_ENABLE_SDL)
    # Recompute DLL search directories for staging (lightweight)
    set(_rogue_runtime_copy_dirs)
    if(EXISTS "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows/bin")
        list(APPEND _rogue_runtime_copy_dirs "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows/bin")
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows/debug/bin")
        list(APPEND _rogue_runtime_copy_dirs "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows/debug/bin")
    endif()
    list(REMOVE_DUPLICATES _rogue_runtime_copy_dirs)
    set(_rogue_runtime_patterns SDL2.dll SDL2d.dll SDL2_image*.dll SDL2_mixer*.dll SDL2_ttf*.dll libpng*.dll zlib*.dll libjpeg*.dll libwebp*.dll ogg.dll vorbis*.dll FLAC*.dll webp*.dll wavpackdll.dll)
    set(_rogue_runtime_found "")
    foreach(_d IN LISTS _rogue_runtime_copy_dirs)
        if(EXISTS "${_d}")
            foreach(_pat IN LISTS _rogue_runtime_patterns)
                file(GLOB _matches "${_d}/${_pat}")
                if(_matches)
                    list(APPEND _rogue_runtime_found ${_matches})
                endif()
            endforeach()
        endif()
    endforeach()
    if(_rogue_runtime_found)
        list(REMOVE_DUPLICATES _rogue_runtime_found)
        add_custom_target(copy_sdl_runtimes ALL
            COMMAND ${CMAKE_COMMAND} -E echo "Copying SDL runtimes (${_rogue_runtime_found})"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/runtimes"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_rogue_runtime_found} "${CMAKE_BINARY_DIR}/runtimes"
            COMMENT "Stage SDL2 & dependent runtime DLLs locally")
        option(ROGUE_INSTALL_SDL_RUNTIMES "Install staged SDL runtime DLLs" ON)
        if(ROGUE_INSTALL_SDL_RUNTIMES)
            install(DIRECTORY ${CMAKE_BINARY_DIR}/runtimes/ DESTINATION bin FILES_MATCHING PATTERN "*.dll")
        endif()
    else()
        message(WARNING "No SDL runtime DLLs found to copy (patterns: ${_rogue_runtime_patterns})")
    endif()
endif()

# After all tests (including those added in subdirectories) are defined, unify
# the test environment so every test gets the dummy SDL drivers and the staged
# runtime PATH. Previously only a subset of root-level tests had PATH injection,
# leaving many (e.g. AI phase tests) without SDL2d.dll on the loader search path.
# Unified environment now applied at definition time via rogue_add_test; previous late global override removed.

find_program(CLANG_FORMAT_EXE NAMES clang-format)
if(CLANG_FORMAT_EXE)
    add_custom_target(format
        COMMAND ${CMAKE_COMMAND} -DCLANG_FORMAT_EXE=${CLANG_FORMAT_EXE} -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR} -DMODE=format -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunClangFormat.cmake
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running clang-format (per-file)" VERBATIM)
    add_custom_target(format-check
        COMMAND ${CMAKE_COMMAND} -DCLANG_FORMAT_EXE=${CLANG_FORMAT_EXE} -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR} -DMODE=check -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunClangFormat.cmake
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Checking clang-format compliance (per-file)" VERBATIM)
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

# Documentation (Doxygen)
# Previous unconditional find_package(Doxygen) caused intermittent configuration failures on
# Windows when CMake re-entered the project many times in parallel (MSBuild /m) and the module
# attempted to generate/read CMakeDoxyfile.tpl concurrently. We now:
#  1. Only attempt to locate/configure Doxygen if ROGUE_BUILD_DOCS=ON.
#  2. Perform the configuration exactly once (cache guard) to avoid repeated generation races.
#  3. Provide clear status messages for each path (disabled, not found, configured, skipped).
if(ROGUE_BUILD_DOCS)
    if(NOT DEFINED ROGUE_DOCS_CONFIGURED)
        set(ROGUE_DOCS_CONFIGURED TRUE CACHE INTERNAL "Docs configured once to avoid race conditions")
        # Acquire an inter-process lock so parallel CMake configure invocations (MSBuild /m)
        # don't race inside FindDoxygen while it generates/reads temporary templates.
        # If locking fails quickly, we just skip docs instead of failing the whole configure.
        set(_doxygen_lock "${CMAKE_BINARY_DIR}/doxygen.lock")
        set(_got_lock TRUE)
        file(LOCK "${_doxygen_lock}" GUARD FUNCTION TIMEOUT 15 RESULT_VARIABLE _lock_result)
        if(NOT _lock_result STREQUAL "0")
            message(WARNING "Could not acquire Doxygen configure lock (${_doxygen_lock}); skipping docs target this run")
            set(_got_lock FALSE)
        endif()
        if(_got_lock)
            find_package(Doxygen QUIET OPTIONAL_COMPONENTS dot)
        endif()
        if(DOXYGEN_FOUND)
            if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/docs/doxygen/Doxyfile.in)
                message(WARNING "Doxygen build requested but docs/doxygen/Doxyfile.in missing; skipping docs target")
            else()
                set(DOXYGEN_PROJECT_NAME "Roguelike Game Engine")
                set(DOXYGEN_PROJECT_VERSION "${ROGUE_GIT_HASH} (${ROGUE_GIT_BRANCH})")
                set(DOXYGEN_INPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
                set(DOXYGEN_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/docs")
                configure_file(
                    ${CMAKE_CURRENT_SOURCE_DIR}/docs/doxygen/Doxyfile.in
                    ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
                    @ONLY
                )
                add_custom_target(docs
                    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
                    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                    COMMENT "Generating API documentation with Doxygen"
                    VERBATIM
                )
                message(STATUS "Doxygen docs target configured (one-time)")
            endif()
    elseif(_got_lock)
            message(STATUS "ROGUE_BUILD_DOCS=ON but Doxygen not found; skipping docs target")
        endif()
    else()
        message(STATUS "Skipping Doxygen reconfiguration (already configured in prior configure pass)")
    endif()
else()
    # Prevent any nested / third-party CMake logic from trying to locate Doxygen implicitly.
    set(CMAKE_DISABLE_FIND_PACKAGE_Doxygen ON CACHE INTERNAL "Docs disabled; suppress find_package(Doxygen)")
    message(STATUS "ROGUE_BUILD_DOCS=OFF; docs target not generated")
endif()

# -----------------------------
# Packaging (CPack)
# -----------------------------
# Try to pick up a computed semantic version from CI (scripts/ci/compute-version.ps1)
# The script writes build/version.json with keys: version, semver, short_sha, is_prerelease
set(ROGUE_SEMVER "0.1.0")
set(ROGUE_SHORT_SHA "unknown")
if(EXISTS "${CMAKE_BINARY_DIR}/version.json")
    file(READ "${CMAKE_BINARY_DIR}/version.json" _rogue_ver_json)
    if(_rogue_ver_json)
        # Prefer SemVer if present; support both lowercase and PascalCase keys.
        unset(CMAKE_MATCH_1)
        string(REGEX MATCH "\"[Ss]em[Vv]er\"[ \t\r\n]*:[ \t\r\n]*\"([^\"]+)\"" _m_semver "${_rogue_ver_json}")
        if(_m_semver)
            set(ROGUE_SEMVER "${CMAKE_MATCH_1}")
        else()
            unset(CMAKE_MATCH_1)
            string(REGEX MATCH "\"[Vv]ersion\"[ \t\r\n]*:[ \t\r\n]*\"([^\"]+)\"" _m_ver "${_rogue_ver_json}")
            if(_m_ver)
                set(ROGUE_SEMVER "${CMAKE_MATCH_1}")
            endif()
        endif()

        # Extract short SHA from either short_sha or ShortSha.
        unset(CMAKE_MATCH_1)
        string(REGEX MATCH "\"short_sha\"[ \t\r\n]*:[ \t\r\n]*\"([^\"]+)\"" _m_sha1 "${_rogue_ver_json}")
        if(_m_sha1)
            set(ROGUE_SHORT_SHA "${CMAKE_MATCH_1}")
        else()
            unset(CMAKE_MATCH_1)
            string(REGEX MATCH "\"ShortSha\"[ \t\r\n]*:[ \t\r\n]*\"([^\"]+)\"" _m_sha2 "${_rogue_ver_json}")
            if(_m_sha2)
                set(ROGUE_SHORT_SHA "${CMAKE_MATCH_1}")
            endif()
        endif()
    endif()
endif()

set(CPACK_PACKAGE_NAME "roguelike")
set(CPACK_PACKAGE_VENDOR "Roguelike Team")
set(CPACK_PACKAGE_CONTACT "support@example.com")
set(CPACK_PACKAGE_VERSION_MAJOR 0)
set(CPACK_PACKAGE_VERSION_MINOR 1)
set(CPACK_PACKAGE_VERSION_PATCH 0)
set(CPACK_GENERATOR "ZIP;TGZ")

# Name archives with semver (may include pre-release/build metadata) and platform for clarity.
# Example: roguelike-1.2.3-4+gabcd123-Windows.zip
string(TOLOWER "${CMAKE_SYSTEM_NAME}" _rogue_sysname)
if(NOT _rogue_sysname)
    set(_rogue_sysname "unknown")
endif()
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${ROGUE_SEMVER}-${_rogue_sysname}")
set(CPACK_PACKAGE_CHECKSUM "SHA256")

# Install layout: executable + assets
install(TARGETS roguelike RUNTIME DESTINATION bin)
install(DIRECTORY assets/ DESTINATION assets)

if(WIN32 AND ROGUE_ENABLE_SDL)
    # Attempt to include SDL2 runtime DLL if discoverable from VCPKG_ROOT
    if(DEFINED ENV{VCPKG_ROOT})
        set(_SDL_DLL "$ENV{VCPKG_ROOT}/installed/x64-windows/bin/SDL2.dll")
        if(EXISTS "${_SDL_DLL}")
            install(FILES "${_SDL_DLL}" DESTINATION bin)
        endif()
    endif()
endif()

# -----------------------------------------------------------------------------
# Final safeguard: apply unified test environment to ANY test (legacy add_test)
# that was not registered through rogue_add_test. This ensures older test
# definitions still receive PATH & dummy SDL drivers without mass refactors.
# -----------------------------------------------------------------------------
## Enforcement: detect any future raw add_test of test_* targets not wrapped
get_property(_all_tests GLOBAL PROPERTY TESTS)
set(_violations)
foreach(_t IN LISTS _all_tests)
    # If name starts with test_ and not present in wrapped list, flag
    if(_t MATCHES "^test_" )
        list(FIND ROGUE_WRAPPED_TESTS ${_t} _idx)
        if(_idx EQUAL -1)
            list(APPEND _violations ${_t})
        endif()
    endif()
endforeach()
if(_violations)
    list(JOIN _violations ", " _violations_str)
    message(FATAL_ERROR "Detected raw add_test usage for test targets without rogue_add_test wrapper: ${_violations_str}. Please register tests via rogue_add_test for unified environment.")
endif()

include(CPack)
