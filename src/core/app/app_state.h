/**
 * @file app_state.h
 * @brief Global application state structure and management.
 *
 * Defines the central RogueAppState structure that holds all of the game's
 * runtime state including player data, world state, UI states, graphics
 * resources, audio handles, and various subsystem configurations. This
 * structure serves as the primary data hub for the entire application.
 *
 * @author [Your Name]
 * @date September 2025
 * @version 1.0
 */

#ifndef ROGUE_CORE_APP_STATE_H
#define ROGUE_CORE_APP_STATE_H
#include "../../entities/enemy.h"
#include "../../entities/player.h"
#include "../../game/combat.h"
#include "../../graphics/sprite.h"
#include "../../graphics/tile_sprites.h"
#include "../../input/input.h"
#include "../../util/log.h"
#include "../../world/tilemap.h"
#include "app.h"
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif
#ifdef ROGUE_HAVE_SDL_MIXER
#include <SDL_mixer.h>
#endif

/**
 * @brief Central application state structure.
 *
 * Contains all runtime state for the roguelike application including
 * configuration, graphics resources, game world state, player data,
 * enemy management, UI panel states, and various subsystem data.
 * This structure serves as the primary data hub that connects all
 * game systems together.
 */
typedef struct RogueAppState
{
    /* Player progression data */
    int level;                         ///< Player level (mirrored from player.level for tests/persistence)
    unsigned long long xp_total_accum; ///< Total accumulated XP (mirrored from player.xp_total_accum)
    
    /* Core application configuration */
    RogueAppConfig cfg; ///< Application configuration (window, rendering, etc.)
    
#ifdef ROGUE_HAVE_SDL
    SDL_Window* window;     ///< SDL window handle
    SDL_Renderer* renderer; ///< SDL renderer handle
#endif
    
    /* Application mode flags */
    int headless;          ///< 1 if running without graphics (testing mode)
    int show_start_screen; ///< 1 if currently displaying start screen
    
    /* World and input state */
    RogueTileMap world_map; ///< The game world tilemap
    RogueInputState input;  ///< Current input state
    RoguePlayer player;     ///< Player entity and stats
    
    /* Character progression */
    int unspent_stat_points; ///< Available attribute points to spend
    int talent_points;       ///< Available skill tree points
    int stats_dirty;         ///< 1 if stats need UI refresh
    
    /* Graphics and rendering state */
    int tileset_loaded;    ///< 1 if tileset graphics are loaded
    int tile_size;         ///< Size of each tile in pixels
    int player_frame_size; ///< Size of player animation frames
    
    /* Player sprite animation data */
    RogueTexture player_tex[4][4];           ///< Player textures [direction][state]
    RogueSprite player_frames[4][4][8];      ///< Player animation frames [dir][state][frame]
    int player_frame_count[4][4];            ///< Number of frames per [direction][state]
    int player_frame_time_ms[4][4][8];       ///< Frame durations in milliseconds
    int player_loaded;                       ///< 1 if player graphics are loaded
    int player_sheet_loaded[4][4];           ///< Load status per [direction][state]
    int player_state;                        ///< Current player animation state
    char player_sheet_path[4][4][256];       ///< Paths to sprite sheet files
    int player_sheet_paths_loaded;           ///< 1 if sprite paths are loaded
    
    /* Start screen state machine */
    double title_time;           ///< Time spent on title screen
    int start_state;             ///< Current start screen state (RogueStartScreenState)
    float start_state_t;         ///< Progress through current state (0..1)
    float start_state_speed;     ///< Animation speed for state transitions
    int reduced_motion;          ///< 1 if reduced motion accessibility is enabled
    int high_contrast;           ///< 1 if high contrast UI mode is enabled
    
    /* World transition effects */
    int dev_escape_to_start;   ///< 1 to allow ESC to return to start screen (dev mode)
    int world_fade_active;     ///< 1 while world fade-in overlay is animating
    float world_fade_t;        ///< Fade overlay alpha (1=fully black, 0=transparent)
    float world_fade_speed;    ///< Fade animation speed in units per second
    
    /* Start screen background */
#ifdef ROGUE_HAVE_SDL
    struct RogueTexture* start_bg_tex; ///< Background texture for start screen
#endif
    int start_bg_loaded;        ///< 1 if background texture is loaded
    int start_bg_scale;         ///< Background scaling mode (RogueStartBGScale)
    unsigned int start_bg_tint; ///< Background tint color (ARGB packed)
    
    /* Start screen UI sub-states */
    int start_show_load_list; ///< 1 when load game list is displayed
    int start_load_selection; ///< Selected index in load game list
    int start_show_settings;  ///< 1 when settings overlay is active
    int start_settings_index; ///< Selected index in settings menu
    
    /* Credits and legal overlay */
    int start_show_credits;     ///< 1 when credits/legal overlay is active
    int start_credits_tab;      ///< Active tab (0=Credits, 1=Licenses, 2=Build)
    float start_credits_scroll; ///< Current scroll position
    float start_credits_vel;    ///< Scroll velocity for inertial scrolling
    
    /* Menu navigation */
    int menu_index;        ///< Currently selected menu item
    int entering_seed;     ///< 1 if currently entering world seed
    unsigned int pending_seed; ///< Seed being entered for new game
    
    /* Background loading and performance */
    int start_prewarm_active;  ///< 1 while background preloading is active
    int start_prewarm_done;    ///< 1 after all preloading is complete
    int start_prewarm_step;    ///< Current preloading step index
    float start_spinner_angle; ///< Loading spinner rotation angle
    
    /* Performance monitoring for start screen */
    double start_perf_budget_ms;             ///< Frame time budget threshold
    double start_perf_baseline_ms;           ///< Measured baseline frame time
    double start_perf_accum_ms;              ///< Accumulated samples for baseline
    int start_perf_samples;                  ///< Number of baseline samples recorded
    int start_perf_target_samples;           ///< Target samples for baseline calculation
    double start_perf_regress_threshold_pct; ///< Performance regression threshold
    int start_perf_regressed;                ///< 1 if performance regression detected
    int start_perf_reduce_quality;           ///< 1 to reduce visual quality for performance
    int start_perf_warned;                   ///< 1 if performance warning was logged
    
    /* Navigation input repeat handling */
    double start_nav_accum_ms;    ///< Accumulated time for current navigation direction
    int start_nav_dir_v;          ///< Navigation direction (-1 up, +1 down, 0 none)
    int start_nav_repeating;      ///< 1 when in repeat input mode
    double start_nav_initial_ms;  ///< Initial delay before input repeat
    double start_nav_interval_ms; ///< Interval between repeated inputs
    
    /* Frame timing and performance */
    int frame_count;               ///< Total frames rendered since init
    double dt;                     ///< Delta time for current frame
    double fps;                    ///< Current frames per second
    double frame_ms;               ///< Current frame time in milliseconds
    double avg_frame_ms_accum;     ///< Accumulated frame times for averaging
    int avg_frame_samples;         ///< Number of frame samples in accumulator
    double game_time_ms;           ///< Total accumulated game time in milliseconds
    
    /* Camera and viewport */
    float cam_x, cam_y;           ///< Camera position in world coordinates
    int viewport_w, viewport_h;   ///< Viewport dimensions in pixels
    float walk_speed, run_speed;  ///< Player movement speeds
    
    /* Tile rendering system */
    const RogueSprite** tile_sprite_lut; ///< Lookup table for tile sprites
    int tile_sprite_lut_ready;           ///< 1 if sprite lookup table is ready
    
    /* Minimap rendering */
    int minimap_dirty; ///< 1 if minimap needs regeneration
#ifdef ROGUE_HAVE_SDL
    SDL_Texture* minimap_tex; ///< Cached minimap texture
#endif
    int minimap_w, minimap_h, minimap_step; ///< Minimap dimensions and resolution
    int chunk_size, chunks_x, chunks_y;     ///< World chunking parameters
    unsigned char* chunk_dirty;             ///< Dirty flags for world chunks
    
    /* Animation timing */
    float anim_dt_accum_ms; ///< Accumulated delta time for animations
    
    /* Rendering statistics */
    int frame_draw_calls; ///< Number of draw calls in current frame
    int frame_tile_quads; ///< Number of tile quads rendered in current frame
    
    /* World generation parameters */
    double gen_water_level;      ///< Water level for world generation
    int gen_noise_octaves;       ///< Number of noise octaves
    double gen_noise_gain;       ///< Noise gain parameter
    double gen_noise_lacunarity; ///< Noise lacunarity parameter
    int gen_river_sources;       ///< Number of river sources to generate
    int gen_river_max_length;    ///< Maximum river length
    double gen_cave_thresh;      ///< Cave generation threshold
    int gen_params_dirty;        ///< 1 if generation parameters changed
    
    /* Enemy management */
    RogueEnemy enemies[ROGUE_MAX_ENEMIES];         ///< Array of active enemies
    int enemy_count;                               ///< Number of active enemies
    RogueEnemyTypeDef enemy_types[ROGUE_MAX_ENEMY_TYPES]; ///< Enemy type definitions
    int enemy_type_count;                          ///< Number of registered enemy types
    RoguePlayerCombat player_combat;               ///< Player combat state
    int total_kills;                               ///< Total enemies killed
    int per_type_counts[ROGUE_MAX_ENEMY_TYPES];    ///< Kill count per enemy type
    double difficulty_scalar;                     ///< Current difficulty multiplier
    
    /* UI panel states */
    int show_stats_panel;  ///< 1 if stats panel is visible
    int stats_panel_index; ///< Selected index in stats panel
    int show_minimap;      ///< 1 if minimap is visible
    
    /* Player status and regeneration */
    float time_since_player_hit_ms; ///< Time since player last took damage
    float health_regen_accum_ms;    ///< Accumulated time for health regeneration
    float mana_regen_accum_ms;      ///< Accumulated time for mana regeneration
    float ap_regen_accum_ms;        ///< Accumulated time for action point regeneration
    float levelup_aura_timer_ms;    ///< Remaining time for level-up visual effect
    float ap_throttle_timer_ms;     ///< Remaining time for AP regeneration penalty
    
    /* Action Point overdrive system */
    int ap_overdrive_bonus;   ///< Temporary AP capacity bonus
    float ap_overdrive_ms;    ///< Remaining overdrive duration
    float ap_exhaustion_ms;   ///< Remaining exhaustion penalty duration
    
    /* Heat/overheat system */
    int overheat_active;      ///< 1 if player is overheated
    float heat_vent_accum_ms; ///< Accumulated time for heat venting
    
    /* Audio resources */
#ifdef ROGUE_HAVE_SDL_MIXER
    Mix_Chunk* sfx_levelup; ///< Level-up sound effect
    Mix_Music* bgm_music;   ///< Background music track
    int bgm_playing;        ///< 1 if background music is playing
#endif
    
    /* Combat animation */
    float attack_anim_time_ms; ///< Remaining time for attack animation
    
    /* Floating damage numbers */
    struct
    {
        float x, y;         ///< World position
        float vx, vy;       ///< Velocity vector
        float life_ms;      ///< Remaining life time
        float total_ms;     ///< Total life time (for fade calculations)
        int amount;         ///< Damage amount to display
        int from_player;    ///< 1 if damage dealt by player
        int crit;           ///< 1 if critical hit
        float scale;        ///< Display scale factor
        float spawn_ms;     ///< Time when damage number was spawned
        float alpha;        ///< Current alpha transparency
    } dmg_numbers[128];     ///< Array of active damage numbers
    int dmg_number_count;   ///< Number of active damage numbers
    
    /* Gameplay timing */
    double spawn_accum_ms;   ///< Accumulated time for enemy spawning
    float hitstop_timer_ms;  ///< Remaining hitstop/freeze frame time
    
    /* Skill system */
    int skill_count;                      ///< Number of registered skill definitions
    struct RogueSkillDef* skill_defs;     ///< Array of skill definitions
    struct RogueSkillState* skill_states; ///< Runtime state per skill (cooldowns, ranks)
    int skill_bar[10];                    ///< Skill IDs assigned to hotbar slots (-1 = empty)
    
    /* Item and loot system */
    struct RogueItemInstance* item_instances; ///< Array of item instances in world
    int item_instance_cap;                    ///< Capacity of item instance array
    int item_instance_count;                  ///< Number of active item instances
    
    /* Session metrics */
    double session_start_seconds;         ///< Session start time
    unsigned int session_items_dropped;   ///< Items dropped this session
    unsigned int session_items_picked;    ///< Items picked up this session
    unsigned int session_rarity_drops[5]; ///< Item drops by rarity tier
    
    /* Analytics counters */
    unsigned long long analytics_damage_dealt_total; ///< Total damage dealt (persistent)
    unsigned long long analytics_gold_earned_total;  ///< Total gold earned (persistent)
    
    /* Vendor system */
    float vendor_x, vendor_y;             ///< Vendor NPC position
    int show_vendor_panel;                ///< 1 if vendor UI is visible
    int vendor_selection;                 ///< Selected item in vendor UI
    int vendor_tab;                       ///< Active vendor tab (Buy/Sell/Buyback/Special)
    int vendor_def_index;                 ///< Current vendor definition index
    unsigned int vendor_seed;             ///< Seed for vendor inventory generation
    double vendor_time_accum_ms;          ///< Time accumulator for vendor restocking
    double vendor_restock_interval_ms;    ///< Vendor restock interval
    
    /* Vendor transaction confirmation */
    int vendor_confirm_active;            ///< 1 if transaction confirmation is active
    int vendor_confirm_def_index;         ///< Item index for pending transaction
    int vendor_confirm_price;             ///< Price for pending transaction
    double vendor_insufficient_flash_ms;  ///< Flash timer for insufficient funds
    
    /* Equipment panel UI */
    int show_equipment_panel; ///< 1 if equipment panel is visible
    
    /* Inventory panel UI */
    int show_inventory_panel; ///< 1 if inventory panel is visible
    
    /* Skill graph UI */
    int show_skill_graph; ///< 1 if skill tree UI is visible
    
    /* Skill icon textures */
#ifdef ROGUE_HAVE_SDL
    RogueTexture* skill_icon_textures; ///< Icon textures parallel to skill_defs array
#endif
    
    /* Run metadata */
    int permadeath_mode; ///< 1 if current run uses permadeath
    
    /* Inventory UI settings */
    int inventory_sort_mode; ///< Current inventory sorting mode (persistent)
    
    /* Debug overlay toggles */
    int show_metrics_overlay; ///< 1 if performance metrics overlay is visible
    
    /* Internal test hooks */
    int last_minimap_rendered; ///< Frame count when minimap was last rendered
    int last_metrics_rendered; ///< Frame count when metrics were last rendered
    int last_alerts_rendered;  ///< Frame count when alerts were last rendered
    
    /* Debug visualization toggles */
    int show_hit_debug;           ///< 1 if hit detection debug overlay is visible
    int show_skill_area_overlay;  ///< 1 if skill area overlay is visible
    
    /* Enemy targeting */
    int target_enemy_active; ///< 1 if an enemy is currently targeted
    int target_enemy_level;  ///< Level of currently targeted enemy
    
    /* World generation */
    unsigned int world_seed; ///< Seed used for world and encounter generation
    
    /* Enemy difficulty scaling */
    float time_since_last_enemy_death_ms; ///< Time since last enemy was killed
    
    /* Player debug modes */
    int noclip_enabled;   ///< 1 if collision detection is disabled
    int god_mode_enabled; ///< 1 if player takes no damage
} RogueAppState;

/**
 * @brief Global application state instance.
 *
 * The single global instance of the application state that is accessed
 * throughout the codebase. Contains all runtime game state.
 */
extern RogueAppState g_app;

/**
 * @brief Exposed player reference for statistics.
 *
 * A global reference to the player data specifically for statistics
 * and external systems that need read-only player access.
 */
extern RoguePlayer g_exposed_player_for_stats;

#if defined(_MSC_VER)
/**
 * @brief Initialize application state (MSVC-specific).
 *
 * MSVC-specific initialization function for the global application state.
 * Ensures proper initialization on Microsoft Visual C++ compiler.
 */
void rogue_app_state_maybe_init(void);
#endif

#endif
