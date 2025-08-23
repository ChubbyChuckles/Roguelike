#ifndef ROGUE_CORE_APP_STATE_H
#define ROGUE_CORE_APP_STATE_H
#include "core/app.h"
#include "entities/enemy.h"
#include "entities/player.h"
#include "game/combat.h"
#include "graphics/sprite.h"
#include "graphics/tile_sprites.h"
#include "input/input.h"
#include "util/log.h"
#include "world/tilemap.h"
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif
#ifdef ROGUE_HAVE_SDL_MIXER
#include <SDL_mixer.h>
#endif

typedef struct RogueAppState
{
    RogueAppConfig cfg;
#ifdef ROGUE_HAVE_SDL
    SDL_Window* window;
    SDL_Renderer* renderer;
#endif
    int headless;
    int show_start_screen;
    RogueTileMap world_map;
    RogueInputState input;
    RoguePlayer player;
    int unspent_stat_points;
    int talent_points; /* points for skill tree */
    int stats_dirty;
    int tileset_loaded;
    int tile_size;
    int player_frame_size;
    RogueTexture player_tex[4][4];
    RogueSprite player_frames[4][4][8];
    int player_frame_count[4][4];
    int player_frame_time_ms[4][4][8];
    int player_loaded;
    int player_sheet_loaded[4][4];
    int player_state;
    char player_sheet_path[4][4][256];
    int player_sheet_paths_loaded;
    double title_time;
    /* Start screen state machine (Phase 1.1) */
    int start_state;         /* RogueStartScreenState */
    float start_state_t;     /* 0..1 progress for fades */
    float start_state_speed; /* units per second for fades (scaled if reduced motion) */
    /* Start screen background (Phase 2.1..2.3) */
#ifdef ROGUE_HAVE_SDL
    struct RogueTexture* start_bg_tex; /* owned texture handle */
#endif
    int start_bg_loaded;        /* 1 if texture present */
    int start_bg_scale;         /* RogueStartBGScale */
    unsigned int start_bg_tint; /* ARGB tint packed */
    int menu_index;
    int entering_seed;
    unsigned int pending_seed;
    /* Start screen navigation repeat (Phase 3.2 wiring) */
    double start_nav_accum_ms;    /* accumulated hold time for current direction */
    int start_nav_dir_v;          /* -1 up, +1 down, 0 none */
    int start_nav_repeating;      /* 1 when in repeat window */
    double start_nav_initial_ms;  /* initial delay before repeat */
    double start_nav_interval_ms; /* repeat interval */
    int frame_count;
    double dt;
    double fps;
    double frame_ms;
    double avg_frame_ms_accum;
    int avg_frame_samples;
    double game_time_ms; /* accumulated game time in milliseconds (for cooldowns) */
    float cam_x, cam_y;
    int viewport_w, viewport_h;
    float walk_speed, run_speed;
    const RogueSprite** tile_sprite_lut;
    int tile_sprite_lut_ready;
    int minimap_dirty;
#ifdef ROGUE_HAVE_SDL
    SDL_Texture* minimap_tex;
#endif
    int minimap_w, minimap_h, minimap_step;
    int chunk_size, chunks_x, chunks_y;
    unsigned char* chunk_dirty;
    float anim_dt_accum_ms;
    int frame_draw_calls;
    int frame_tile_quads;
    double gen_water_level;
    int gen_noise_octaves;
    double gen_noise_gain;
    double gen_noise_lacunarity;
    int gen_river_sources;
    int gen_river_max_length;
    double gen_cave_thresh;
    int gen_params_dirty;
    RogueEnemy enemies[ROGUE_MAX_ENEMIES];
    int enemy_count;
    RogueEnemyTypeDef enemy_types[ROGUE_MAX_ENEMY_TYPES];
    int enemy_type_count;
    RoguePlayerCombat player_combat;
    int total_kills;
    int per_type_counts[ROGUE_MAX_ENEMY_TYPES];
    double difficulty_scalar;
    int show_stats_panel;
    int stats_panel_index;
    /* Phase 6.5 minimap toggle */
    int show_minimap;
    float time_since_player_hit_ms;
    float health_regen_accum_ms;
    float mana_regen_accum_ms;
    float ap_regen_accum_ms;
    float levelup_aura_timer_ms;
    float ap_throttle_timer_ms; /* remaining ms of AP soft throttle (reduced regen) */
#ifdef ROGUE_HAVE_SDL_MIXER
    Mix_Chunk* sfx_levelup;
#endif
    float attack_anim_time_ms;
    struct
    {
        float x, y;
        float vx, vy;
        float life_ms;
        float total_ms;
        int amount;
        int from_player;
        int crit;
        float scale;
        float spawn_ms;
        float alpha;
    } dmg_numbers[128]; /* Phase 6.4 adds spawn_ms & alpha */
    int dmg_number_count;
    double spawn_accum_ms;
    float hitstop_timer_ms;
    /* Skill system */
    int skill_count;                      /* number of registered skill definitions */
    struct RogueSkillDef* skill_defs;     /* registry (owned by skills module) */
    struct RogueSkillState* skill_states; /* per-skill runtime state (ranks, cooldowns) */
    int skill_bar[10];                    /* skill id per slot (-1 empty) */
    /* Loot runtime (phase 2) */
    struct RogueItemInstance* item_instances;
    int item_instance_cap;
    int item_instance_count;
    /* Session metrics (9.5) */
    double session_start_seconds;
    unsigned int session_items_dropped;
    unsigned int session_items_picked;
    unsigned int session_rarity_drops[5];
    /* Analytics counters (Phase 7.7 persistence) */
    unsigned long long analytics_damage_dealt_total; /* cumulative damage dealt */
    unsigned long long analytics_gold_earned_total;  /* cumulative gold earned */
    /* In-world vendor (interactive economy showcase) */
    float vendor_x, vendor_y;
    int show_vendor_panel;
    int vendor_selection;
    unsigned int vendor_seed;
    double vendor_time_accum_ms;
    double vendor_restock_interval_ms;
    /* Phase 4.8 vendor transaction confirmation */
    int vendor_confirm_active;
    int vendor_confirm_def_index;
    int vendor_confirm_price;
    double vendor_insufficient_flash_ms;
    /* Equipment panel UI */
    int show_equipment_panel;
    /* Inventory panel UI */
    int show_inventory_panel;
    /* Runtime skill graph (Phase 5 demo) */
    int show_skill_graph; /* toggled via hotkey; renders experimental UI skill graph */
    /* Skill icon textures (parallel to skill_defs by id). Loaded after skills.cfg parse. */
#ifdef ROGUE_HAVE_SDL
    RogueTexture* skill_icon_textures; /* array size skill_count */
#endif
    /* Run metadata (Phase 7.8) */
    int permadeath_mode; /* 1 if run flagged permadeath */
    /* Inventory UI (13.5) */
    int inventory_sort_mode; /* RogueInventorySortMode persisted */
    /* Phase 6.7 metrics overlay toggle */
    int show_metrics_overlay;
    /* Internal test hooks (non-persisted) */
    int last_minimap_rendered;
    int last_metrics_rendered;
    int last_alerts_rendered;
    /* Hit System Phase 6 debug toggle */
    int show_hit_debug;
    /* Enemy Difficulty Phase 1.6 UI indicator state */
    int target_enemy_active; /* 1 if a target enemy is selected/focused */
    int target_enemy_level;  /* level of targeted enemy */
    /* Enemy Integration Phase 1: global world seed used for encounter seed derivation */
    unsigned int world_seed;
    /* Enemy Difficulty Phase 5: recent death timer for intensity escalation */
    float time_since_last_enemy_death_ms;
} RogueAppState;

extern RogueAppState g_app;
extern RoguePlayer g_exposed_player_for_stats;
#if defined(_MSC_VER)
void rogue_app_state_maybe_init(void);
#endif

#endif
