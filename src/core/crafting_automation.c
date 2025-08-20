/* Phase 9 Automation & Smart Assist Implementation */
#include <string.h>
#include <stdio.h>
#include "core/crafting_automation.h"
#include "core/crafting.h"
#include "core/inventory.h"
#include "core/gathering.h"
#include "core/material_registry.h"
#include "core/material_refine.h"

int rogue_craft_plan_build(const struct RogueCraftRecipe* recipe, int batch_qty, RogueCraftPlan* out){
    if(!recipe||!out||batch_qty<=0) return -1; memset(out,0,sizeof *out); int ec=0; int total_missing=0; for(int i=0;i<recipe->input_count && ec<8;i++){ int def = recipe->inputs[i].def_index; int need = recipe->inputs[i].quantity * batch_qty; int have = rogue_inventory_get_count(def); int miss = need - have; if(miss<0) miss=0; out->entries[ec].def_index=def; out->entries[ec].needed=need; out->entries[ec].have=have; out->entries[ec].missing=miss; total_missing += miss; ec++; } out->entry_count=ec; out->total_missing=total_missing; return 0; }

int rogue_craft_suggest_gather_routes(const RogueCraftPlan* plan, int max_indices, int* out_node_def_indices){
    if(!plan||!out_node_def_indices||max_indices<=0) return 0; int node_count = rogue_gather_def_count(); if(node_count<=0) return 0; struct NodeScore { int idx; int score; } tmp[256]; if(node_count>256) node_count=256; int tc=0; for(int n=0;n<node_count;n++){ const RogueGatherNodeDef* nd = rogue_gather_def_at(n); if(!nd) continue; int score=0; for(int m=0;m<plan->entry_count;m++){ int mat_def = plan->entries[m].def_index; if(plan->entries[m].missing<=0) continue; for(int k=0;k<nd->material_count;k++){ if(nd->material_defs[k]==mat_def){ score += nd->material_weights[k] * plan->entries[m].missing; } } } if(score>0){ tmp[tc].idx=n; tmp[tc].score=score; tc++; } }
    for(int i=1;i<tc;i++){ struct NodeScore v=tmp[i]; int j=i-1; while(j>=0 && (tmp[j].score < v.score || (tmp[j].score==v.score && tmp[j].idx>v.idx))){ tmp[j+1]=tmp[j]; j--; } tmp[j+1]=v; }
    int outc = tc<max_indices?tc:max_indices; for(int i=0;i<outc;i++) out_node_def_indices[i]=tmp[i].idx; return outc; }

int rogue_craft_suggest_refine(RogueRefineSuggestion* out, int max){
    if(!out||max<=0) return 0; int mat_count = rogue_material_count(); int sc=0; for(int m=0;m<mat_count && sc<max;m++){ const RogueMaterialDef* md = rogue_material_get(m); if(!md) continue; for(int q=0;q<=90 && sc<max; q+=10){ int c = rogue_material_quality_count(m,q); if(c>=10){ int higher = rogue_material_quality_count(m,q+10); if(higher < c/2){ RogueRefineSuggestion* rs=&out[sc++]; rs->material_def=m; rs->from_quality=q; rs->to_quality=q+10; rs->consume_count=c/2; if(rs->consume_count<=0) sc--; } } } }
    return sc; }

int rogue_craft_decision_score(const struct RogueCraftRecipe* recipe){ if(!recipe) return 0; int inputs=0; for(int i=0;i<recipe->input_count;i++) inputs += recipe->inputs[i].quantity; return recipe->output_qty - inputs; }

int rogue_craft_idle_recommendation(char* buf, int buf_sz){ if(!buf||buf_sz<=0) return -1; const struct RogueCraftRecipe* best=NULL; int best_missing=0; for(int i=0;i<rogue_craft_recipe_count();i++){ const struct RogueCraftRecipe* r=rogue_craft_recipe_at(i); if(!r) continue; RogueCraftPlan plan; if(rogue_craft_plan_build(r,1,&plan)!=0) continue; if(plan.total_missing>best_missing){ best=r; best_missing=plan.total_missing; } }
    if(!best || best_missing==0){ RogueRefineSuggestion rs[1]; int n=rogue_craft_suggest_refine(rs,1); if(n>0){ const RogueMaterialDef* md = rogue_material_get(rs[0].material_def); return snprintf(buf,buf_sz,"Refine %d units %s q%d->q%d", rs[0].consume_count, md?md->id:"mat", rs[0].from_quality, rs[0].to_quality); } return snprintf(buf,buf_sz,"Idle: nothing urgent"); }
    RogueCraftPlan plan; rogue_craft_plan_build(best,1,&plan); int nodes[3]; int nc = rogue_craft_suggest_gather_routes(&plan,3,nodes); if(nc>0){ const RogueGatherNodeDef* nd = rogue_gather_def_at(nodes[0]); return snprintf(buf,buf_sz,"Gather for %s via node %s (missing %d)", best->id, nd?nd->id:"node", best_missing); }
    return snprintf(buf,buf_sz,"Gather materials for %s (missing %d)", best->id, best_missing); }
