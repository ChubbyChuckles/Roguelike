#include "ui/crafting_ui.h"
#include "core/crafting.h"
#include "core/crafting_queue.h"
#include "core/material_refine.h"
#include "core/material_registry.h"
#include "core/inventory.h"
#include "core/equipment.h"
#include "core/loot_instances.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static char g_search[32];
static int  g_text_only=0;

void rogue_crafting_ui_set_search(const char* s){ if(!s){ g_search[0]='\0'; return;} size_t n=strlen(s); if(n>=sizeof g_search) n=sizeof g_search-1; memcpy(g_search,s,n); g_search[n]='\0'; }
const char* rogue_crafting_ui_last_search(void){ return g_search; }
void rogue_crafting_ui_set_text_only(int e){ g_text_only = e?1:0; }
int  rogue_crafting_ui_text_only(void){ return g_text_only; }

static int recipe_visible(const RogueCraftRecipe* r){ if(!g_search[0]) return 1; if(!r) return 0; if(strstr(r->id,g_search)) return 1; const RogueItemDef* d=rogue_item_def_at(r->output_def); if(d && strstr(d->id,g_search)) return 1; return 0; }

/* Simple heuristic: recipe upgrades currently equipped weapon/armor if output rarity > existing slot rarity. */
static int recipe_is_upgrade(const RogueCraftRecipe* r){ /* Heuristic: treat higher rarity weapon recipe as upgrade if any equipped weapon of lower rarity exists. */
    if(!r) return 0; const RogueItemDef* out_def=rogue_item_def_at(r->output_def); if(!out_def) return 0; if(out_def->category!=ROGUE_ITEM_WEAPON) return 0; /* limit scope */
    /* Scan equipped weapon slot */
    int inst_index = rogue_equip_get(ROGUE_EQUIP_WEAPON); if(inst_index<0) return 0; const RogueItemInstance* inst = rogue_item_instance_at(inst_index); if(!inst) return 0; const RogueItemDef* cur_def = rogue_item_def_at(inst->def_index); if(!cur_def) return 0; return out_def->rarity > cur_def->rarity; }

int rogue_crafting_ui_render_panel(RogueUIContext* ctx, float x, float y, float w, float h){ if(!ctx) return 0; int rendered=0; float line_h=14.0f; float cy=y; int rc=rogue_craft_recipe_count(); for(int i=0;i<rc;i++){ const RogueCraftRecipe* r=rogue_craft_recipe_at(i); if(!r) continue; if(!recipe_visible(r)) continue; const RogueItemDef* d=rogue_item_def_at(r->output_def); int can_craft=1; for(int j=0;j<r->input_count;j++){ int have = rogue_inventory_get_count(r->inputs[j].def_index); if(have < r->inputs[j].quantity){ can_craft=0; break; } }
    char line[160]; char tag[8]=""; if(recipe_is_upgrade(r)) { const char* up="[UP] "; for(int k=0;k<7 && up[k];k++){ if(k<(int)sizeof(tag)-1) tag[k]=up[k]; } tag[sizeof(tag)-1]='\0'; }
    snprintf(line,sizeof line,"%s%s x%d %s%s", tag, d?d->id:"?", r->output_qty, can_craft?"":"(MISSING)", g_text_only?"":"");
        uint32_t color = can_craft?0xFFFFFFFFu:0x777777FFu; if(!g_text_only) rogue_ui_text(ctx,(RogueUIRect){x,cy,w,line_h}, line, color); else fprintf(stdout,"CRAFT_UI:%s\n", line);
        cy += line_h; if(cy > y+h-line_h) break; rendered++; }
    return rendered; }

float rogue_crafting_ui_expected_fracture_damage(int intensity){ /* success 80%, failure 20%; failure durability damage = 5+intensity */ float fail_p=0.20f; float dmg=(float)(5+intensity); return fail_p * dmg; }

int rogue_crafting_ui_render_enhancement_risk(RogueUIContext* ctx, float x, float y, int intensity){ if(!ctx) return -1; char buf[96]; float expect=rogue_crafting_ui_expected_fracture_damage(intensity); snprintf(buf,sizeof buf,"Temper Intensity %d: Success 80%% | Fail 20%% | Exp Fracture Dmg %.2f", intensity, expect); if(!g_text_only) rogue_ui_text(ctx,(RogueUIRect){x,y,400,14}, buf, 0xFFA060FFu); else fprintf(stdout,"CRAFT_RISK:%s\n", buf); return 0; }

int rogue_crafting_ui_render_material_ledger(RogueUIContext* ctx, float x, float y, float w, float h){ if(!ctx) return 0; int count=0; float line_h=14.0f; float cy=y; int mcap=rogue_material_count(); for(int i=0;i<mcap;i++){ const RogueMaterialDef* md=rogue_material_get(i); if(!md) continue; int total=rogue_material_quality_total(i); if(total<=0) continue; int avg=rogue_material_quality_average(i); float bias=rogue_material_quality_bias(i); char line[160]; snprintf(line,sizeof line,"%s total=%d avg=%d bias=%.2f", md->id, total, avg, bias); if(!g_text_only) rogue_ui_text(ctx,(RogueUIRect){x,cy,w,line_h}, line, 0xC0FF90FFu); else fprintf(stdout,"CRAFT_LEDGER:%s\n", line); cy+=line_h; if(cy>y+h-line_h) break; count++; }
    return count; }
