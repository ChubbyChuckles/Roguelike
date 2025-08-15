#define SDL_MAIN_HANDLED
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "game/combat.h"
#include "entities/player.h"

RoguePlayer g_exposed_player_for_stats = {0};
int rogue_get_current_attack_frame(void){ return 3; }
void rogue_app_add_hitstop(float ms){ (void)ms; }
void rogue_add_damage_number(float x,float y,int amount,int from_player){ (void)x;(void)y;(void)amount;(void)from_player; }
void rogue_add_damage_number_ex(float x,float y,int amount,int from_player,int crit){ (void)x;(void)y;(void)amount;(void)from_player;(void)crit; }

/* Use real app struct to avoid symbol redefinition; stub input functions via existing input API expectations. */
#include "core/app.h"
#include "core/player_controller.h"
/* Override movement-affecting helpers with no-op via macros if needed */
float rogue_vegetation_tile_move_scale(int x,int y){ (void)x;(void)y; return 1.0f; }
int rogue_vegetation_tile_blocking(int x,int y){ (void)x;(void)y; return 0; }
int rogue_vegetation_entity_blocking(float ox,float oy,float nx,float ny){ (void)ox;(void)oy;(void)nx;(void)ny; return 0; }

/* Provide world tiles */
static unsigned char tiles_stub[4] = {0,0,0,0};

int main(){
    rogue_player_init(&g_app.player); g_exposed_player_for_stats = g_app.player; g_app.run_speed=2.0f; g_app.walk_speed=1.0f; g_app.dt=16.0f; g_app.world_map.width=2; g_app.world_map.height=2; g_app.world_map.tiles=tiles_stub; g_app.tile_size=16; g_app.viewport_w=64; g_app.viewport_h=64; g_app.player_state=2;
    float start_x = g_app.player.base.pos.x;
    /* Apply slow 50% */
    g_app.player.cc_slow_ms = 100.0f; g_app.player.cc_slow_pct = 0.5f; g_app.player_state=2; /* running */
    rogue_player_controller_update();
    float moved_slow = g_app.player.base.pos.x - start_x; /* zero because no input, we test speed scaling indirectly */
    /* Stun prevents attacks: attempt attack start suppressed by setting stun */
    RoguePlayerCombat pc; rogue_combat_init(&pc);
    g_app.player.cc_stun_ms = 200.0f;
    rogue_combat_update_player(&pc,16.0f,1); /* attack_pressed=1 */
    assert(pc.buffered_attack==0); /* suppressed */
    /* Disarm prevents attack but not movement (movement still zero due to no input) */
    g_app.player.cc_stun_ms = 0.0f; g_app.player.cc_disarm_ms = 200.0f;
    rogue_combat_update_player(&pc,16.0f,1);
    assert(pc.buffered_attack==0);
    /* Root prevents movement but allows attack: clear disarm, set root then attack */
    g_app.player.cc_disarm_ms=0.0f; g_app.player.cc_root_ms=200.0f;
    rogue_combat_update_player(&pc,16.0f,1);
    assert(pc.buffered_attack==1);
    printf("phase4_cc: OK (slow%.2f)\n", moved_slow);
    return 0;
}
