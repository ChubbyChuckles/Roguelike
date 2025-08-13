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

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(cmake/Warnings.cmake)
rogue_set_project_warnings(rogue_warnings)

if(ROGUE_ENABLE_SDL)
    find_package(SDL2 REQUIRED)
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
    src/core/app.c
    src/core/metrics.c
    src/core/app_state.c
    src/core/game_loop.c
    src/core/minimap.c
    src/core/player_controller.c
    src/core/input_events.c
    src/core/player_render.c
    src/core/enemy_render.c
    src/core/hud.c
    src/core/player_progress.c
    src/core/world_renderer.c
    src/core/animation_system.c
    src/core/persistence.c
    src/core/persistence_autosave.c
    src/core/skills.c
    src/core/skill_tree.c
    src/core/skill_bar.c
    src/core/asset_config.c
    src/core/platform.c
    src/core/tile_sprite_cache.c
    src/platform/platform_sdl.c
    src/graphics/renderer.c
    src/graphics/sprite.c
    src/graphics/tile_sprites.c
    src/graphics/png_wic.c
    src/input/input.c
    src/world/tilemap.c
    src/world/world_gen.c
    src/world/world_gen_config.c
    src/graphics/font.c
    src/entities/player.c
    src/entities/enemy.c
    src/game/combat.c
    src/game/damage_numbers.c
    src/core/enemy_system.c
    src/core/start_screen.c
    src/core/player_assets.c
    src/util/log.c
)

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

add_executable(roguelike src/main.c)
target_link_libraries(roguelike PRIVATE rogue_core)

if(WIN32)
    target_link_libraries(rogue_core PUBLIC ole32 windowscodecs)
endif()

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
