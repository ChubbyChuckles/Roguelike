#include "panel_skills_audio.h"
#include "../../core/skills/skill_debug.h"
#include "../overlay_core.h"
#include "../widgets/overlay_widgets.h"
#include "panel_skills_shared.h"

#if ROGUE_ENABLE_DEBUG_OVERLAY
void panel_skills_draw_audio(int sel)
{
    overlay_label("Audio");
    static RogueSkillVisualParams vis;
    if (rogue_skill_debug_get_visuals(sel, &vis) == 0)
    {
        int vchanged = 0;
        vchanged |=
            overlay_input_text("Cast Sound Id", vis.cast_sound_id, (int) sizeof vis.cast_sound_id);
        vchanged |= overlay_input_text("Impact Sound Id", vis.impact_sound_id,
                                       (int) sizeof vis.impact_sound_id);
        vchanged |=
            overlay_input_text("Loop Sound Id", vis.loop_sound_id, (int) sizeof vis.loop_sound_id);
        vchanged |= overlay_slider_int("Sound Volume", &vis.sound_volume, 0, 100);
        vchanged |= overlay_slider_float("Sound Pitch Var", &vis.sound_pitch_variance, 0.0f, 12.0f);
        if (vchanged)
        {
            (void) rogue_skill_debug_set_visuals(sel, &vis);
            panel_skills_save_overrides_and_refresh();
        }
    }
}
#endif
