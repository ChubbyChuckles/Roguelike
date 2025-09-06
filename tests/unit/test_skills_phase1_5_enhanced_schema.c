#include "../../src/content/json_envelope.h"
#include "../../src/content/json_io.h"
#include "../../src/content/schema_skills.h"
#include "../../src/core/skills/skills.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Lightweight harness: build several RogueSkillDef entries exercising extended Phase 1.1/1.2 fields
 * and validate via rogue_skills_validate_defs. Also inject deliberate negative cases to ensure
 * schema rejects out of range values (max_rank, frame_count, pierce_count, aoe_shape,
 * effect2_repeat_count timing).
 */
static RogueSkillDef make_base(const char* name, int max_rank)
{
    RogueSkillDef d;
    memset(&d, 0, sizeof d);
    d.name = name;
    d.icon = "test.png";
    d.max_rank = max_rank;
    d.skill_strength = 1;
    d.base_cooldown_ms = 100;
    d.cooldown_reduction_ms_per_rank = 5;
    d.effect_spec_id = 1;
    d.cast_time_ms = 50;
    return d;
}

int main(void)
{
    RogueSkillDef ok[3];
    /* 0: Visual + audio + projectile + AoE + animation */
    ok[0] = make_base("P1_5_VisualAudio", 3);
    ok[0].skill_type = 2;
    ok[0].cast_sprite_sheet = "skills/fire/cast.png";
    ok[0].projectile_sprite = "skills/fire/proj.png";
    ok[0].impact_sprite = "skills/fire/impact.png";
    ok[0].aoe_sprite = "skills/fire/aoe.png";
    ok[0].frame_count = 8;
    ok[0].frame_duration_ms = 40;
    ok[0].animation_loops = 1;
    ok[0].grid_width = 4;
    ok[0].grid_height = 2;
    ok[0].cast_sound_id = "snd_fire_cast";
    ok[0].impact_sound_id = "snd_fire_impact";
    ok[0].loop_sound_id = "snd_fire_loop";
    ok[0].sound_volume = 80;
    ok[0].aoe_shape = 1;
    ok[0].aoe_radius = 2.5f;
    ok[0].aoe_angle = 0;
    ok[0].projectile_velocity = 12.0f;
    ok[0].trajectory_type = 2;
    ok[0].pierce_count = 2;
    ok[0].homing_strength = 0.5f;
    /* Add effect nodes (two) with timing */
    ok[0].effect_node_count = 2;
    ok[0].effect_nodes[0].effect_spec_id = 2;
    ok[0].effect_nodes[0].delay_ms = 150;
    ok[0].effect_nodes[0].repeat_count = 2;
    ok[0].effect_nodes[0].repeat_interval_ms = 200;
    ok[0].effect_nodes[0].duration_ms = 500;
    ok[0].effect_nodes[1].effect_spec_id = 3;
    ok[0].effect_nodes[1].delay_ms = 100;
    ok[0].effect_nodes[1].duration_ms = 300;
    /* 1: Minimal passive with only grid -> frame_count derived left 0 intentionally */
    ok[1] = make_base("P1_5_Passive", 1);
    ok[1].is_passive = 1;
    ok[1].skill_type = 9;
    ok[1].grid_width = 1;
    ok[1].grid_height = 1;
    /* 2: AoE cone + projectile scatter combo test */
    ok[2] = make_base("P1_5_AoEProjectile", 2);
    ok[2].aoe_shape = 2;
    ok[2].aoe_radius = 4.f;
    ok[2].aoe_angle = 60.f;
    ok[2].trajectory_type = 3;
    ok[2].pierce_count = 0;
    ok[2].projectile_velocity = 6.0f;

    RogueSchemaValidationResult vr = {0};
    if (!rogue_skills_validate_defs(ok, 3, &vr))
    {
        fprintf(stderr, "FAIL enhanced_schema valid set rejected: %s\n",
                vr.error_count ? vr.errors[0].message : "unknown");
        return 1;
    }

    /* Negative cases */
    RogueSkillDef bad = make_base("Bad_Overflow", 11); /* max_rank > 10 */
    vr = (RogueSchemaValidationResult){0};
    if (rogue_skills_validate_defs(&bad, 1, &vr))
    {
        fprintf(stderr, "FAIL expected rejection for max_rank>10\n");
        return 2;
    }
    RogueSkillDef bad_frame = make_base("Bad_Frame", 2);
    bad_frame.frame_count = 5000; /* exceeds 1024 */
    vr = (RogueSchemaValidationResult){0};
    if (rogue_skills_validate_defs(&bad_frame, 1, &vr))
    {
        fprintf(stderr, "FAIL expected rejection for frame_count>1024\n");
        return 3;
    }
    RogueSkillDef bad_pierce = make_base("Bad_Pierce", 2);
    bad_pierce.pierce_count = 99;
    vr = (RogueSchemaValidationResult){0};
    if (rogue_skills_validate_defs(&bad_pierce, 1, &vr))
    {
        fprintf(stderr, "FAIL expected rejection for pierce_count>32\n");
        return 4;
    }
    RogueSkillDef bad_aoe = make_base("Bad_AoEShape", 2);
    bad_aoe.aoe_shape = 6;
    vr = (RogueSchemaValidationResult){0};
    if (rogue_skills_validate_defs(&bad_aoe, 1, &vr))
    {
        fprintf(stderr, "FAIL expected rejection for aoe_shape>4\n");
        return 5;
    }
    /* NOTE: Cross-field rule (repeat_count>0 requires repeat_interval_ms>0) is enforced in
     * runtime validator (skills_validate.c) but not in schema JSON layer because repeat_interval_ms
     * field elides when 0 and schema cannot infer conditional requirement. Negative case skipped.
     */

    printf(
        "OK test_skills_phase1_5_enhanced_schema: validated extended fields and negative cases\n");
    return 0;
}
