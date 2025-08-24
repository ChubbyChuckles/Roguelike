#include "../../src/core/app/app_state.h"
#include "../../src/core/persistence/persistence.h"
#include "../../src/entities/player.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Minimal fake of g_app (defined in app_state.c in production). For unit test we just allocate it
 * here. */
RogueAppState g_app;                    /* zero-initialized */
RoguePlayer g_exposed_player_for_stats; /* unused */

static int file_exists(const char* p)
{
    FILE* f = NULL;
    if ((f = fopen(p, "rb")) != NULL)
    {
        fclose(f);
        return 1;
    }
    return 0;
}

int main(void)
{
    const char* tmp_stats = "test_player_stats_tmp.cfg";
    const char* tmp_gen =
        "test_gen_params_tmp.cfg"; /* not used directly but set to avoid writing real file */
    remove(tmp_stats);
    remove(tmp_gen);
    rogue_persistence_set_paths(tmp_stats, tmp_gen);
    /* Init defaults */
    rogue_player_init(&g_app.player);
    g_app.unspent_stat_points = 0;
    g_app.stats_dirty = 0;
    /* Simulate progression */
    g_app.player.level = 3;
    g_app.player.xp = 42;
    g_app.player.xp_to_next = 99;
    g_app.player.strength = 17;
    g_app.player.dexterity = 9;
    g_app.player.vitality = 33;
    g_app.player.intelligence = 11;
    g_app.player.crit_chance = 12;
    g_app.player.crit_damage = 175;
    g_app.unspent_stat_points = 5;
    rogue_player_recalc_derived(&g_app.player);
    int expected_hp = g_app.player.health;
    int expected_mp = g_app.player.mana;
    rogue_persistence_save_player_stats();
    if (!file_exists(tmp_stats))
    {
        printf("save file missing\n");
        return 1;
    }
    /* Zero out fields then load and verify */
    g_app.player.level = 0;
    g_app.player.xp = 0;
    g_app.player.xp_to_next = 0;
    g_app.player.strength = 0;
    g_app.player.dexterity = 0;
    g_app.player.vitality = 0;
    g_app.player.intelligence = 0;
    g_app.player.crit_chance = 0;
    g_app.player.crit_damage = 0;
    g_app.unspent_stat_points = 0;
    g_app.player.health = 0;
    g_app.player.mana = 0;
    rogue_persistence_load_player_stats();
    if (g_app.player.level != 3 || g_app.player.xp != 42 || g_app.player.xp_to_next != 99)
    {
        printf("basic progression fields mismatch\n");
        return 1;
    }
    if (g_app.player.strength != 17 || g_app.player.dexterity != 9 || g_app.player.vitality != 33 ||
        g_app.player.intelligence != 11)
    {
        printf("core stats mismatch\n");
        return 1;
    }
    if (g_app.player.crit_chance != 12 || g_app.player.crit_damage != 175)
    {
        printf("crit stats mismatch\n");
        return 1;
    }
    if (g_app.unspent_stat_points != 5)
    {
        printf("unspent mismatch\n");
        return 1;
    }
    if (g_app.player.health != expected_hp || g_app.player.mana != expected_mp)
    {
        printf("hp/mp mismatch after load\n");
        return 1;
    }
    remove(tmp_stats);
    remove(tmp_gen);
    return 0;
}
