#include "game/combat.h"
#include <math.h>

/* Advanced positional & timing bonuses (backstab, parry/riposte, guard break) */
int rogue_combat_try_backstab(RoguePlayer* p, RoguePlayerCombat* pc, RogueEnemy* e)
{
    if (!p || !pc || !e)
        return 0;
    if (!e->alive)
        return 0;
    if (pc->backstab_cooldown_ms > 0)
        return 0;
    float ex = e->base.pos.x, ey = e->base.pos.y;
    float px = p->base.pos.x, py = p->base.pos.y;
    float dx = px - ex;
    float dy = py - ey;
    float dist2 = dx * dx + dy * dy;
    if (dist2 > 2.25f)
        return 0;
    float fdx = (e->facing == 1) ? -1.0f : (e->facing == 2) ? 1.0f : 0.0f;
    float fdy = (e->facing == 0) ? 1.0f : (e->facing == 3) ? -1.0f : 0.0f;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.0001f)
        return 0;
    dx /= len;
    dy /= len;
    float dot = dx * fdx + dy * fdy;
    if (dot > -0.70f)
        return 0;
    pc->backstab_cooldown_ms = 650.0f;
    pc->backstab_pending_mult = 1.75f;
    return 1;
}
void rogue_player_begin_parry(RoguePlayer* p, RoguePlayerCombat* pc)
{
    if (!p || !pc)
        return;
    if (pc->parry_active)
        return;
    pc->parry_active = 1;
    pc->parry_timer_ms = 0.0f;
}
int rogue_player_is_parry_active(const RoguePlayerCombat* pc)
{
    return (pc && pc->parry_active) ? 1 : 0;
}
int rogue_player_register_incoming_attack_parry(RoguePlayer* p, RoguePlayerCombat* pc,
                                                float attack_dir_x, float attack_dir_y)
{
    if (!p || !pc)
        return 0;
    if (!pc->parry_active)
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
    float alen = sqrtf(attack_dir_x * attack_dir_x + attack_dir_y * attack_dir_y);
    if (alen > 0.0001f)
    {
        attack_dir_x /= alen;
        attack_dir_y /= alen;
    }
    float dot = fdx * attack_dir_x + fdy * attack_dir_y;
    if (dot < 0.10f)
        return 0;
    pc->parry_active = 0;
    pc->riposte_ready = 1;
    pc->riposte_window_ms = 650.0f;
    p->iframes_ms = 350.0f;
    p->riposte_ms = pc->riposte_window_ms;
    return 1;
}
int rogue_player_try_riposte(RoguePlayer* p, RoguePlayerCombat* pc, RogueEnemy* e)
{
    if (!p || !pc || !e)
        return 0;
    if (!pc->riposte_ready)
        return 0;
    if (!e->alive)
        return 0;
    pc->riposte_ready = 0;
    p->riposte_ms = 0.0f;
    pc->riposte_pending_mult = 2.25f;
    return 1;
}
void rogue_player_set_guard_break(RoguePlayer* p, RoguePlayerCombat* pc)
{
    (void) p;
    if (!pc)
        return;
    pc->guard_break_ready = 1;
    pc->riposte_ready = 1;
    pc->riposte_window_ms = 800.0f;
    pc->guard_break_pending_mult = 1.50f;
    pc->force_crit_next_strike = 1;
}
int rogue_player_consume_guard_break_bonus(RoguePlayerCombat* pc)
{
    if (!pc)
        return 0;
    if (!pc->guard_break_ready)
        return 0;
    pc->guard_break_ready = 0;
    return 1;
}
float rogue_combat_peek_backstab_mult(const RoguePlayerCombat* pc)
{
    return pc ? pc->backstab_pending_mult : 1.0f;
}
