/* Test adaptive haste: buff increases cast speed and channel tick frequency */
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "../../src/core/skills.h"
#include "../../src/core/app_state.h"
#include "../../src/core/buffs.h"

static int g_hits=0;
static int cb_cast(const RogueSkillDef* d, struct RogueSkillState* st, const RogueSkillCtx* ctx){ (void)d;(void)st;(void)ctx; g_hits++; return 1; }

static void advance(double start,double end){ for(double t=start;t<=end;t+=16.0){ rogue_skills_update(t);} }

int main(void){
    rogue_skills_init();
    g_app.talent_points=1;
    RogueSkillDef cast; memset(&cast,0,sizeof cast); cast.name="HasteTest"; cast.max_rank=1; cast.base_cooldown_ms=0; cast.on_activate=cb_cast; cast.cast_type=1; cast.cast_time_ms=400.0f;
    int id = rogue_skill_register(&cast); assert(rogue_skill_rank_up(id)==1);
    RogueSkillCtx ctx={0};
    ctx.now_ms=0.0; assert(rogue_skill_try_activate(id,&ctx)==1);
    advance(0,400);
    assert(g_hits==1);
    g_hits=0; /* reset for hasted scenario */
    /* Move timeline forward to avoid overlapping previous progress */
    ctx.now_ms=1000.0;
    ((struct RogueSkillState*)rogue_skill_get_state(id))->cooldown_end_ms = 0.0;
    rogue_buffs_apply(ROGUE_BUFF_POWER_STRIKE,25,1000.0,ctx.now_ms,ROGUE_BUFF_STACK_ADD,0); /* strong haste (clamped at 0.5 factor) */
    assert(rogue_skill_try_activate(id,&ctx)==1);
    advance(1000.0, 1000.0+230.0); /* should finish well before baseline 400ms */
    assert(g_hits==1); /* finished faster than baseline 400ms window */
    printf("HASTE_OK hits=%d\n", g_hits);
    rogue_skills_shutdown();
    return 0;
}
