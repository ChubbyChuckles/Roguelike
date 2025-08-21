#include "core/loot/loot_multiplayer.h"
#include "core/app_state.h"
#include "core/loot/loot_instances.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

static RogueLootMode g_mode = ROGUE_LOOT_MODE_SHARED;

void rogue_loot_set_mode(RogueLootMode m) { g_mode = m; }
RogueLootMode rogue_loot_get_mode(void) { return g_mode; }

int rogue_loot_assign_owner(int inst_index, int player_id)
{
    if (inst_index < 0 || inst_index >= g_app.item_instance_cap)
        return -1;
    RogueItemInstance* it = &g_app.item_instances[inst_index];
    if (!it->active)
        return -2;
    it->owner_player_id = (player_id >= 0 && g_mode == ROGUE_LOOT_MODE_PERSONAL) ? player_id : -1;
    return 0;
}

/* ---- Need / Greed Session Implementation ---- */
typedef struct NeedGreedSession
{
    int inst_index;
    int active; /* 1 if an unresolved session exists */
    int participants[8];
    uint8_t chose_mask; /* bit i -> participant i submitted */
    int rolls[8];       /* -1 until chosen or passed */
    uint8_t need_flags; /* bit i -> need chosen */
    uint8_t pass_flags; /* bit i -> passed */
    int participant_count;
    int winner_player_id; /* -1 until resolved */
    unsigned int rng_state;
} NeedGreedSession;

static NeedGreedSession g_needgreed_sessions[16]; /* small ring of concurrent sessions */

static NeedGreedSession* find_session(int inst_index)
{
    for (int i = 0; i < 16; i++)
        if (g_needgreed_sessions[i].active && g_needgreed_sessions[i].inst_index == inst_index)
            return &g_needgreed_sessions[i];
    return NULL;
}
static NeedGreedSession* allocate_session(void)
{
    for (int i = 0; i < 16; i++)
    {
        if (!g_needgreed_sessions[i].active)
        {
            memset(&g_needgreed_sessions[i], 0, sizeof(NeedGreedSession));
            g_needgreed_sessions[i].winner_player_id = -1;
            return &g_needgreed_sessions[i];
        }
    }
    return NULL;
}
static unsigned int rng_next(unsigned int* s)
{
    *s = (*s) * 1664525u + 1013904223u;
    return *s;
}

int rogue_loot_need_greed_begin(int inst_index, const int* player_ids, int count)
{
    if (inst_index < 0 || inst_index >= g_app.item_instance_cap || count <= 0 || count > 8)
        return -1;
    RogueItemInstance* it = &g_app.item_instances[inst_index];
    if (!it->active)
        return -2;
    if (find_session(inst_index))
        return -3; /* already active */
    NeedGreedSession* s = allocate_session();
    if (!s)
        return -4;
    s->inst_index = inst_index;
    s->active = 1;
    s->participant_count = count;
    for (int i = 0; i < count; i++)
    {
        s->participants[i] = player_ids[i];
        s->rolls[i] = -1;
    }
    s->rng_state = (unsigned int) (inst_index * 977 + (unsigned) time(NULL));
    return 0;
}

int rogue_loot_need_greed_choose(int inst_index, int player_id, int need_flag, int pass_flag)
{
    NeedGreedSession* s = find_session(inst_index);
    if (!s)
        return -1;
    for (int i = 0; i < s->participant_count; i++)
        if (s->participants[i] == player_id)
        {
            if (s->chose_mask & (1u << i))
                return -2; /* already chose */
            s->chose_mask |= (1u << i);
            if (pass_flag)
            {
                s->pass_flags |= (1u << i);
                s->rolls[i] = -1;
                return -3;
            }
            if (need_flag)
                s->need_flags |= (1u << i);
            unsigned int r = rng_next(&s->rng_state);
            int base = need_flag ? 700 : 400;
            s->rolls[i] = base + (int) (r % 300u); /* 700-999 need, 400-699 greed */
            return s->rolls[i];
        }
    return -4; /* player not in session */
}

static void determine_winner(NeedGreedSession* s)
{
    int best_idx = -1;
    int best_roll = -1;
    int any_need = s->need_flags != 0;
    for (int i = 0; i < s->participant_count; i++)
    {
        if (s->pass_flags & (1u << i))
            continue;
        int is_need = (s->need_flags & (1u << i)) != 0;
        if (any_need && !is_need)
            continue; /* greed ignored if any need */
        if (s->rolls[i] > best_roll)
        {
            best_roll = s->rolls[i];
            best_idx = i;
        }
    }
    if (best_idx >= 0)
    {
        s->winner_player_id = s->participants[best_idx];
    }
}

int rogue_loot_need_greed_resolve(int inst_index)
{
    NeedGreedSession* s = find_session(inst_index);
    if (!s)
        return -1;
    /* Must have all choices or force resolution */
    if (s->chose_mask != (uint8_t) ((1u << s->participant_count) - 1))
    {
        /* force pass for missing participants */
        for (int i = 0; i < s->participant_count; i++)
            if (!(s->chose_mask & (1u << i)))
            {
                s->pass_flags |= (1u << i);
            }
    }
    determine_winner(s);
    if (s->winner_player_id >= 0)
    {
        /* Force personal mode ownership if currently shared so winner retains protected pickup
         * before trade */
        RogueLootMode prev = g_mode;
        g_mode = ROGUE_LOOT_MODE_PERSONAL; /* temporarily ensure assignment */
        rogue_loot_assign_owner(inst_index, s->winner_player_id);
        g_mode = prev;
    }
    s->active = 0; /* mark finished to unlock */
    return s->winner_player_id;
}

int rogue_loot_need_greed_winner(int inst_index)
{
    NeedGreedSession* s = find_session(inst_index);
    if (!s)
        return -1;
    return s->winner_player_id;
}

int rogue_loot_instance_locked(int inst_index)
{
    NeedGreedSession* s = find_session(inst_index);
    return (s && s->active) ? 1 : 0;
}

/* Trade validation: reject while locked or inactive, ensure from_player has ownership (or
 * shared/unowned), simple cooldown placeholder */
int rogue_loot_trade_request(int inst_index, int from_player, int to_player)
{
    if (inst_index < 0 || inst_index >= g_app.item_instance_cap)
        return -1;
    if (from_player == to_player)
        return -2;
    RogueItemInstance* it = &g_app.item_instances[inst_index];
    if (!it->active)
        return -3;
    if (rogue_loot_instance_locked(inst_index))
        return -4; /* locked by need/greed */
    if (it->owner_player_id >= 0 && it->owner_player_id != from_player)
        return -5; /* not owned by requester */
    /* simple distance check placeholder: ensure within 10 tiles (future: actual player array) */
    /* Accept trade */
    it->owner_player_id = to_player;
    return 0;
}
