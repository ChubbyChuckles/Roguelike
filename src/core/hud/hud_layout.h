/* hud_layout.h - UI Phase 6.1 HUD layout spec loader
   Provides data-driven placement for HUD elements (health/mana/xp bars, level text).
   Layout file format (kv .cfg style using unified kv parser):
       health_bar = x,y,w,h
       mana_bar   = x,y,w,h
       xp_bar     = x,y,w,h
       level_text = x,y
   Missing keys fall back to defaults. Negative / zero sizes clamped.
*/
#ifndef ROGUE_HUD_LAYOUT_H
#define ROGUE_HUD_LAYOUT_H

typedef struct RogueHUDBarRect
{
    int x, y, w, h;
} RogueHUDBarRect;
typedef struct RogueHUDLayout
{
    RogueHUDBarRect health;
    RogueHUDBarRect mana;
    RogueHUDBarRect xp;
    int level_text_x;
    int level_text_y;
    int loaded; /* 1 if a file successfully parsed at least one key */
} RogueHUDLayout;

/* Load layout from path (tries fallback ../ if not found). Returns 1 on success, 0 on failure.
   On failure defaults are preserved. */
int rogue_hud_layout_load(const char* path);
/* Access current immutable layout */
const RogueHUDLayout* rogue_hud_layout(void);
/* Reset to compiled-in defaults (also clears loaded flag). */
void rogue_hud_layout_reset_defaults(void);

#endif /* ROGUE_HUD_LAYOUT_H */
