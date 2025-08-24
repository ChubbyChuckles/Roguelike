#ifndef ROGUE_PLAYER_ASSETS_H
#define ROGUE_PLAYER_ASSETS_H
#include "../app/app_state.h"
void rogue_player_assets_ensure_loaded(void);
void rogue_player_assets_update_animation(float frame_dt_ms, float dt_ms, float raw_dt_ms,
                                          int attack_pressed);
#endif
