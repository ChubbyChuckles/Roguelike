#ifndef ROGUE_GAME_DAMAGE_NUMBERS_H
#define ROGUE_GAME_DAMAGE_NUMBERS_H
void rogue_add_damage_number(float x, float y, int amount, int from_player);
void rogue_add_damage_number_ex(float x, float y, int amount, int from_player, int crit);
int rogue_app_damage_number_count(void);
void rogue_app_test_decay_damage_numbers(float ms);
#endif
