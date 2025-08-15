#ifndef ROGUE_CORE_APP_STATE_H
#define ROGUE_CORE_APP_STATE_H
#include "core/app.h"
#include "world/tilemap.h"
#include "input/input.h"
#include "entities/player.h"
#include "entities/enemy.h"
#include "game/combat.h"
#include "graphics/sprite.h"
#include "graphics/tile_sprites.h"
#include "util/log.h"
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif
#ifdef ROGUE_HAVE_SDL_MIXER
#include <SDL_mixer.h>
#endif

typedef struct RogueAppState {
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
    RogueSprite  player_frames[4][4][8];
    int          player_frame_count[4][4];
    int          player_frame_time_ms[4][4][8];
    int player_loaded;
    int player_sheet_loaded[4][4];
    int player_state;
    char player_sheet_path[4][4][256];
    int player_sheet_paths_loaded;
    double title_time;
    int menu_index;
    int entering_seed;
    unsigned int pending_seed;
    int frame_count; double dt; double fps; double frame_ms; double avg_frame_ms_accum; int avg_frame_samples;
    double game_time_ms; /* accumulated game time in milliseconds (for cooldowns) */
    float cam_x, cam_y; int viewport_w, viewport_h;
    float walk_speed, run_speed;
    const RogueSprite** tile_sprite_lut; int tile_sprite_lut_ready;
    int minimap_dirty;
#ifdef ROGUE_HAVE_SDL
    SDL_Texture* minimap_tex;
#endif
    int minimap_w, minimap_h, minimap_step;
    int chunk_size, chunks_x, chunks_y; unsigned char* chunk_dirty;
    float anim_dt_accum_ms;
    int frame_draw_calls; int frame_tile_quads;
    double gen_water_level; int gen_noise_octaves; double gen_noise_gain; double gen_noise_lacunarity; int gen_river_sources; int gen_river_max_length; double gen_cave_thresh; int gen_params_dirty;
    RogueEnemy enemies[ROGUE_MAX_ENEMIES]; int enemy_count; RogueEnemyTypeDef enemy_types[ROGUE_MAX_ENEMY_TYPES]; int enemy_type_count; RoguePlayerCombat player_combat; int total_kills; int per_type_counts[ROGUE_MAX_ENEMY_TYPES]; double difficulty_scalar;
    int show_stats_panel; int stats_panel_index;
    float time_since_player_hit_ms; float health_regen_accum_ms; float mana_regen_accum_ms; float ap_regen_accum_ms; float levelup_aura_timer_ms;
    float ap_throttle_timer_ms; /* remaining ms of AP soft throttle (reduced regen) */
#ifdef ROGUE_HAVE_SDL_MIXER
    Mix_Chunk* sfx_levelup;
#endif
    float attack_anim_time_ms;
    struct { float x,y; float vx,vy; float life_ms; float total_ms; int amount; int from_player; int crit; float scale; } dmg_numbers[128];
    int dmg_number_count;
    double spawn_accum_ms;
    float hitstop_timer_ms;
    /* Skill system */
    int skill_count; /* number of registered skill definitions */
    struct RogueSkillDef* skill_defs; /* registry (owned by skills module) */
    struct RogueSkillState* skill_states; /* per-skill runtime state (ranks, cooldowns) */
    int skill_bar[10]; /* skill id per slot (-1 empty) */
    /* Loot runtime (phase 2) */
    struct RogueItemInstance* item_instances; int item_instance_cap; int item_instance_count;
    /* Session metrics (9.5) */
    double session_start_seconds;
    unsigned int session_items_dropped;
    unsigned int session_items_picked;
    unsigned int session_rarity_drops[5];
    /* In-world vendor (interactive economy showcase) */
    float vendor_x, vendor_y; int show_vendor_panel; int vendor_selection; unsigned int vendor_seed; double vendor_time_accum_ms; double vendor_restock_interval_ms;
    /* Phase 4.8 vendor transaction confirmation */
    int vendor_confirm_active; int vendor_confirm_def_index; int vendor_confirm_price; double vendor_insufficient_flash_ms;
    /* Equipment panel UI */
    int show_equipment_panel;
} RogueAppState;

extern RogueAppState g_app;
extern RoguePlayer g_exposed_player_for_stats;

#endif
