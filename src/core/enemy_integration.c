#include "core/enemy_integration.h"
#include "core/app_state.h"
#include "core/enemy_difficulty_scaling.h"
#include <string.h>

int rogue_enemy_integration_build_mappings(RogueEnemyTypeMapping* out, int max, int* out_count){
    if(!out||max<=0){ if(out_count) *out_count=0; return 0; }
    int count = g_app.enemy_type_count; if(count>max) count=max; for(int i=0;i<count;i++){
        RogueEnemyTypeDef* t=&g_app.enemy_types[i]; RogueEnemyTypeMapping* m=&out[i]; memset(m,0,sizeof *m);
        m->type_index=i; m->archetype_id = t->archetype_id; m->tier_id = t->tier_id; m->base_level_offset = t->base_level_offset;
#if defined(_MSC_VER)
        strncpy_s(m->id,sizeof m->id,t->id[0]?t->id:t->name,_TRUNCATE);
        strncpy_s(m->name,sizeof m->name,t->name,_TRUNCATE);
#else
        strncpy(m->id,t->id[0]?t->id:t->name,sizeof m->id -1); m->id[sizeof m->id -1]='\0';
        strncpy(m->name,t->name,sizeof m->name -1); m->name[sizeof m->name -1]='\0';
#endif
    }
    if(out_count) *out_count=count; return count>0; }

int rogue_enemy_integration_find_by_type(int type_index, const RogueEnemyTypeMapping* arr, int count){ if(!arr||count<=0) return -1; for(int i=0;i<count;i++) if(arr[i].type_index==type_index) return i; return -1; }

int rogue_enemy_integration_validate_unique(const RogueEnemyTypeMapping* arr, int count){ if(!arr||count<=0) return 0; for(int i=0;i<count;i++){ for(int j=i+1;j<count;j++){ if(arr[i].type_index==arr[j].type_index) return 0; if(arr[i].id[0] && arr[j].id[0] && strcmp(arr[i].id,arr[j].id)==0) return 0; } } return 1; }

void rogue_enemy_integration_apply_spawn(struct RogueEnemy* e, const RogueEnemyTypeMapping* map_entry, int player_level){ if(!e||!map_entry) return; e->tier_id = map_entry->tier_id; e->base_level_offset = map_entry->base_level_offset; int enemy_level = player_level + e->base_level_offset; if(enemy_level<1) enemy_level=1; e->level = enemy_level; RogueEnemyFinalStats stats; if(rogue_enemy_compute_final_stats(player_level, enemy_level, e->tier_id, &stats)==0){ e->final_hp=stats.hp; e->final_damage=stats.damage; e->final_defense=stats.defense; e->max_health=(int)(stats.hp); if(e->max_health<1) e->max_health=1; e->health=e->max_health; } }
