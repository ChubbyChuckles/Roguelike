#include "core/crafting_automation.h"
#include "core/crafting.h"
#include "core/material_registry.h"
#include "core/material_refine.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static int ensure_recipe(void){ return rogue_craft_recipe_count()>0?0:-1; }

int main(void){
    ensure_recipe();
    const RogueCraftRecipe* r = rogue_craft_recipe_at(0);
    RogueCraftPlan p1, p2; rogue_craft_plan_build(r,2,&p1); rogue_craft_plan_build(r,2,&p2);
    assert(p1.entry_count==p2.entry_count);
    for(int i=0;i<p1.entry_count;i++){ if(p1.entries[i].missing<0){ fprintf(stderr,"P9_FAIL negative_missing\n"); return 10; } assert(p1.entries[i].def_index==p2.entries[i].def_index); assert(p1.entries[i].missing==p2.entries[i].missing); }
    RogueCraftRecipe a={0}; strcpy(a.id,"a"); a.output_qty=2; a.input_count=1; a.inputs[0].quantity=1;
    RogueCraftRecipe b={0}; strcpy(b.id,"b"); b.output_qty=6; b.input_count=2; b.inputs[0].quantity=2; b.inputs[1].quantity=2;
    int sa = rogue_craft_decision_score(&a); int sb = rogue_craft_decision_score(&b); if(sb < sa){ fprintf(stderr,"P9_FAIL decision_score %d<%d\n", sb, sa); return 11; }
    rogue_material_registry_load_default(); rogue_material_quality_reset(); if(rogue_material_count()>0){ for(int i=0;i<20;i++) rogue_material_quality_add(0,0,1); RogueRefineSuggestion rs[4]; int rc = rogue_craft_suggest_refine(rs,4); if(rc<=0){ fprintf(stderr,"P9_FAIL refine_suggest none\n"); return 12; } for(int i=0;i<rc;i++){ if(!(rs[i].from_quality < rs[i].to_quality) || rs[i].consume_count<=0){ fprintf(stderr,"P9_FAIL refine_invalid\n"); return 13; } } }
    char buf1[128]; char buf2[128]; rogue_craft_idle_recommendation(buf1,sizeof buf1); rogue_craft_idle_recommendation(buf2,sizeof buf2); if(strcmp(buf1,buf2)!=0){ fprintf(stderr,"P9_FAIL idle_non_deterministic %s | %s\n", buf1, buf2); return 14; }
    printf("CRAFT_P9_OK missing=%d score_a=%d score_b=%d idle='%s'\n", p1.total_missing, sa, sb, buf1);
    return 0; }
