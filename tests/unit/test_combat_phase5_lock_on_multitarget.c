#define SDL_MAIN_HANDLED
#include "../../src/entities/enemy.h"
#include "../../src/entities/player.h"
#include "../../src/game/lock_on.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* Utility: populate enemy at (x,y) */
static void set_enemy(RogueEnemy* e, float x, float y)
{
    memset(e, 0, sizeof *e);
    e->alive = 1;
    e->base.pos.x = x;
    e->base.pos.y = y;
    e->health = 100;
    e->max_health = 100;
}

/* Compute expected angular ordering (atan2) for cardinal placement (E,N,W,S) becomes angles
 * (0,pi/2,pi,-pi/2) -> sorted: -pi/2 (S), 0 (E), pi/2 (N), pi (W) */
int main()
{
    RoguePlayer player;
    memset(&player, 0, sizeof player);
    player.lock_on_radius = 8.0f;
    player.facing = 2; /* facing east bias */
    RogueEnemy enemies[4];
    set_enemy(&enemies[0], 1.5f, 0.0f);  /* East */
    set_enemy(&enemies[1], 0.0f, 1.5f);  /* North */
    set_enemy(&enemies[2], -1.5f, 0.0f); /* West */
    set_enemy(&enemies[3], 0.0f, -1.5f); /* South */

    if (!rogue_lockon_acquire(&player, enemies, 4))
    {
        printf("fail_acquire\n");
        return 1;
    }
    if (player.lock_on_target_index < 0 || player.lock_on_target_index > 3)
    {
        printf("fail_initial_index=%d\n", player.lock_on_target_index);
        return 2;
    }

    /* Build expected cycle order starting from whichever was acquired by rotating through sorted
     * angle ring. */
    int sorted[4] = {3, 0, 1, 2}; /* S,E,N,W */
    /* Locate acquired in sorted list */
    int pos = -1;
    for (int i = 0; i < 4; i++)
    {
        if (sorted[i] == player.lock_on_target_index)
        {
            pos = i;
            break;
        }
    }
    if (pos == -1)
    {
        printf("fail_acquired_not_in_sorted idx=%d\n", player.lock_on_target_index);
        return 3;
    }

    /* Forward cycles should traverse next indices in angular order (clockwise) respecting cooldown
     * ticks. */
    int sequence[5];
    sequence[0] = player.lock_on_target_index;
    int seq_len = 1;
    for (int step = 1; step <= 4; ++step)
    {
        /* need to clear cooldown artificially for deterministic test */
        player.lock_on_switch_cooldown_ms = 0;
        if (!rogue_lockon_cycle(&player, enemies, 4, 1))
        {
            printf("fail_cycle_step=%d\n", step);
            return 4;
        }
        sequence[seq_len++] = player.lock_on_target_index;
    }
    /* Expect that after 4 forward cycles we returned to original target and visited each exactly
     * once. */
    if (seq_len != 5 || sequence[0] != sequence[4])
    {
        printf("fail_sequence_len_or_wrap len=%d first=%d last=%d\n", seq_len, sequence[0],
               sequence[4]);
        return 5;
    }
    int seen[4] = {0};
    for (int i = 0; i < 4; i++)
    {
        int idx = sequence[i];
        if (idx < 0 || idx > 3)
        {
            printf("fail_seq_idx_range i=%d val=%d\n", i, idx);
            return 6;
        }
        if (seen[idx])
        {
            continue;
        }
        seen[idx] = 1;
    }
    int distinct = 0;
    for (int i = 0; i < 4; i++)
    {
        if (seen[i])
            distinct++;
    }
    if (distinct != 4)
    {
        printf("fail_distinct=%d\n", distinct);
        return 7;
    }

    /* Backward cycle test: go one backward from current (which equals start) and ensure index
     * matches previous in ring. */
    player.lock_on_switch_cooldown_ms = 0;
    if (!rogue_lockon_cycle(&player, enemies, 4, -1))
    {
        printf("fail_cycle_backward\n");
        return 8;
    }
    int expected_prev = sorted[(pos - 1 + 4) % 4];
    if (player.lock_on_target_index != expected_prev)
    {
        printf("fail_backward_expected=%d got=%d\n", expected_prev, player.lock_on_target_index);
        return 9;
    }

    printf("phase5_lock_on_multitarget: OK start=%d fwd_seq=%d,%d,%d,%d,%d back=%d\n", sequence[0],
           sequence[0], sequence[1], sequence[2], sequence[3], sequence[4],
           player.lock_on_target_index);
    return 0;
}
