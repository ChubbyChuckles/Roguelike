#include "core/persistence.h"
#include "core/persistence_internal.h"
#include "core/app_state.h"
#include "entities/player.h"
#include "core/skills.h"
#include "core/inventory.h"
#include "core/loot_instances.h"
#include "core/loot_affixes.h"
#include <stdlib.h>
#include <string.h>

static int g_player_stats_version = 1; /* default if absent */
int rogue_persistence_player_version(void){ return g_player_stats_version; }
void rogue_persistence_load_player_stats(void){
    FILE* f=NULL;
#if defined(_MSC_VER)
    fopen_s(&f, rogue__player_stats_path(), "rb");
#else
    f=fopen(rogue__player_stats_path(),"rb");
#endif
    if(!f) return;
    char line[256];
    while(fgets(line,sizeof line,f)){
        if(line[0]=='#' || line[0]=='\n' || line[0]=='\0') continue;
        char* eq=strchr(line,'='); if(!eq) continue; *eq='\0';
        char* key=line; char* val=eq+1;
        for(char* p=key; *p; ++p){ if(*p=='\r'||*p=='\n'){*p='\0';break;} }
        for(char* p=val; *p; ++p){ if(*p=='\r'||*p=='\n'){*p='\0';break;} }
        if(strcmp(key,"VERSION")==0){ g_player_stats_version = atoi(val); if(g_player_stats_version<=0) g_player_stats_version=1; continue; }
        if(strcmp(key,"LEVEL")==0) g_app.player.level = atoi(val);
        else if(strcmp(key,"XP")==0) g_app.player.xp = atoi(val);
    else if(strcmp(key,"XP_TO_NEXT")==0) g_app.player.xp_to_next = atoi(val);
    else if(strcmp(key,"XP_TOTAL")==0) { unsigned long long v=0ULL; 
#if defined(_MSC_VER)
        sscanf_s(val, "%llu", &v);
#else
        sscanf(val, "%llu", &v);
#endif
        g_app.player.xp_total_accum = v; }
        else if(strcmp(key,"STR")==0) g_app.player.strength = atoi(val);
        else if(strcmp(key,"DEX")==0) g_app.player.dexterity = atoi(val);
        else if(strcmp(key,"VIT")==0) g_app.player.vitality = atoi(val);
        else if(strcmp(key,"INT")==0) g_app.player.intelligence = atoi(val);
        else if(strcmp(key,"CRITC")==0) g_app.player.crit_chance = atoi(val);
        else if(strcmp(key,"CRITD")==0) g_app.player.crit_damage = atoi(val);
        else if(strcmp(key,"UNSPENT")==0) g_app.unspent_stat_points = atoi(val);
        else if(strcmp(key,"HP")==0) g_app.player.health = atoi(val);
        else if(strcmp(key,"MP")==0) g_app.player.mana = atoi(val);
        else if(strcmp(key,"TALENTPTS")==0) g_app.talent_points = atoi(val);
        else if(strncmp(key,"SKRANK",6)==0){
            int id = atoi(key+6);
            const RogueSkillDef* def = rogue_skill_get_def(id);
            struct RogueSkillState* st = (struct RogueSkillState*)rogue_skill_get_state(id);
            if(def && st){ st->rank = atoi(val); if(st->rank > def->max_rank) st->rank = def->max_rank; }
        } else if(strncmp(key,"SKBAR",5)==0){
            int slot = atoi(key+5); if(slot>=0 && slot<10){ g_app.skill_bar[slot] = atoi(val); }
        } else if(strncmp(key, "SKCD",4)==0){
            int id = atoi(key+4);
            struct RogueSkillState* st = (struct RogueSkillState*)rogue_skill_get_state(id);
            if(st){ st->cooldown_end_ms = atof(val); }
        }
        else if(rogue_inventory_try_parse_kv(key,val)) { /* handled */ }
        else if(strncmp(key,"GI",2)==0){
            int slot = atoi(key+2); if(slot>=0 && slot<g_app.item_instance_cap){
                int fields[7]; for(int k=0;k<7;k++) fields[k]=-1;
                int fi=0; char* cur=val; while(cur && fi<7){ char* p=cur; while(*p && *p!=','){ if(*p=='\r'||*p=='\n'){ *p='\0'; break;} p++; } char term=*p; *p='\0'; fields[fi++] = atoi(cur); if(term==','){ p++; } cur = (term==',')? p:NULL; }
                int def_index = fields[0]; int qty=fields[1]; int rarity=fields[2]; int pidx=fields[3]; int pval=fields[4]; int sidx=fields[5]; int sval=fields[6];
                if(def_index>=0 && qty>0){ int inst = rogue_items_spawn(def_index, qty, 0.0f,0.0f); if(inst>=0){ rogue_item_instance_apply_affixes(inst, rarity, pidx,pval,sidx,sval); }}
            }
        }
    }
    fclose(f);
    rogue_player_recalc_derived(&g_app.player);
    g_app.stats_dirty = 0; /* loaded */
}

void rogue_persistence_save_player_stats(void){
    FILE* f=NULL;
#if defined(_MSC_VER)
    fopen_s(&f, rogue__player_stats_path(), "wb");
#else
    f=fopen(rogue__player_stats_path(),"wb");
#endif
    if(!f) return;
    fprintf(f,"# Saved player progression\n");
    fprintf(f,"LEVEL=%d\n", g_app.player.level);
        fprintf(f,"VERSION=%d\n", g_player_stats_version);
    fprintf(f,"XP=%d\n", g_app.player.xp);
    fprintf(f,"XP_TO_NEXT=%d\n", g_app.player.xp_to_next);
    fprintf(f,"XP_TOTAL=%llu\n", g_app.player.xp_total_accum);
    fprintf(f,"STR=%d\n", g_app.player.strength);
    fprintf(f,"DEX=%d\n", g_app.player.dexterity);
    fprintf(f,"VIT=%d\n", g_app.player.vitality);
    fprintf(f,"INT=%d\n", g_app.player.intelligence);
    fprintf(f,"CRITC=%d\n", g_app.player.crit_chance);
    fprintf(f,"CRITD=%d\n", g_app.player.crit_damage);
    fprintf(f,"UNSPENT=%d\n", g_app.unspent_stat_points);
    fprintf(f,"HP=%d\n", g_app.player.health);
    fprintf(f,"MP=%d\n", g_app.player.mana);
    fprintf(f,"TALENTPTS=%d\n", g_app.talent_points);
    for(int i=0;i<g_app.skill_count;i++){
        const RogueSkillState* st = rogue_skill_get_state(i);
        if(st){ fprintf(f,"SKRANK%d=%d\n", i, st->rank); }
    }
    for(int s=0;s<10;s++){ fprintf(f,"SKBAR%d=%d\n", s, g_app.skill_bar[s]); }
    for(int i=0;i<g_app.skill_count;i++){
        const RogueSkillState* st = rogue_skill_get_state(i);
        if(st && st->cooldown_end_ms>0){ fprintf(f,"SKCD%d=%.0f\n", i, st->cooldown_end_ms); }
    }
    rogue_inventory_serialize(f);
    if(g_app.item_instances){
        for(int i=0;i<g_app.item_instance_cap;i++) if(g_app.item_instances[i].active){
            const RogueItemInstance* it = &g_app.item_instances[i];
            fprintf(f,"GI%d=%d,%d,%d,%d,%d,%d,%d\n", i, it->def_index, it->quantity, it->rarity,
                    it->prefix_index, it->prefix_value, it->suffix_index, it->suffix_value);
        }
    }
    fclose(f);
    g_app.stats_dirty = 0;
}
