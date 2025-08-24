#include "combat.h"

/* Mobility, dodge, aerial, projectile deflection */
int rogue_player_dodge_roll(RoguePlayer* p, RoguePlayerCombat* pc, int dir)
{
    if (!p || !pc)
        return 0;
    if (p->reaction_type != 0)
        return 0;
    if (pc->phase == ROGUE_ATTACK_STRIKE)
        return 0;
    const float COST = 18.0f;
    if (pc->stamina < COST)
        return 0;
    pc->stamina -= COST;
    if (pc->stamina < 0)
        pc->stamina = 0;
    pc->stamina_regen_delay = 350.0f;
    p->iframes_ms = 400.0f;
    if (p->poise + 10.0f < p->poise_max)
        p->poise += 10.0f;
    else
        p->poise = p->poise_max;
    if (dir >= 0 && dir <= 3)
        p->facing = dir;
    return 1;
}
void rogue_player_set_airborne(RoguePlayer* p, RoguePlayerCombat* pc)
{
    (void) p;
    if (!pc)
        return;
    pc->aerial_attack_pending = 1;
}
int rogue_player_is_airborne(const RoguePlayer* p)
{
    (void) p;
    return 0;
}
int rogue_player_try_deflect_projectile(RoguePlayer* p, RoguePlayerCombat* pc, float proj_dir_x,
                                        float proj_dir_y, float* out_reflect_dir_x,
                                        float* out_reflect_dir_y)
{
    if (!p || !pc)
        return 0;
    int can_deflect = 0;
    if (pc->parry_active)
        can_deflect = 1;
    if (pc->riposte_ready)
        can_deflect = 1;
    if (!can_deflect)
        return 0;
    float fdx = 0, fdy = 0;
    switch (p->facing)
    {
    case 0:
        fdy = 1;
        break;
    case 1:
        fdx = -1;
        break;
    case 2:
        fdx = 1;
        break;
    case 3:
        fdy = -1;
        break;
    }
    if (out_reflect_dir_x)
        *out_reflect_dir_x = fdx;
    if (out_reflect_dir_y)
        *out_reflect_dir_y = fdy;
    (void) proj_dir_x;
    (void) proj_dir_y;
    return 1;
}
