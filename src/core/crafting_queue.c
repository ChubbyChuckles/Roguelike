/* Crafting Queue Implementation (Phase 4.3–4.5) */
#include "core/crafting_queue.h"
#include <string.h>

#ifndef ROGUE_CRAFT_JOB_CAP
#define ROGUE_CRAFT_JOB_CAP 256
#endif

static RogueCraftJob g_jobs[ROGUE_CRAFT_JOB_CAP];
static int g_job_count=0; static int g_next_id=1; /* 0 reserved */

static int station_caps[ROGUE_CRAFT_STATION__COUNT] = {2,2,2,1}; /* base capacities */

int rogue_craft_station_id(const char* tag){ if(!tag) return -1; if(strcmp(tag,"forge")==0) return ROGUE_CRAFT_STATION_FORGE; if(strcmp(tag,"alchemy_table")==0||strcmp(tag,"alchemy")==0) return ROGUE_CRAFT_STATION_ALCHEMY; if(strcmp(tag,"workbench")==0) return ROGUE_CRAFT_STATION_WORKBENCH; if(strcmp(tag,"mystic_altar")==0||strcmp(tag,"altar")==0) return ROGUE_CRAFT_STATION_ALTAR; return -1; }
int rogue_craft_station_capacity(int station_id){ if(station_id<0||station_id>=ROGUE_CRAFT_STATION__COUNT) return 0; return station_caps[station_id]; }

void rogue_craft_queue_reset(void){ g_job_count=0; g_next_id=1; }
int  rogue_craft_queue_job_count(void){ return g_job_count; }
int  rogue_craft_queue_active_count(int station_id){ int c=0; for(int i=0;i<g_job_count;i++){ if(g_jobs[i].station==station_id && g_jobs[i].state==1) c++; } return c; }
const RogueCraftJob* rogue_craft_queue_job_at(int index){ if(index<0||index>=g_job_count) return NULL; return &g_jobs[index]; }

static void try_activate_waiting(void){
    for(int s=0;s<ROGUE_CRAFT_STATION__COUNT;s++){
        int cap = rogue_craft_station_capacity(s); int active = rogue_craft_queue_active_count(s);
        if(active>=cap) continue;
        for(int i=0;i<g_job_count && active<cap;i++){
            if(g_jobs[i].station==s && g_jobs[i].state==0){ g_jobs[i].state=1; active++; }
        }
    }
}

int rogue_craft_queue_enqueue(const RogueCraftRecipe* recipe, int recipe_index,
    int current_skill,
    RogueInvGetFn inv_get, RogueInvConsumeFn inv_consume){
    if(!recipe || recipe_index<0 || !inv_get || !inv_consume) return -1; if(recipe->output_def<0) return -2;
    if(current_skill < recipe->skill_req) return -3;
    /* check materials */
    for(int i=0;i<recipe->input_count;i++){ const RogueCraftIngredient* ing=&recipe->inputs[i]; if(inv_get(ing->def_index) < ing->quantity) return -4; }
    for(int i=0;i<recipe->input_count;i++){ const RogueCraftIngredient* ing=&recipe->inputs[i]; if(inv_consume(ing->def_index, ing->quantity) < ing->quantity) return -5; }
    if(g_job_count>=ROGUE_CRAFT_JOB_CAP) return -6;
    RogueCraftJob* j=&g_jobs[g_job_count++]; j->id=g_next_id++; j->recipe_index=recipe_index; j->station= rogue_craft_station_id(recipe->station); if(j->station<0) j->station=ROGUE_CRAFT_STATION_WORKBENCH; j->total_ms = recipe->time_ms>0? recipe->time_ms: 1; j->remaining_ms=j->total_ms; j->state=0; /* waiting -> activation pass */
    try_activate_waiting();
    return j->id;
}

void rogue_craft_queue_update(int delta_ms, RogueInvAddFn inv_add){ if(delta_ms<=0||!inv_add) return; for(int i=0;i<g_job_count;i++){ RogueCraftJob* j=&g_jobs[i]; if(j->state==1){ j->remaining_ms -= delta_ms; if(j->remaining_ms<=0){ j->state=2; j->remaining_ms=0; /* completion recorded; output deferred until after loop */ } } }
    /* issue outputs after marking completion */
    for(int i=0;i<g_job_count;i++){ RogueCraftJob* j=&g_jobs[i]; if(j->state==2){ const RogueCraftRecipe* r = rogue_craft_recipe_at(j->recipe_index); if(r){ inv_add(r->output_def, r->output_qty); } }
    }
    try_activate_waiting();
}

int rogue_craft_queue_cancel(int job_id, const RogueCraftRecipe* recipe, RogueInvAddFn inv_add){ if(job_id<=0||!recipe||!inv_add) return -1; for(int i=0;i<g_job_count;i++){ RogueCraftJob* j=&g_jobs[i]; if(j->id==job_id){ if(j->state==2||j->state==3) return -2; int full = (j->state==0); j->state=3; /* canceled */ for(int k=0;k<recipe->input_count;k++){ int qty = recipe->inputs[k].quantity; if(!full){ qty = qty/2; } if(qty>0) inv_add(recipe->inputs[k].def_index, qty); } try_activate_waiting(); return 0; } } return -3; }
