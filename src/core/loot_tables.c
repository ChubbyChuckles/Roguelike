#include "core/loot_tables.h"
#include "core/loot_item_defs.h"
#include <string.h>
#include "core/loot_dynamic_weights.h"
#include <stdio.h>
#include <stdlib.h>
#include "core/loot_rarity_adv.h"

static RogueLootTableDef g_tables[ROGUE_MAX_LOOT_TABLES];
static int g_table_count = 0;

int rogue_loot_tables_reset(void){ g_table_count=0; return 0; }
int rogue_loot_tables_count(void){ return g_table_count; }

static char* lt_next_field(char** cur){ if(!*cur) return NULL; char* s=*cur; char* p=*cur; while(*p && *p!=','){ p++; } if(*p==','){ *p='\0'; p++; } else if(*p=='\0'){} *cur = (*p)? p:NULL; return s; }
static int parse_line(char* line){
    for(char* p=line; *p; ++p){ if(*p=='\r'||*p=='\n'){ *p='\0'; break; } }
    if(line[0]=='#' || line[0]=='\0') return 0;
    char* cursor=line; char* id_f=lt_next_field(&cursor); char* rolls_min_f=lt_next_field(&cursor); char* rolls_max_f=lt_next_field(&cursor);
    if(!id_f||!rolls_min_f||!rolls_max_f) return -1;
    RogueLootTableDef t; memset(&t,0,sizeof t);
#if defined(_MSC_VER)
    strncpy_s(t.id,sizeof t.id,id_f,_TRUNCATE);
#else
    strncpy(t.id,id_f,sizeof t.id -1);
#endif
    t.rolls_min = (int)strtol(rolls_min_f,NULL,10); if(t.rolls_min<0) t.rolls_min=0;
    t.rolls_max = (int)strtol(rolls_max_f,NULL,10); if(t.rolls_max < t.rolls_min) t.rolls_max = t.rolls_min;
    if(!cursor) return -1;
    char* seg_cursor = cursor;
    while(seg_cursor && *seg_cursor){
        while(*seg_cursor==' '||*seg_cursor=='\t') seg_cursor++;
        char* semi = strchr(seg_cursor,';'); if(semi) *semi='\0';
        if(*seg_cursor){
            char* item_cur = seg_cursor; char* f_item = lt_next_field(&item_cur); char* f_weight = lt_next_field(&item_cur); char* f_qmin = lt_next_field(&item_cur); char* f_qmax = lt_next_field(&item_cur); char* f_rmin = lt_next_field(&item_cur); char* f_rmax = lt_next_field(&item_cur);
            if(f_item && f_weight && f_qmin && f_qmax && t.entry_count < ROGUE_MAX_LOOT_ENTRIES){
                RogueLootEntry* e = &t.entries[t.entry_count];
                int idx = rogue_item_def_index(f_item);
                e->item_def_index = idx;
                e->weight = (int)strtol(f_weight,NULL,10); if(e->weight<0) e->weight=0;
                e->qmin = (int)strtol(f_qmin,NULL,10); e->qmax = (int)strtol(f_qmax,NULL,10); if(e->qmax < e->qmin) e->qmax = e->qmin;
                e->rarity_min = -1; e->rarity_max = -1;
                if(f_rmin){ e->rarity_min = (int)strtol(f_rmin,NULL,10); if(e->rarity_min<-1) e->rarity_min=-1; }
                if(f_rmax){ e->rarity_max = (int)strtol(f_rmax,NULL,10); if(e->rarity_max<-1) e->rarity_max=-1; if(e->rarity_max < e->rarity_min) e->rarity_max = e->rarity_min; }
                if(idx>=0 && e->weight>0) t.entry_count++;
            }
        }
        if(!semi) break; else seg_cursor = semi+1;
    }
    if(g_table_count < ROGUE_MAX_LOOT_TABLES && t.entry_count>0){ g_tables[g_table_count++] = t; }
    return 1;
}

int rogue_loot_tables_load_from_cfg(const char* path){
    FILE* f=NULL; int added=0;
#if defined(_MSC_VER)
    fopen_s(&f,path,"rb");
#else
    f=fopen(path,"rb");
#endif
    if(!f) return -1;
    char line[1024];
    while(fgets(line,sizeof line,f)){
        char work[1024];
#if defined(_MSC_VER)
        strncpy_s(work,sizeof work,line,_TRUNCATE);
#else
        strncpy(work,line,sizeof work -1); work[sizeof work -1]='\0';
#endif
        int r = parse_line(work);
        if(r>0) added++;
    }
    fclose(f); return added;
}

const RogueLootTableDef* rogue_loot_table_by_id(const char* id){ if(!id) return NULL; for(int i=0;i<g_table_count;i++){ if(strcmp(g_tables[i].id,id)==0) return &g_tables[i]; } return NULL; }
int rogue_loot_table_index(const char* id){ if(!id) return -1; for(int i=0;i<g_table_count;i++){ if(strcmp(g_tables[i].id,id)==0) return i; } return -1; }

int rogue_loot_roll(int table_index, unsigned int* rng_state, int max_out,
                    int* out_item_def_indices, int* out_quantities){
    if(table_index<0 || table_index>=g_table_count) return 0; if(max_out<=0) return 0; if(!rng_state) return 0;
    const RogueLootTableDef* t = &g_tables[table_index];
    int rolls_range = t->rolls_max - t->rolls_min + 1;
    int rolls = t->rolls_min + (rolls_range>0? rogue_rng_range(rng_state, rolls_range):0);
    int produced=0;
    for(int r=0; r<rolls; ++r){
        /* Compute total weight */
        int total_w=0; for(int i=0;i<t->entry_count;i++){ total_w += t->entries[i].weight; }
        if(total_w<=0) break;
        int pick = rogue_rng_range(rng_state, total_w);
        int acc=0; const RogueLootEntry* chosen=NULL;
        for(int i=0;i<t->entry_count;i++){ acc += t->entries[i].weight; if(pick < acc){ chosen = &t->entries[i]; break; }}
        if(!chosen) continue;
        int qty_range = chosen->qmax - chosen->qmin + 1;
        int qty = chosen->qmin + (qty_range>0? rogue_rng_range(rng_state, qty_range):0);
        if(produced < max_out){ out_item_def_indices[produced] = chosen->item_def_index; out_quantities[produced] = qty; produced++; }
    }
    return produced;
}

int rogue_loot_roll_ex(int table_index, unsigned int* rng_state, int max_out,
                    int* out_item_def_indices, int* out_quantities, int* out_rarities){
    if(table_index<0 || table_index>=g_table_count) return 0; if(max_out<=0) return 0; if(!rng_state) return 0;
    const RogueLootTableDef* t = &g_tables[table_index];
    int rolls_range = t->rolls_max - t->rolls_min + 1;
    int rolls = t->rolls_min + (rolls_range>0? rogue_rng_range(rng_state, rolls_range):0);
    int produced=0;
    for(int r=0; r<rolls; ++r){
        int total_w=0; for(int i=0;i<t->entry_count;i++){ total_w += t->entries[i].weight; }
        if(total_w<=0) break;
        int pick = rogue_rng_range(rng_state, total_w);
        int acc=0; const RogueLootEntry* chosen=NULL;
        for(int i=0;i<t->entry_count;i++){ acc += t->entries[i].weight; if(pick < acc){ chosen = &t->entries[i]; break; }}
        if(!chosen) continue;
        int qty_range = chosen->qmax - chosen->qmin + 1;
        int qty = chosen->qmin + (qty_range>0? rogue_rng_range(rng_state, qty_range):0);
        int rarity = -1;
        if(chosen->rarity_min >= 0){
            rarity = rogue_loot_rarity_sample(rng_state, chosen->rarity_min, chosen->rarity_max);
        }
        if(produced < max_out){
            out_item_def_indices[produced] = chosen->item_def_index;
            out_quantities[produced] = qty;
            if(out_rarities) out_rarities[produced] = rarity;
            produced++;
        }
    }
    return produced;
}

int rogue_loot_rarity_sample(unsigned int* rng_state, int rmin, int rmax){
    if(rmin < 0) return -1; if(rmax < rmin) rmax = rmin; int span = rmax - rmin + 1; if(span <= 0) return rmin;
    int weights[5]; for(int i=0;i<5;i++) weights[i]=0; for(int r=rmin;r<=rmax && r<5;r++){ weights[r]=1; }
    rogue_loot_dyn_apply(weights);
    int total=0; for(int r=rmin;r<=rmax && r<5;r++){ total += weights[r]; }
    if(total<=0) return rmin;
    int pick = rogue_rng_range(rng_state,total);
    int acc=0; int rolled=rmin; for(int r=rmin;r<=rmax && r<5;r++){ acc += weights[r]; if(pick < acc){ rolled = r; break; }}
    /* Apply rarity floor and pity system */
    rolled = rogue_rarity_apply_floor(rolled, rmin, rmax);
    rolled = rogue_rarity_apply_pity(rolled, rmin, rmax);
    return rolled;
}
