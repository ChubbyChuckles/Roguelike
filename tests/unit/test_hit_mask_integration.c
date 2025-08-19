#include "game/hit_pixel_mask.h"
#include "game/hit_system.h"
#include "entities/player.h"
#include "entities/enemy.h"
#include "game/combat.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#ifdef main
#undef main
#endif
int main(int argc, char** argv){ (void)argc; (void)argv; g_hit_use_pixel_masks = 1; /* enable */
    RoguePlayer p={0}; p.base.pos.x=0; p.base.pos.y=0; p.anim_frame=7; p.equipped_weapon_id=0; p.facing=2; /* right */
    RoguePlayerCombat pc={0}; pc.phase=ROGUE_ATTACK_STRIKE; /* in strike phase */
    RogueEnemy enemies[2]; for(int i=0;i<2;i++){ enemies[i].alive=1; enemies[i].base.pos.x = 0.0f; enemies[i].base.pos.y = 0.0f; }
    enemies[0].base.pos.x = 30.0f; enemies[0].base.pos.y = 7.0f; /* inside placeholder mask bar region */
    enemies[1].base.pos.x = 5.0f; enemies[1].base.pos.y = 5.0f; /* out of range */
    int hc = rogue_combat_weapon_sweep_apply(&pc, &p, enemies, 2);
    if(hc < 1){ printf("FAIL: expected at least 1 hit via pixel mask path\n"); return 1; }
    printf("PASS: pixel mask integration basic (hits=%d)\n", hc);
    return 0; }
