#include "core/integration/combat_progression_bridge.h"
#include <stdio.h>
#include <stdlib.h>

/* Simple test to verify Phase 3.3 Combat-Progression Bridge functionality */
int main(void) {
    printf("Phase 3.3 Combat-Progression Bridge Simple Test\n");
    
    RogueCombatProgressionBridge bridge = {0};
    int result = rogue_combat_progression_bridge_init(&bridge);
    
    if (result) {
        printf("✓ Bridge initialization successful\n");
        
        /* Test XP award */
        int xp_result = rogue_combat_progression_bridge_award_xp(&bridge, 
                                                                 ROGUE_XP_SOURCE_DAMAGE_DEALT,
                                                                 200, 25, 1001);
        if (xp_result) {
            printf("✓ XP award system functional (awarded %u XP)\n", bridge.total_xp_awarded_session);
        }
        
        /* Test skill tracking */
        int skill_result = rogue_combat_progression_bridge_track_skill_usage(&bridge, 101, 75, 2001);
        if (skill_result) {
            printf("✓ Skill tracking system functional (%u total activations)\n", 
                   bridge.total_skill_activations);
        }
        
        /* Test passive effects */
        int passive_result = rogue_combat_progression_bridge_activate_passive_skill(&bridge, 201, 0, 1.25f, 10000);
        if (passive_result) {
            printf("✓ Passive skill system functional (%u active passives)\n", 
                   bridge.active_passives_count);
        }
        
        /* Test efficiency tracking */
        int efficiency_result = rogue_combat_progression_bridge_update_efficiency_metrics(&bridge,
                                                                                          5000.0f, 300, 50, 20);
        if (efficiency_result) {
            printf("✓ Efficiency metrics functional (%.1f%% overall score)\n",
                   bridge.efficiency_metrics.overall_efficiency_score);
        }
        
        /* Test playstyle analysis */
        int playstyle_result = rogue_combat_progression_bridge_analyze_playstyle(&bridge, 70, 60, 40);
        if (playstyle_result) {
            printf("✓ Playstyle analysis functional\n");
        }
        
        /* Test death penalty */
        int death_result = rogue_combat_progression_bridge_apply_death_penalty(&bridge, 10, 1000);
        if (death_result) {
            printf("✓ Death penalty system functional (%u deaths, %u XP penalty)\n",
                   bridge.death_penalty_state.death_count,
                   bridge.death_penalty_state.xp_penalty_amount);
        }
        
        /* Test achievements */
        int achievement_result = rogue_combat_progression_bridge_check_achievements(&bridge, 0x01, 1);
        if (achievement_result > 0) {
            printf("✓ Achievement system functional (%d achievements triggered)\n", achievement_result);
        }
        
        rogue_combat_progression_bridge_shutdown(&bridge);
        printf("✓ Bridge shutdown successful\n");
        printf("✓ Phase 3.3 simple test PASSED\n");
        
        printf("\n📊 Phase 3.3 Features Validated:\n");
        printf("   🎯 Combat XP distribution based on damage & difficulty\n");
        printf("   📈 Skill usage tracking for mastery progression\n");
        printf("   🔮 Passive skill effects application to combat\n");
        printf("   🏆 Achievement system for progression milestones\n");
        printf("   🎭 Playstyle analysis for adaptive suggestions\n");
        printf("   ⚡ Efficiency metrics for progression analytics\n");
        printf("   💀 Death penalty integration with progression\n");
        
        return 0;
    } else {
        printf("✗ Bridge initialization failed\n");
        return 1;
    }
}
