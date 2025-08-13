#include "core/persistence.h"
#include "core/app_state.h"
#include "entities/player.h"
#include "core/skills.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Allow tests to redirect persistence output to temp paths */
static char g_player_stats_path[260] = {0};
static char g_gen_params_path[260] = {0};

static const char* rogue_player_stats_path(void){
    if(!g_player_stats_path[0]) return "player_stats.cfg"; return g_player_stats_path;
}
static const char* rogue_gen_params_path(void){
    if(!g_gen_params_path[0]) return "gen_params.cfg"; return g_gen_params_path;
}

void rogue_persistence_set_paths(const char* player_stats_path, const char* gen_params_path){
    if(player_stats_path){
#if defined(_MSC_VER)
        strncpy_s(g_player_stats_path, sizeof g_player_stats_path, player_stats_path, _TRUNCATE);
#else
        strncpy(g_player_stats_path, player_stats_path, sizeof g_player_stats_path -1); g_player_stats_path[sizeof g_player_stats_path -1] = '\0';
#endif
    }
    if(gen_params_path){
#if defined(_MSC_VER)
        strncpy_s(g_gen_params_path, sizeof g_gen_params_path, gen_params_path, _TRUNCATE);
#else
        strncpy(g_gen_params_path, gen_params_path, sizeof g_gen_params_path -1); g_gen_params_path[sizeof g_gen_params_path -1] = '\0';
#endif
    }
}

void rogue_persistence_load_generation_params(void){
    /* Defaults */
    g_app.gen_water_level = 0.34; g_app.gen_noise_octaves = 6; g_app.gen_noise_gain = 0.48; g_app.gen_noise_lacunarity = 2.05; g_app.gen_river_sources = 10; g_app.gen_river_max_length = 1200; g_app.gen_cave_thresh = 0.60; g_app.gen_params_dirty = 0;
    FILE* f=NULL;
#if defined(_MSC_VER)
    fopen_s(&f, rogue_gen_params_path(), "rb");
#else
    f=fopen(rogue_gen_params_path(),"rb");
#endif
    if(!f) return;
    char line[256];
    while(fgets(line,sizeof line,f)){
        if(line[0]=='#' || line[0]=='\n' || line[0]=='\0') continue;
        char* eq = strchr(line,'='); if(!eq) continue; *eq='\0';
        char* key=line; char* val=eq+1;
        for(char* p=key; *p; ++p){ if(*p=='\r'||*p=='\n'){*p='\0';break;} }
        for(char* p=val; *p; ++p){ if(*p=='\r'||*p=='\n'){*p='\0';break;} }
        if(strcmp(key,"WATER_LEVEL")==0) g_app.gen_water_level = (float)atof(val);
        else if(strcmp(key,"NOISE_OCTAVES")==0) g_app.gen_noise_octaves = atoi(val);
        else if(strcmp(key,"NOISE_GAIN")==0) g_app.gen_noise_gain = (float)atof(val);
        else if(strcmp(key,"NOISE_LACUNARITY")==0) g_app.gen_noise_lacunarity = (float)atof(val);
        else if(strcmp(key,"RIVER_SOURCES")==0) g_app.gen_river_sources = atoi(val);
        else if(strcmp(key,"RIVER_MAX_LENGTH")==0) g_app.gen_river_max_length = atoi(val);
        else if(strcmp(key,"CAVE_THRESH")==0) g_app.gen_cave_thresh = (float)atof(val);
    }
    fclose(f);
}

void rogue_persistence_save_generation_params_if_dirty(void){
    if(!g_app.gen_params_dirty) return;
    FILE* f=NULL;
#if defined(_MSC_VER)
    fopen_s(&f, rogue_gen_params_path(), "wb");
#else
    f=fopen(rogue_gen_params_path(),"wb");
#endif
    if(!f) return;
    fprintf(f,"# Saved world generation parameters\n");
    fprintf(f,"WATER_LEVEL=%.4f\n", g_app.gen_water_level);
    fprintf(f,"NOISE_OCTAVES=%d\n", g_app.gen_noise_octaves);
    fprintf(f,"NOISE_GAIN=%.4f\n", g_app.gen_noise_gain);
    fprintf(f,"NOISE_LACUNARITY=%.4f\n", g_app.gen_noise_lacunarity);
    fprintf(f,"RIVER_SOURCES=%d\n", g_app.gen_river_sources);
    fprintf(f,"RIVER_MAX_LENGTH=%d\n", g_app.gen_river_max_length);
    fprintf(f,"CAVE_THRESH=%.4f\n", g_app.gen_cave_thresh);
    fclose(f);
    g_app.gen_params_dirty = 0;
}

void rogue_persistence_load_player_stats(void){
    FILE* f=NULL;
#if defined(_MSC_VER)
    fopen_s(&f, rogue_player_stats_path(), "rb");
#else
    f=fopen(rogue_player_stats_path(),"rb");
#endif
    if(!f) return;
    char line[256];
    while(fgets(line,sizeof line,f)){
        if(line[0]=='#' || line[0]=='\n' || line[0]=='\0') continue;
        char* eq=strchr(line,'='); if(!eq) continue; *eq='\0';
        char* key=line; char* val=eq+1;
        for(char* p=key; *p; ++p){ if(*p=='\r'||*p=='\n'){*p='\0';break;} }
        for(char* p=val; *p; ++p){ if(*p=='\r'||*p=='\n'){*p='\0';break;} }
        if(strcmp(key,"LEVEL")==0) g_app.player.level = atoi(val);
        else if(strcmp(key,"XP")==0) g_app.player.xp = atoi(val);
        else if(strcmp(key,"XP_TO_NEXT")==0) g_app.player.xp_to_next = atoi(val);
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
            /* Optional: cooldown end timestamp (ms). If in past relative to new session start, it will immediately be available. */
            int id = atoi(key+4);
            struct RogueSkillState* st = (struct RogueSkillState*)rogue_skill_get_state(id);
            if(st){ st->cooldown_end_ms = atof(val); }
        }
    }
    fclose(f);
    rogue_player_recalc_derived(&g_app.player);
    g_app.stats_dirty = 0; /* loaded */
}

void rogue_persistence_save_player_stats(void){
    FILE* f=NULL;
#if defined(_MSC_VER)
    fopen_s(&f, rogue_player_stats_path(), "wb");
#else
    f=fopen(rogue_player_stats_path(),"wb");
#endif
    if(!f) return;
    fprintf(f,"# Saved player progression\n");
    fprintf(f,"LEVEL=%d\n", g_app.player.level);
    fprintf(f,"XP=%d\n", g_app.player.xp);
    fprintf(f,"XP_TO_NEXT=%d\n", g_app.player.xp_to_next);
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
    /* Persist skill bar slot assignments */
    for(int s=0;s<10;s++){ fprintf(f,"SKBAR%d=%d\n", s, g_app.skill_bar[s]); }
    /* Persist cooldown end times (optional; allows preserving long cooldown skills across sessions) */
    for(int i=0;i<g_app.skill_count;i++){
        const RogueSkillState* st = rogue_skill_get_state(i);
        if(st && st->cooldown_end_ms>0){ fprintf(f,"SKCD%d=%.0f\n", i, st->cooldown_end_ms); }
    }
    fclose(f);
    g_app.stats_dirty = 0;
}
