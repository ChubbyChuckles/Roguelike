#ifndef ROGUE_MINIMAP_H
#define ROGUE_MINIMAP_H
#include "../app/app_state.h"
/* Update (rebuild target texture if dirty) and render minimap. */
void rogue_minimap_update_and_render(int mm_max_size);
/* Internal overlay hook (12.4) - draws active loot pings. */
void rogue_minimap_render_loot_pings(int mm_x_off, int mm_y_off, int mm_w, int mm_h);
#endif /* ROGUE_MINIMAP_H */
