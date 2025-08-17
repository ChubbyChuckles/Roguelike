#include <assert.h>
#include <math.h>
#include <string.h>
#include "../../src/ai/perception/perception.h"

/* Custom blocking predicate for LOS tests */
static int g_block_tx = -999, g_block_ty = -999; 
static int test_blocking(int tx,int ty){ return (tx==g_block_tx && ty==g_block_ty) ? 1 : 0; }

static void test_los(){
    rogue_perception_set_blocking_fn(test_blocking);
    g_block_tx = g_block_ty = -999; /* no block */
    assert(rogue_perception_los(1.5f,1.5f, 8.5f,8.5f) == 1);
    g_block_tx = 5; g_block_ty = 5; /* inject block near middle */
    assert(rogue_perception_los(1.5f,1.5f, 8.5f,8.5f) == 0);
    rogue_perception_set_blocking_fn(NULL); /* restore */
}

static void test_vision_cone(){
    RoguePerceptionAgent a = {0}; a.x=0; a.y=0; a.facing_x=1; a.facing_y=0; /* facing +X */
    /* Target at +X within FOV */
    assert(rogue_perception_can_see(&a, 5, 0, 120.0f, 10.0f, NULL)==1);
    /* Target at +Y outside FOV */
    assert(rogue_perception_can_see(&a, 0, 5, 120.0f, 10.0f, NULL)==0);
    /* Rotate facing to +Y */
    a.facing_x=0; a.facing_y=1; assert(rogue_perception_can_see(&a, 0, 5, 120.0f, 10.0f, NULL)==1);
}

static void test_threat_accumulation_and_decay(){
    RoguePerceptionAgent a = {0}; a.x=0; a.y=0; a.facing_x=1; a.facing_y=0; /* see player at +X */
    float visible_gain = 5.0f; float decay = 2.0f; 
    for(int i=0;i<10;i++){
        rogue_perception_tick_agent(&a, 0.1f, 5.0f, 0.0f, 140.0f, 20.0f, visible_gain, decay, 2.0f);
    }
    assert(a.threat > 0.0f);
    float threat_after_gain = a.threat;
    /* Move player out of FOV behind agent */
    for(int i=0;i<30;i++){
        rogue_perception_tick_agent(&a, 0.1f, -5.0f, 0.0f, 140.0f, 20.0f, visible_gain, decay, 2.0f);
    }
    assert(a.threat < threat_after_gain); /* decayed */
}

static void test_hearing_and_memory(){
    rogue_perception_events_reset();
    RoguePerceptionAgent a = {0}; a.x=0; a.y=0; a.facing_x=1; a.facing_y=0;
    /* Player far behind (not visible) */
    float player_x = -10.0f, player_y = 0.0f;
    rogue_perception_emit_sound(ROGUE_PERCEPTION_SOUND_ATTACK, player_x, player_y, 20.0f);
    int heard = rogue_perception_process_hearing(&a, player_x, player_y, 7.5f, 1.5f);
    assert(heard==1);
    assert(a.threat >= 7.5f - 0.001f);
    assert(a.has_last_seen==1);
    assert(fabsf(a.last_seen_x - player_x) < 0.01f);
}

static void test_group_broadcast(){
    RoguePerceptionAgent agents[3];
    memset(agents,0,sizeof(agents));
    agents[0].x=0; agents[0].y=0; agents[0].facing_x=1; agents[0].facing_y=0;
    agents[1].x=3; agents[1].y=0; agents[1].facing_x=1; agents[1].facing_y=0;
    agents[2].x=20; agents[2].y=0; agents[2].facing_x=1; agents[2].facing_y=0; /* out of broadcast radius */
    /* Source sees player */
    rogue_perception_tick_agent(&agents[0], 0.5f, 5.0f, 0.0f, 140.0f, 15.0f, 10.0f, 0.0f, 2.0f);
    float saved_threat = agents[0].threat;
    if(saved_threat >= 5.0f){
        agents[0].last_seen_x = 5.0f; agents[0].last_seen_y = 0.0f; agents[0].has_last_seen=1; agents[0].last_seen_ttl=2.0f;
        rogue_perception_broadcast_alert(agents, 3, 0, 10.0f, 3.0f, 2.0f);
        assert(agents[1].threat >= 3.0f);
        assert(agents[1].has_last_seen==1);
        assert(agents[2].threat < 0.01f); /* unaffected */
    } else {
        /* ensure test logic thresholds are met */
        assert(0 && "source threat below expected level for broadcast");
    }
}

int main(void){
    test_los();
    test_vision_cone();
    test_threat_accumulation_and_decay();
    test_hearing_and_memory();
    test_group_broadcast();
    return 0;
}
