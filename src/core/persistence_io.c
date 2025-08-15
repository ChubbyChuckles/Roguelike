#include "core/persistence.h"
#include "core/persistence_internal.h"
#include <string.h>

char g_player_stats_path[260] = {0};
char g_gen_params_path[260] = {0};

const char* rogue__player_stats_path(void){
    if(!g_player_stats_path[0]) return "player_stats.cfg"; return g_player_stats_path;
}
const char* rogue__gen_params_path(void){
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
