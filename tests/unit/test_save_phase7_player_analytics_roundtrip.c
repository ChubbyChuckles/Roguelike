#include "core/app_state.h"
#include "core/save_manager.h"
#include "entities/player.h"
#include <stdio.h>
#include <string.h>

static int fail = 0;
#define CHECK(c, msg)                                                                              \
    do                                                                                             \
    {                                                                                              \
        if (!(c))                                                                                  \
        {                                                                                          \
            printf("FAIL:%s line %d %s\n", __FILE__, __LINE__, msg);                               \
            fail = 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    rogue_save_manager_reset_for_tests();
    rogue_save_manager_init();
    extern void rogue_register_core_save_components(void);
    rogue_register_core_save_components();
    g_app.player.level = 7;
    g_app.player.xp = 345;
    g_app.player.xp_to_next = 500;
    g_app.player.health = 777;
    g_app.player.mana = 222;
    g_app.player.action_points = 150;
    g_app.player.strength = 11;
    g_app.player.dexterity = 13;
    g_app.player.vitality = 21;
    g_app.player.intelligence = 9;
    g_app.talent_points = 6;
    g_app.analytics_damage_dealt_total = 123456ULL;
    g_app.analytics_gold_earned_total = 7890ULL;
    g_app.permadeath_mode = 1;
    if (rogue_save_manager_save_slot(0) != 0)
    {
        printf("FAIL: save\n");
        return 1;
    }
    /* wipe */ memset(&g_app.player, 0, sizeof g_app.player);
    g_app.talent_points = 0;
    g_app.analytics_damage_dealt_total = 0;
    g_app.analytics_gold_earned_total = 0;
    g_app.permadeath_mode = 0;
    if (rogue_save_manager_load_slot(0) != 0)
    {
        printf("FAIL: load\n");
        return 1;
    }
    CHECK(g_app.player.level == 7, "level");
    CHECK(g_app.player.xp == 345, "xp");
    CHECK(g_app.player.xp_to_next == 500, "xp_to_next");
    CHECK(g_app.player.health == 777, "health");
    CHECK(g_app.player.mana == 222, "mana");
    CHECK(g_app.player.action_points == 150, "ap");
    CHECK(g_app.player.strength == 11, "str");
    CHECK(g_app.player.dexterity == 13, "dex");
    CHECK(g_app.player.vitality == 21, "vit");
    CHECK(g_app.player.intelligence == 9, "int");
    CHECK(g_app.talent_points == 6, "talent");
    CHECK(g_app.analytics_damage_dealt_total == 123456ULL, "dmg");
    CHECK(g_app.analytics_gold_earned_total == 7890ULL, "gold");
    CHECK(g_app.permadeath_mode == 1, "perm");
    if (fail)
    {
        printf("FAILURES\n");
        return 1;
    }
    printf("OK:save_phase7_player_analytics_roundtrip\n");
    return 0;
}
