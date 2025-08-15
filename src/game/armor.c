#include "game/armor.h"
#include "entities/player.h"

/* Simple static armor catalog (expand / externalize later) */
static const RogueArmorDef g_armor_defs[] = {
    {0,"Cloth Hood",0, 1.5f, 1,0,0,0,0, 0.0f, 1.05f},
    {1,"Leather Cap",1, 3.0f, 3,2,0,0,0, 1.0f, 1.00f},
    {2,"Iron Helm",  2, 6.5f, 7,4,0,0,0, 2.5f, 0.94f},
    {3,"Cloth Robe", 0, 4.0f, 2,0,2,0,3, 0.0f, 1.06f},
    {4,"Leather Jerkin",1,7.5f,5,3,0,0,0, 2.0f, 1.00f},
    {5,"Iron Cuirass",2,12.0f,12,6,0,0,0,4.0f,0.92f},
    {6,"Cloth Pants",0,2.5f,1,0,0,0,0,0.0f,1.05f},
    {7,"Leather Greaves",1,5.0f,4,2,0,0,0,1.5f,1.00f},
    {8,"Iron Legplates",2,10.0f,9,4,0,0,0,3.0f,0.93f},
    {9,"Cloth Gloves",0,0.8f,0,0,0,0,0,0.0f,1.05f},
    {10,"Leather Gloves",1,1.5f,1,1,0,0,0,0.5f,1.01f},
    {11,"Iron Gauntlets",2,3.5f,3,2,0,0,0,1.5f,0.97f},
    {12,"Cloth Boots",0,1.2f,0,0,0,0,0,0.0f,1.05f},
    {13,"Leather Boots",1,2.2f,1,1,0,0,0,0.5f,1.01f},
    {14,"Iron Sabatons",2,4.2f,3,2,0,0,0,1.5f,0.96f},
};
static const int g_armor_count = (int)(sizeof(g_armor_defs)/sizeof(g_armor_defs[0]));

static int g_equipped[ROGUE_ARMOR_SLOT_COUNT] = {-1,-1,-1,-1,-1};

const RogueArmorDef* rogue_armor_get(int id){
    if(id<0) return 0; for(int i=0;i<g_armor_count;i++){ if(g_armor_defs[i].id==id) return &g_armor_defs[i]; } return 0;
}
void rogue_armor_equip_slot(int slot, int armor_id){ if(slot<0||slot>=ROGUE_ARMOR_SLOT_COUNT) return; g_equipped[slot]=armor_id; }
int rogue_armor_get_slot(int slot){ if(slot<0||slot>=ROGUE_ARMOR_SLOT_COUNT) return -1; return g_equipped[slot]; }

void rogue_armor_recalc_player(struct RoguePlayer* p){
    if(!p) return; 
    int total_armor_add=0; int total_phys_res=0,total_fire=0,total_frost=0,total_arc=0; float total_weight=0; float poise_bonus=0; float regen_mult=1.0f;
    for(int s=0;s<ROGUE_ARMOR_SLOT_COUNT;s++){
        int aid = g_equipped[s]; if(aid<0) continue; const RogueArmorDef* d = rogue_armor_get(aid); if(!d) continue;
        total_weight += d->weight; total_armor_add += d->armor; total_phys_res += d->resist_physical; total_fire += d->resist_fire; total_frost += d->resist_frost; total_arc += d->resist_arcane; poise_bonus += d->poise_bonus; regen_mult *= d->stamina_regen_mult;
    }
    p->encumbrance = total_weight; /* capacity unchanged; recalc tier */
    p->armor += total_armor_add; p->resist_physical += total_phys_res; p->resist_fire += total_fire; p->resist_frost += total_frost; p->resist_arcane += total_arc; p->poise_max += poise_bonus; if(p->poise > p->poise_max) p->poise = p->poise_max;
    /* Stamina regen multiplier stored indirectly: we'll scale in combat update by tier * gear regen_mult. For simplicity store on unused field (cc_slow_pct) */
    p->cc_slow_pct = regen_mult; /* reuse existing float (documented) */
    rogue_player_recalc_derived(p);
}
