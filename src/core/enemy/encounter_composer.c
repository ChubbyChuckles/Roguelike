#include "core/enemy/encounter_composer.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

static RogueEncounterTemplate g_templates[ROGUE_MAX_ENCOUNTER_TEMPLATES];
static int g_template_count = 0;

static unsigned int rng_next(unsigned int* s){ unsigned int x=*s; x^=x<<13; x^=x>>17; x^=x<<5; *s=x; return x; }
static int rng_range(unsigned int* s, int hi){ if(hi<=0) return 0; return (int)(rng_next(s)% (unsigned)hi); }
static float rng_float(unsigned int* s){ return (rng_next(s) & 0xFFFFFF)/ (float)0x1000000; }

static int parse_type(const char* v){
    if(strcmp(v,"swarm")==0) return ROGUE_ENCOUNTER_SWARM;
    if(strcmp(v,"mixed")==0) return ROGUE_ENCOUNTER_MIXED;
    if(strcmp(v,"champion_pack")==0) return ROGUE_ENCOUNTER_CHAMPION_PACK;
    if(strcmp(v,"boss_room")==0) return ROGUE_ENCOUNTER_BOSS_ROOM;
    return ROGUE_ENCOUNTER_SWARM;
}

int rogue_encounters_load_file(const char* path){
    FILE* f=NULL; g_template_count=0;
#if defined(_MSC_VER)
    if(fopen_s(&f,path,"rb")!=0) return -1;
#else
    f=fopen(path,"rb"); if(!f) return -1;
#endif
    char line[256]; RogueEncounterTemplate cur; memset(&cur,0,sizeof cur); cur.elite_spacing=3; cur.elite_chance=0.15f;
    while(fgets(line,sizeof line,f)){
        int blank=1; for(char* c=line; *c; ++c){ if(!isspace((unsigned char)*c)){ blank=0; break; }}
        if(blank){ if(cur.name[0]){ if(g_template_count<ROGUE_MAX_ENCOUNTER_TEMPLATES) g_templates[g_template_count++]=cur; memset(&cur,0,sizeof cur); cur.elite_spacing=3; cur.elite_chance=0.15f; } continue; }
        char* eq=strchr(line,'='); if(!eq) continue; *eq='\0'; char* key=line; char* val=eq+1;
        while(*val && isspace((unsigned char)*val)) val++;
        char* end=val+strlen(val); while(end>val && (end[-1]=='\n'||end[-1]=='\r')) *--end='\0';
        if(strcmp(key,"id")==0) cur.id=atoi(val);
    else if(strcmp(key,"name")==0){
#if defined(_MSC_VER)
        strncpy_s(cur.name, sizeof cur.name, val, _TRUNCATE);
#else
        strncpy(cur.name,val,sizeof cur.name-1); cur.name[sizeof cur.name-1]='\0';
#endif
    }
        else if(strcmp(key,"type")==0) cur.type=parse_type(val);
        else if(strcmp(key,"min")==0) cur.min_count=atoi(val);
        else if(strcmp(key,"max")==0) cur.max_count=atoi(val);
        else if(strcmp(key,"boss")==0) cur.boss=atoi(val);
        else if(strcmp(key,"support_min")==0) cur.support_min=atoi(val);
        else if(strcmp(key,"support_max")==0) cur.support_max=atoi(val);
        else if(strcmp(key,"elite_spacing")==0) cur.elite_spacing=atoi(val);
        else if(strcmp(key,"elite_chance")==0) cur.elite_chance=(float)atof(val);
    }
    if(cur.name[0] && g_template_count<ROGUE_MAX_ENCOUNTER_TEMPLATES) g_templates[g_template_count++]=cur;
    fclose(f);
    return g_template_count;
}

int rogue_encounter_template_count(void){ return g_template_count; }
const RogueEncounterTemplate* rogue_encounter_template_at(int index){ if(index<0||index>=g_template_count) return NULL; return &g_templates[index]; }
const RogueEncounterTemplate* rogue_encounter_template_by_id(int id){ for(int i=0;i<g_template_count;i++) if(g_templates[i].id==id) return &g_templates[i]; return NULL; }

int rogue_encounter_compose(int template_id, int player_level, int difficulty_rating, int biome_id, unsigned int seed, RogueEncounterComposition* out){
    (void)player_level; (void)biome_id; if(!out) return -1; memset(out,0,sizeof *out);
    const RogueEncounterTemplate* t = rogue_encounter_template_by_id(template_id); if(!t) return -2; out->template_id = template_id;
    unsigned int state = seed ? seed : 0xA53u;
    int span = (t->max_count > t->min_count)? (t->max_count - t->min_count + 1):1;
    int count = t->min_count + rng_range(&state, span);
    if(count < t->min_count) count = t->min_count; if(count > t->max_count) count = t->max_count;
    int next_elite_slot = t->elite_spacing>0 ? t->elite_spacing : 3;
    for(int i=0; i<count && i<64; i++){
        RogueEncounterUnit* u = &out->units[out->unit_count];
        u->enemy_type_id = 0; /* placeholder baseline enemy type */
        u->level = difficulty_rating; /* baseline: align level to difficulty */
        u->is_elite = 0;
        if(t->boss && i==0){ u->is_elite=1; out->boss_present=1; }
        else {
            if(i==next_elite_slot){
                if(rng_float(&state) < t->elite_chance){ u->is_elite=1; out->elite_count++; next_elite_slot = i + t->elite_spacing; }
                else { next_elite_slot = i + 1; }
            }
        }
        out->unit_count++;
    }
    /* Support adds */
    if(t->boss && t->support_max>0){
        int sup_span = (t->support_max > t->support_min)? (t->support_max - t->support_min + 1):1;
        int sup = t->support_min + rng_range(&state, sup_span);
        for(int s=0; s<sup && out->unit_count<64; s++){
            RogueEncounterUnit* u = &out->units[out->unit_count++];
            u->enemy_type_id = 0; u->level = difficulty_rating; u->is_elite = 0; out->support_count++;
        }
    }
    return 0;
}
