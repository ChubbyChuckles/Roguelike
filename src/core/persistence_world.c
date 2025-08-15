#include "core/persistence.h"
#include "core/persistence_internal.h"
#include "core/app_state.h"
#include <stdlib.h>
#include <string.h>

void rogue_persistence_load_generation_params(void){
    /* Defaults */
    g_app.gen_water_level = 0.34f; g_app.gen_noise_octaves = 6; g_app.gen_noise_gain = 0.48f; g_app.gen_noise_lacunarity = 2.05f; g_app.gen_river_sources = 10; g_app.gen_river_max_length = 1200; g_app.gen_cave_thresh = 0.60f; g_app.gen_params_dirty = 0;
    FILE* f=NULL;
#if defined(_MSC_VER)
    fopen_s(&f, rogue__gen_params_path(), "rb");
#else
    f=fopen(rogue__gen_params_path(),"rb");
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
    fopen_s(&f, rogue__gen_params_path(), "wb");
#else
    f=fopen(rogue__gen_params_path(),"wb");
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
