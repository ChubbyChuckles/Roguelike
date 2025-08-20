/* Crafting & Gathering Phase 8 UI Layer (8.1 - 8.7)
 * Provides lightweight headless-friendly rendering helpers for:
 *  - Recipe browser panel (search filter, availability dimming, batch quantity slider stub)
 *  - Enhancement risk preview (temper success & fracture durability loss expectation)
 *  - Material ledger panel (tiers & average quality/ bias)
 *  - Loadout upgrade diff tagging (prefixes recipe line with [UP])
 *  - Accessibility text-only fallback (compact textual lines, no color semantics)
 */
#ifndef ROGUE_CRAFTING_UI_H
#define ROGUE_CRAFTING_UI_H
#include "ui/core/ui_context.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Configure search filter string (persists across frames). */
void rogue_crafting_ui_set_search(const char* s);
const char* rogue_crafting_ui_last_search(void);

/* Toggle accessibility text-only recipe list fallback. */
void rogue_crafting_ui_set_text_only(int enabled);
int  rogue_crafting_ui_text_only(void);

/* Render main crafting panel (recipes + optional upgrade tags). Returns number of recipe lines rendered. */
int rogue_crafting_ui_render_panel(RogueUIContext* ctx, float x, float y, float w, float h);

/* Render material ledger quality overview; returns lines emitted. */
int rogue_crafting_ui_render_material_ledger(RogueUIContext* ctx, float x, float y, float w, float h);

/* Render enhancement risk preview block (temper). intensity parameter influences displayed durability damage.
 * Returns 0 on success. */
int rogue_crafting_ui_render_enhancement_risk(RogueUIContext* ctx, float x, float y, int intensity);

/* Compute expected fracture durability damage (failure probability * durability_damage). */
float rogue_crafting_ui_expected_fracture_damage(int intensity);

#ifdef __cplusplus
}
#endif
#endif
