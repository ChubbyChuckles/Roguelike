/* Phase 7.2/7.3 extensions test (public API based)
   Validates extended skill state fields (cast/channel/charges/casting_active/channel_active)
   persist across save/load. This complements the broader roundtrip test by focusing purely on the
   extended field set and ensuring non-zero values survive while legacy path (not exercised here)
   remains unaffected. */

#include "core/app_state.h"
#include "core/buffs.h"
#include "core/save_manager.h"
#include "core/skills.h"
#include <stdio.h>
#include <string.h>

static int fail = 0;
#define REQUIRE(cond, msg)                                                                         \
    do                                                                                             \
    {                                                                                              \
        if (!(cond))                                                                               \
        {                                                                                          \
            printf("PH7_EXT_FAIL %s line %d: %s\n", __FILE__, __LINE__, msg);                      \
            fail = 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    rogue_save_manager_reset_for_tests();
    rogue_save_manager_init();
    rogue_skills_init();
    rogue_buffs_init();
    extern void rogue_register_core_save_components(void);
    rogue_register_core_save_components();

    /* Ensure at least two skill defs exist */
    if (!rogue_skill_get_def(0))
    {
        RogueSkillDef def = {0};
        def.max_rank = 5;
        def.base_cooldown_ms = 1000;
        def.name = "E0";
        def.id = rogue_skill_register(&def);
        def.name = "E1";
        def.id = 1;
        rogue_skill_register(&def);
    }
    const RogueSkillDef* d0 = rogue_skill_get_def(0);
    const RogueSkillDef* d1 = rogue_skill_get_def(1);
    if (!d0 || !d1)
    {
        printf("PH7_EXT_FAIL defs_missing\n");
        return 1;
    }
    struct RogueSkillState* s0 = (struct RogueSkillState*) rogue_skill_get_state(0);
    struct RogueSkillState* s1 = (struct RogueSkillState*) rogue_skill_get_state(1);
    REQUIRE(s0 && s1, "states");

    /* Populate extended fields */
    s0->rank = 3;
    s0->cooldown_end_ms = 1234.0;
    s0->cast_progress_ms = 77.0;
    s0->channel_end_ms = 0.0;
    s0->next_charge_ready_ms = 2222.0;
    s0->charges_cur = 2;
    s0->casting_active = 1;
    s0->channel_active = 0;
    s1->rank = 2;
    s1->cooldown_end_ms = 4321.0;
    s1->cast_progress_ms = 0.0;
    s1->channel_end_ms = 5555.0;
    s1->next_charge_ready_ms = 0.0;
    s1->charges_cur = 1;
    s1->casting_active = 0;
    s1->channel_active = 1;

    if (rogue_save_manager_save_slot(0) != 0)
    {
        printf("PH7_EXT_FAIL save\n");
        return 1;
    }

    /* Zero out to confirm restoration */
    memset(s0, 0, sizeof *s0);
    memset(s1, 0, sizeof *s1);

    if (rogue_save_manager_load_slot(0) != 0)
    {
        printf("PH7_EXT_FAIL load\n");
        return 1;
    }

    s0 = (struct RogueSkillState*) rogue_skill_get_state(0);
    s1 = (struct RogueSkillState*) rogue_skill_get_state(1);
    REQUIRE(s0->rank == 3, "rank0");
    REQUIRE(s0->cast_progress_ms == 77.0, "cast_progress0");
    REQUIRE(s0->charges_cur == 2, "charges0");
    REQUIRE(s0->casting_active == 1, "casting_active0");
    REQUIRE(s1->channel_end_ms == 5555.0, "channel_end1");
    REQUIRE(s1->channel_active == 1, "channel_active1");

    if (fail)
    {
        printf("PH7_EXT_FAIL one_or_more\n");
        return 1;
    }
    printf("PH7_EXT_OK rank0=%d charges0=%d channel1_end=%.0f\n", s0->rank, s0->charges_cur,
           s1->channel_end_ms);
    return 0;
}
