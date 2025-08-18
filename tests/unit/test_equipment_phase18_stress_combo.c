/* Phase 18.4: Max layering stress (sockets * gems * set * runeword).
     We author a temporary item definition cfg with:
         - 7 set pieces (weapon + 5 armor + amulet) in set 101 each with 6 sockets fixed.
         - A dedicated gem base definition plus gem rule granting +1 STR +1 FIRE.
         - Runeword bound to weapon id granting +3 STR +5 FIRE.
         - Set thresholds (2pc/4pc/6pc) granting up to +6 STR +6 FIRE at 6+ pieces.
     Assertions:
         1. Fingerprint changes from baseline (no equipment) and stabilizes on repeat.
         2. Layer contributions (implicit + set + runeword + gems) meet analytical lower bound.
         3. No zero / negative regression and piece & socket counts as expected.
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "core/equipment.h"
#include "core/loot_item_defs.h"
#include "core/loot_instances.h"
#include "core/equipment_content.h"
#include "core/equipment_gems.h"
#include "core/stat_cache.h"
#include "core/equipment_stats.h"
#include "entities/player.h"

extern int rogue_items_spawn(int def_index,int quantity,float x,float y);
extern unsigned long long rogue_stat_cache_fingerprint(void);

static void fail(const char* m){ fprintf(stderr,"%s\n",m); exit(1);} static void ck(int c,const char* m){ if(!c) fail(m);} 

static void write_tmp_cfg(void){
        FILE* f = fopen("phase18_stress_items.cfg","wb");
        ck(f!=NULL,"open tmp cfg");
        /* Minimal lines (omit header to avoid long line wrap). Columns:
           id,name,cat,lvl,stack,val,dmin,dmax,armor,sheet,tx,ty,tw,th,rarity,flags,impl_str,impl_dex,impl_vit,impl_int,impl_arm,res_phys,res_fire,res_cold,res_light,res_poison,res_status,set_id,smin,smax */
        fputs("stresswpn,Stress Weapon,2,1,1,50,3,7,0,none,0,0,1,1,1,0,1,0,0,0,0,0,0,0,0,0,0,101,6,6\n",f); /* weapon +1 STR */
        fputs("stress_head,Stress Head,3,1,1,25,0,0,2,none,0,0,1,1,1,0,0,0,0,0,0,0,1,0,0,0,0,101,6,6\n",f);  /* +1 fire */
        fputs("stress_chest,Stress Chest,3,1,1,25,0,0,4,none,0,0,1,1,1,0,0,0,0,0,0,0,1,0,0,0,0,101,6,6\n",f); /* +1 fire */
        fputs("stress_legs,Stress Legs,3,1,1,25,0,0,3,none,0,0,1,1,1,0,0,0,0,0,0,0,1,0,0,0,0,101,6,6\n",f);  /* +1 fire */
        fputs("stress_hands,Stress Hands,3,1,1,25,0,0,1,none,0,0,1,1,1,0,0,0,0,0,0,0,1,0,0,0,0,101,6,6\n",f); /* +1 fire */
        fputs("stress_feet,Stress Feet,3,1,1,25,0,0,1,none,0,0,1,1,1,0,0,0,0,0,0,0,1,0,0,0,0,101,6,6\n",f); /* +1 fire */
        fputs("stress_amulet,Stress Amulet,3,1,1,25,0,0,0,none,0,0,1,1,1,0,1,0,0,0,0,0,0,0,0,0,0,101,6,6\n",f); /* +1 STR */
        fputs("stress_gem,Stress Gem,4,1,1,10,0,0,0,none,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0\n",f);   /* gem base */
        fclose(f);
}

int main(void){
        rogue_sets_reset();
        rogue_runewords_reset();
        rogue_item_defs_reset();
        rogue_equip_reset();
        memset(&g_player_stat_cache,0,sizeof g_player_stat_cache);

        write_tmp_cfg();
                int loaded = rogue_item_defs_load_from_cfg("phase18_stress_items.cfg");
                if(loaded<8){ /* try parent (if test run from subdir) */
                        loaded = rogue_item_defs_load_from_cfg("../phase18_stress_items.cfg");
                }
                if(loaded<8){ fprintf(stderr,"FAIL load stress defs loaded=%d cwd?\n", loaded); return 1; }

        /* Debug: print implicit_strength for first 8 defs */
        for(int i=0;i<loaded && i<8;i++){ const RogueItemDef* d=rogue_item_def_at(i); if(d) fprintf(stderr,"DBG def[%d] id=%s impl_str=%d impl_fire=%d set=%d sockets=%d-%d\n", i,d->id,d->implicit_strength,d->implicit_resist_fire,d->set_id,d->socket_min,d->socket_max); }

        /* Register set 101 thresholds: 2pc(+2/+2 fire), 4pc(+4/+4), 6pc(+6/+6). With 7 pieces we clamp at 6pc. */
        RogueSetDef set; memset(&set,0,sizeof set); set.set_id=101; set.bonus_count=3;
        set.bonuses[0].pieces=2; set.bonuses[0].strength=2; set.bonuses[0].resist_fire=2;
        set.bonuses[1].pieces=4; set.bonuses[1].strength=4; set.bonuses[1].resist_fire=4;
        set.bonuses[2].pieces=6; set.bonuses[2].strength=6; set.bonuses[2].resist_fire=6;
        ck(rogue_set_register(&set)>=0,"register set");

        /* Runeword matches weapon id */
                RogueRuneword rw; memset(&rw,0,sizeof rw); strcpy(rw.pattern,"stresswpn"); rw.strength=3; rw.resist_fire=5; ck(rogue_runeword_register(&rw)>=0,"runeword reg");

        /* Gem definition (+1 STR +1 FIRE) referencing gem base item */
        int gem_item_def = rogue_item_def_index("stress_gem"); ck(gem_item_def>=0,"gem base idx");
        RogueGemDef gem; memset(&gem,0,sizeof gem); strcpy(gem.id,"stress_gem_def"); gem.item_def_index=gem_item_def; gem.strength=1; gem.resist_fire=1; ck(rogue_gem_register(&gem)>=0,"gem reg");

        /* Baseline fingerprint (empty equipment) */
                /* Provide a minimal player (stat cache requires non-null) */
                        RoguePlayer player; memset(&player,0,sizeof player); player.strength=10; player.dexterity=10; player.vitality=10; player.intelligence=10; player.max_health=100; player.crit_chance=5; player.crit_damage=150;
                rogue_stat_cache_force_update(&player); unsigned long long fp_empty = rogue_stat_cache_fingerprint();

        /* Deterministically equip each set piece */
                int inst_weapon = rogue_items_spawn(rogue_item_def_index("stresswpn"),1,0,0); ck(inst_weapon>=0,"spawn weapon");
        int inst_head   = rogue_items_spawn(rogue_item_def_index("stress_head"),1,0,0); ck(inst_head>=0,"spawn head");
        int inst_chest  = rogue_items_spawn(rogue_item_def_index("stress_chest"),1,0,0); ck(inst_chest>=0,"spawn chest");
        int inst_legs   = rogue_items_spawn(rogue_item_def_index("stress_legs"),1,0,0); ck(inst_legs>=0,"spawn legs");
        int inst_hands  = rogue_items_spawn(rogue_item_def_index("stress_hands"),1,0,0); ck(inst_hands>=0,"spawn hands");
        int inst_feet   = rogue_items_spawn(rogue_item_def_index("stress_feet"),1,0,0); ck(inst_feet>=0,"spawn feet");
        int inst_amulet = rogue_items_spawn(rogue_item_def_index("stress_amulet"),1,0,0); ck(inst_amulet>=0,"spawn amulet");

        int insts[]={inst_weapon,inst_head,inst_chest,inst_legs,inst_hands,inst_feet,inst_amulet};
        enum RogueEquipSlot slots[]={ROGUE_EQUIP_WEAPON,ROGUE_EQUIP_ARMOR_HEAD,ROGUE_EQUIP_ARMOR_CHEST,ROGUE_EQUIP_ARMOR_LEGS,ROGUE_EQUIP_ARMOR_HANDS,ROGUE_EQUIP_ARMOR_FEET,ROGUE_EQUIP_AMULET};
        for(int i=0;i<7;i++){ RogueItemInstance* it=(RogueItemInstance*)rogue_item_instance_at(insts[i]); ck(it!=NULL,"inst ptr"); it->socket_count=6; for(int s=0;s<6;s++){ it->sockets[s]=-1; } ck(rogue_equip_try(slots[i],insts[i])==0,"equip"); }

        /* Insert same gem into every socket */
        for(int i=0;i<7;i++){ for(int s=0;s<6;s++){ ck(rogue_item_instance_socket_insert(insts[i],s,gem.item_def_index)==0,"socket ins"); } }

                        rogue_stat_cache_mark_dirty();
                        rogue_equipment_apply_stat_bonuses(&player); /* aggregate after equip + sockets */
                        rogue_stat_cache_force_update(&player); unsigned long long fp1 = rogue_stat_cache_fingerprint();
                if(fp1==fp_empty){ fprintf(stderr,"FAIL fingerprint did not change (fp=%llu)\n", (unsigned long long)fp1); return 2; }
                        rogue_stat_cache_mark_dirty();
                        rogue_equipment_apply_stat_bonuses(&player); /* idempotent */
                        rogue_stat_cache_force_update(&player); unsigned long long fp2 = rogue_stat_cache_fingerprint();
        ck(fp1==fp2,"fingerprint stable");

        /* Expected contributions */
        int implicit_strength_observed = g_player_stat_cache.implicit_strength; /* may exceed minimal due to engine side effects */
        int implicit_strength_lower_bound = 2; /* weapon + amulet */
        int set_strength = 6; /* capped at 6pc threshold despite 7 pieces */
        int runeword_strength_observed = g_player_stat_cache.runeword_strength; /* expect >=3 */
        int gem_strength = 7 /* pieces */ * 6 /* sockets */ * 1;
        if(runeword_strength_observed < 3){ fprintf(stderr,"FAIL runeword_strength %d < 3\n", runeword_strength_observed); return 6; }
        if(implicit_strength_observed < implicit_strength_lower_bound){ fprintf(stderr,"FAIL implicit_strength %d < %d\n", implicit_strength_observed, implicit_strength_lower_bound); return 7; }
        int min_total_strength = implicit_strength_lower_bound + set_strength + 3 /* minimal runeword */ + gem_strength;

        int implicit_fire = 5; /* five armor pieces each +1 fire */
        int set_fire = 6;
        int runeword_fire = 5;
        int gem_fire = 7 * 6 * 1;
        int min_fire = implicit_fire + set_fire + runeword_fire + gem_fire;

                if(g_player_stat_cache.total_strength < min_total_strength){ fprintf(stderr,"FAIL strength %d < min %d\n", g_player_stat_cache.total_strength, min_total_strength); return 3; }
                if(g_player_stat_cache.resist_fire < min_fire){ fprintf(stderr,"FAIL fire %d < min %d\n", g_player_stat_cache.resist_fire, min_fire); return 4; }
                if(g_player_stat_cache.set_strength != set_strength){ fprintf(stderr,"FAIL set_strength %d != %d\n", g_player_stat_cache.set_strength,set_strength); return 5; }
                /* runeword + implicit already validated as lower bounds; allow higher engine-side aggregation */
        ck(g_player_stat_cache.total_strength > 0 && g_player_stat_cache.resist_fire > 0, "non-zero aggregates");

        printf("Phase18.4 stress combo OK (fp=%llu str=%d fire=%d minStr=%d minFire=%d)\n", (unsigned long long)fp1, g_player_stat_cache.total_strength, g_player_stat_cache.resist_fire, min_total_strength, min_fire);
        return 0;
}
