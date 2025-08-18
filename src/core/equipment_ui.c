#include "core/equipment_ui.h"
#include "core/loot_tooltip.h" /* reuse simple builder for base lines */
#include "core/loot_instances.h"
#include "core/loot_item_defs.h"
#include "core/equipment_procs.h"
#include <string.h>
#include <stdio.h>

static int g_socket_sel_inst=-1, g_socket_sel_index=-1; /* ephemeral drag selection */
static int g_transmog_last[ROGUE_EQUIP__COUNT];
static int g_initialized=0;

static void ensure_init(void){ if(g_initialized) return; for(int i=0;i<ROGUE_EQUIP__COUNT;i++) g_transmog_last[i]=-1; g_initialized=1; }

static unsigned int fnv1a(const char* s){ unsigned int h=2166136261u; while(s&&*s){ h^=(unsigned char)*s++; h*=16777619u; } return h; }

char* rogue_equipment_panel_build(char* buf, size_t cap){ if(!buf||cap<8) return NULL; ensure_init();
    size_t off=0; int n=snprintf(buf+off,cap-off,"[Weapons]\n"); if(n<0||n>= (int)(cap-off)) return buf; off+=n;
    int w=rogue_equip_get(ROGUE_EQUIP_WEAPON); int oh=rogue_equip_get(ROGUE_EQUIP_OFFHAND);
    const RogueItemInstance* wit=rogue_item_instance_at(w); const RogueItemDef* wd = wit? rogue_item_def_at(wit->def_index):NULL;
    const RogueItemInstance* ohit=rogue_item_instance_at(oh); const RogueItemDef* ohd = ohit? rogue_item_def_at(ohit->def_index):NULL;
    n=snprintf(buf+off,cap-off,"Weapon: %s\n", wd? wd->name: "<empty>"); if(n<0||n>= (int)(cap-off)) return buf; off+=n;
    n=snprintf(buf+off,cap-off,"Offhand: %s\n\n", ohd? ohd->name: "<empty>"); if(n<0||n>= (int)(cap-off)) return buf; off+=n;
    n=snprintf(buf+off,cap-off,"[Armor]\n"); if(n<0||n>= (int)(cap-off)) return buf; off+=n;
    enum RogueEquipSlot armor_slots[]={ROGUE_EQUIP_ARMOR_HEAD,ROGUE_EQUIP_ARMOR_CHEST,ROGUE_EQUIP_ARMOR_LEGS,ROGUE_EQUIP_ARMOR_HANDS,ROGUE_EQUIP_ARMOR_FEET,ROGUE_EQUIP_CLOAK};
    for(int i=0;i<6;i++){ enum RogueEquipSlot s=armor_slots[i]; int inst=rogue_equip_get(s); const RogueItemInstance* it=rogue_item_instance_at(inst); const RogueItemDef* d= it? rogue_item_def_at(it->def_index):NULL; const char* label=""; switch(s){case ROGUE_EQUIP_ARMOR_HEAD: label="Head";break;case ROGUE_EQUIP_ARMOR_CHEST: label="Chest";break;case ROGUE_EQUIP_ARMOR_LEGS: label="Legs";break;case ROGUE_EQUIP_ARMOR_HANDS: label="Hands";break;case ROGUE_EQUIP_ARMOR_FEET: label="Feet";break;case ROGUE_EQUIP_CLOAK: label="Cloak";break; default: label="Slot";} n=snprintf(buf+off,cap-off,"%s: %s\n", label, d? d->name: "<empty>"); if(n<0||n>= (int)(cap-off)) return buf; off+=n; }
    if(off+2<cap){ buf[off++]='\n'; buf[off]='\0'; }
    n=snprintf(buf+off,cap-off,"[Jewelry]\n"); if(n<0||n>= (int)(cap-off)) return buf; off+=n;
    enum RogueEquipSlot jew[]={ROGUE_EQUIP_RING1,ROGUE_EQUIP_RING2,ROGUE_EQUIP_AMULET,ROGUE_EQUIP_BELT};
    const char* jnames[]={"Ring1","Ring2","Amulet","Belt"};
    for(int i=0;i<4;i++){ int inst=rogue_equip_get(jew[i]); const RogueItemInstance* it=rogue_item_instance_at(inst); const RogueItemDef* d= it? rogue_item_def_at(it->def_index):NULL; n=snprintf(buf+off,cap-off,"%s: %s\n", jnames[i], d? d->name: "<empty>"); if(n<0||n>= (int)(cap-off)) return buf; off+=n; }
    n=snprintf(buf+off,cap-off,"\n[Charms]\n"); if(n<0||n>= (int)(cap-off)) return buf; off+=n;
    enum RogueEquipSlot charms[]={ROGUE_EQUIP_CHARM1,ROGUE_EQUIP_CHARM2};
    for(int i=0;i<2;i++){ int inst=rogue_equip_get(charms[i]); const RogueItemInstance* it=rogue_item_instance_at(inst); const RogueItemDef* d= it? rogue_item_def_at(it->def_index):NULL; n=snprintf(buf+off,cap-off,"Charm%d: %s\n", i+1, d? d->name: "<empty>"); if(n<0||n>= (int)(cap-off)) return buf; off+=n; }
    /* Set progress (simple count by set_id) */
    int set_ids[16]; int set_counts[16]; int set_unique=0;
    for(int s=0;s<ROGUE_EQUIP__COUNT;s++){ int inst=rogue_equip_get((enum RogueEquipSlot)s); if(inst<0) continue; const RogueItemInstance* it=rogue_item_instance_at(inst); if(!it) continue; const RogueItemDef* d = rogue_item_def_at(it->def_index); if(!d||d->set_id<=0) continue; int found=-1; for(int k=0;k<set_unique;k++) if(set_ids[k]==d->set_id){ found=k; break; } if(found<0 && set_unique<16){ set_ids[set_unique]=d->set_id; set_counts[set_unique]=1; set_unique++; } else if(found>=0) set_counts[found]++; }
    if(off+16<cap){ int n2=snprintf(buf+off,cap-off,"\nSet Progress: "); if(n2>0 && n2 < (int)(cap-off)) off+=n2; }
    for(int i=0;i<set_unique;i++){ int n2=snprintf(buf+off,cap-off,"set_%d=%d ", set_ids[i], set_counts[i]); if(n2<0||n2>= (int)(cap-off)) break; off+=n2; }
    if(off<cap) buf[off]='\0'; return buf; }

char* rogue_equipment_compare_deltas(int inst_index, int compare_slot, char* buf, size_t cap){ if(!buf||cap<4) return NULL; buf[0]='\0'; if(compare_slot<0) return buf; int equipped=rogue_equip_get((enum RogueEquipSlot)compare_slot); if(equipped<0) return buf; int cand_min=rogue_item_instance_damage_min(inst_index); int cand_max=rogue_item_instance_damage_max(inst_index); int cur_min=rogue_item_instance_damage_min(equipped); int cur_max=rogue_item_instance_damage_max(equipped); int dmin=cand_min-cur_min; int dmax=cand_max-cur_max; snprintf(buf,cap,"Delta Damage: %+d-%+d\n", dmin,dmax); return buf; }

char* rogue_item_tooltip_build_layered(int inst_index, int compare_slot, char* buf, size_t cap){ if(!buf||cap<8) return NULL; char base[512]; rogue_item_tooltip_build(inst_index, base, sizeof base); /* base builder */ size_t off=0; int n=snprintf(buf+off,cap-off,"%s", base); if(n<0||n>= (int)(cap-off)) return buf; off+=n; const RogueItemInstance* it=rogue_item_instance_at(inst_index); const RogueItemDef* d = it? rogue_item_def_at(it->def_index):NULL; if(d && d->base_armor>0){ n=snprintf(buf+off,cap-off,"Implicit: +%d Armor\n", d->base_armor); if(n>0 && n < (int)(cap-off)) off+=n; }
    if(it){ for(int s=0;s<it->socket_count && s<6; s++){ int gem = rogue_item_instance_get_socket(inst_index,s); if(gem>=0){ n=snprintf(buf+off,cap-off,"Gem%d: id=%d\n", s, gem); if(n<0||n>= (int)(cap-off)) break; off+=n; } } }
    if(d && d->set_id>0){ n=snprintf(buf+off,cap-off,"Set: %d\n", d->set_id); if(n>0 && n < (int)(cap-off)) off+=n; }
    if(compare_slot>=0){ char delta[128]; rogue_equipment_compare_deltas(inst_index, compare_slot, delta, sizeof delta); n=snprintf(buf+off,cap-off,"%s", delta); if(n>0 && n < (int)(cap-off)) off+=n; }
    if(off<cap) buf[off]='\0'; return buf; }

float rogue_equipment_proc_preview_dps(void){ float sum=0.f; for(int i=0;i<64;i++){ float t=rogue_proc_triggers_per_min(i); if(t<=0) continue; sum += t/60.f; } return sum; }

int rogue_equipment_socket_select(int inst_index, int socket_index){ const RogueItemInstance* it=rogue_item_instance_at(inst_index); if(!it) return -1; if(socket_index<0 || socket_index>=it->socket_count) return -2; g_socket_sel_inst=inst_index; g_socket_sel_index=socket_index; return 0; }
int rogue_equipment_socket_place_gem(int gem_item_def_index){ if(g_socket_sel_inst<0||g_socket_sel_index<0) return -1; const RogueItemInstance* it=rogue_item_instance_at(g_socket_sel_inst); if(!it) return -2; if(gem_item_def_index<0) return -3; if(rogue_item_instance_get_socket(g_socket_sel_inst,g_socket_sel_index)>=0) return -4; int r=rogue_item_instance_socket_insert(g_socket_sel_inst,g_socket_sel_index,gem_item_def_index); g_socket_sel_inst=-1; g_socket_sel_index=-1; return r; }
void rogue_equipment_socket_clear_selection(void){ g_socket_sel_inst=-1; g_socket_sel_index=-1; }

int rogue_equipment_transmog_select(enum RogueEquipSlot slot, int def_index){ ensure_init(); int r=rogue_equip_set_transmog(slot,def_index); if(r==0) g_transmog_last[slot]=def_index; return r; }
int rogue_equipment_transmog_last_selected(enum RogueEquipSlot slot){ ensure_init(); if(slot<0||slot>=ROGUE_EQUIP__COUNT) return -1; return g_transmog_last[slot]; }

unsigned int rogue_item_tooltip_hash(int inst_index, int compare_slot){ char tmp[768]; rogue_item_tooltip_build_layered(inst_index,compare_slot,tmp,sizeof tmp); return fnv1a(tmp); }
