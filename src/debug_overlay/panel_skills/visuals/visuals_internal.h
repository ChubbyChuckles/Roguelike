#ifndef ROGUE_PANEL_SKILLS_VISUALS_INTERNAL_H
#define ROGUE_PANEL_SKILLS_VISUALS_INTERNAL_H

#include "../../../core/skills/skill_debug.h"
#include "../../../core/skills/skill_sprite_loader.h"
#include "../../overlay_core.h"
#include "../../widgets/overlay_widgets.h"
#include "../../widgets/overlay_widgets_internal.h"
#include "../shared/panel_skills_shared.h"
#include "panel_skills_visuals.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

/* Shared enums/state used across submodules (kept internal) */

enum rogue_visuals_preview_target
{
    PREV_CAST = 0,
    PREV_PROJECTILE = 1,
    PREV_IMPACT = 2,
    PREV_AOE = 3
};

/* Persistent preview state (lifetime of overlay session). */
struct rogue_visuals_preview_state
{
    RogueTexture tex;
    int loaded;
    char path[256];
    int anim_elapsed_ms;
    int play;
    int loop_override; /* -1 follow vis.animation_loops, else 0/1 */
    int manual_frame;
    int preview_target; /* enum rogue_visuals_preview_target */
    int speed_pct;      /* 10..400 */
};

/* Accessor to a single static instance (defined in preview.c). */
struct rogue_visuals_preview_state* rogue_visuals_preview_state_get(void);

/* Submodule entry points */
void rogue_visuals_draw_import_wizard(RogueSkillVisualParams* vis, int* vchanged);
void rogue_visuals_draw_sprite_browser(RogueSkillVisualParams* vis, int* vchanged);
void rogue_visuals_draw_asset_picker(RogueSkillVisualParams* vis, int* vchanged);
void rogue_visuals_draw_core_fields(RogueSkillVisualParams* vis, int stype, int* vchanged);
void rogue_visuals_draw_prototypes(RogueSkillVisualParams* vis);
void rogue_visuals_draw_sheet_editor(RogueSkillVisualParams* vis, int* vchanged);
void rogue_visuals_preview_section(RogueSkillVisualParams* vis, int* vchanged);
void rogue_visuals_dependency_viewer(RogueSkillVisualParams* vis);
void rogue_visuals_validation_panel(RogueSkillVisualParams* vis);

#endif /* ROGUE_PANEL_SKILLS_VISUALS_INTERNAL_H */
