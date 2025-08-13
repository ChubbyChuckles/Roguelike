#include "game/damage_numbers.h"
#include "core/app_state.h"

void rogue_add_damage_number_ex(float x, float y, int amount, int from_player, int crit){
    if(amount==0) return;
    if(g_app.dmg_number_count < (int)(sizeof(g_app.dmg_numbers)/sizeof(g_app.dmg_numbers[0]))){
        int i = g_app.dmg_number_count++;
        g_app.dmg_numbers[i].x = x; g_app.dmg_numbers[i].y = y;
        g_app.dmg_numbers[i].vx = 0.0f; g_app.dmg_numbers[i].vy = -0.38f;
        g_app.dmg_numbers[i].life_ms = 700.0f; g_app.dmg_numbers[i].total_ms = 700.0f;
        g_app.dmg_numbers[i].amount = amount; g_app.dmg_numbers[i].from_player = from_player;
        g_app.dmg_numbers[i].crit = crit?1:0; g_app.dmg_numbers[i].scale = crit? 1.4f : 1.0f;
    }
}
void rogue_add_damage_number(float x, float y, int amount, int from_player){
    rogue_add_damage_number_ex(x,y,amount,from_player,0);
}
int rogue_app_damage_number_count(void){ return g_app.dmg_number_count; }
void rogue_app_test_decay_damage_numbers(float ms){
    for(int i=0;i<g_app.dmg_number_count;){
        g_app.dmg_numbers[i].life_ms -= ms;
        if(g_app.dmg_numbers[i].life_ms <= 0){
            g_app.dmg_numbers[i] = g_app.dmg_numbers[g_app.dmg_number_count-1];
            g_app.dmg_number_count--; continue;
        }
        ++i;
    }
}
