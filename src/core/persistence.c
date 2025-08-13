#include "core/persistence.h"
#include "core/app_state.h"
#include "entities/player.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void rogue_persistence_load_generation_params(void){
    /* Defaults */
    g_app.gen_water_level = 0.34; g_app.gen_noise_octaves = 6; g_app.gen_noise_gain = 0.48; g_app.gen_noise_lacunarity = 2.05; g_app.gen_river_sources = 10; g_app.gen_river_max_length = 1200; g_app.gen_cave_thresh = 0.60; g_app.gen_params_dirty = 0;
    FILE* f=NULL;
#if defined(_MSC_VER)
    fopen_s(&f, "gen_params.cfg", "rb");
#else
    f=fopen("gen_params.cfg","rb");
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
    fopen_s(&f, "gen_params.cfg", "wb");
#else
    f=fopen("gen_params.cfg","wb");
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
    fopen_s(&f, "player_stats.cfg", "rb");
#else
    f=fopen("player_stats.cfg","rb");
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
    }
    fclose(f);
    rogue_player_recalc_derived(&g_app.player);
    g_app.stats_dirty = 0; /* loaded */
}

void rogue_persistence_save_player_stats(void){
    FILE* f=NULL;
#if defined(_MSC_VER)
    fopen_s(&f, "player_stats.cfg", "wb");
#else
    f=fopen("player_stats.cfg","wb");
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
    fclose(f);
    g_app.stats_dirty = 0;
}
