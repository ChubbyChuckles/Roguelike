/* Simple item sprite atlas adapter: loads single PNG (items.png).
   Future: integrate with texture manager / caching. */
#ifndef ROGUE_ITEM_SPRITE_H
#define ROGUE_ITEM_SPRITE_H
int rogue_item_sprite_load_atlas(const char* path); /* returns 0 on success */
#endif
