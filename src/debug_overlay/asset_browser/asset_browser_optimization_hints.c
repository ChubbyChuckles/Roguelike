/* asset_browser_optimization_hints.c
   Extracted lightweight heuristic optimization pass from panels_asset_browser.c */

#include "debug_overlay/asset_browser/asset_browser_optimization_hints.h"
#include "../../graphics/renderer.h" /* RogueColor */
#include "../widgets/overlay_widgets.h"
#include "debug_overlay/asset_browser/asset_browser_state.h"

#define g_ab_state (*rogue_asset_browser_state())

/* Local shims: original implementations were static in panels_asset_browser.c.
   Keep lightweight here to avoid linking against panel object. */
#if ROGUE_ENABLE_DEBUG_OVERLAY
static void overlay_colored_label(const char* text, RogueColor color)
{
    (void) color; /* color styling not yet supported in minimal overlay */
    overlay_label(text);
}
#endif

void rogue_asset_browser_draw_optimization_hints(RogueAssetManager* m)
{
    if (!m)
        return;
    g_ab_state.opt_tex_large_count = 0;
    g_ab_state.opt_tex_unloaded_count = 0;
    g_ab_state.opt_audio_unloaded_count = 0;
    const int LARGE_TEX_DIM = 1024;
    for (uint32_t ti = 0; ti < m->texture_count; ++ti)
    {
        const RogueAssetTexture* t = &m->textures[ti];
        if (t->width >= LARGE_TEX_DIM || t->height >= LARGE_TEX_DIM)
        {
            if (g_ab_state.opt_tex_large_count < 8)
                snprintf(g_ab_state.opt_tex_large[g_ab_state.opt_tex_large_count++], 96, "%s %dx%d",
                         t->id, t->width, t->height);
        }
        if (!t->sdl_texture && !t->load_failed && t->ref_count > 0)
        {
            if (g_ab_state.opt_tex_unloaded_count < 8)
                snprintf(g_ab_state.opt_tex_unloaded[g_ab_state.opt_tex_unloaded_count++], 96,
                         "%s (lazy) refs=%u", t->id, t->ref_count);
        }
    }
    for (uint32_t ai = 0; ai < m->audio_count; ++ai)
    {
        const RogueAssetAudio* a = &m->audio[ai];
        if (!a->sdl_chunk && !a->load_failed && a->ref_count > 0)
        {
            if (g_ab_state.opt_audio_unloaded_count < 8)
                snprintf(g_ab_state.opt_audio_unloaded[g_ab_state.opt_audio_unloaded_count++], 96,
                         "%s (lazy) refs=%u", a->id, a->ref_count);
        }
    }
    if (g_ab_state.opt_tex_large_count == 0 && g_ab_state.opt_tex_unloaded_count == 0 &&
        g_ab_state.opt_audio_unloaded_count == 0)
    {
        overlay_label("(no optimization hints)");
        return;
    }
    if (g_ab_state.opt_tex_large_count)
    {
        overlay_colored_label("Large Textures:", (RogueColor){200, 150, 40, 255});
        for (int i = 0; i < g_ab_state.opt_tex_large_count; ++i)
            overlay_label(g_ab_state.opt_tex_large[i]);
    }
    if (g_ab_state.opt_tex_unloaded_count)
    {
        overlay_colored_label("Deferred (Referenced) Textures:", (RogueColor){160, 200, 40, 255});
        for (int i = 0; i < g_ab_state.opt_tex_unloaded_count; ++i)
            overlay_label(g_ab_state.opt_tex_unloaded[i]);
    }
    if (g_ab_state.opt_audio_unloaded_count)
    {
        overlay_colored_label("Deferred (Referenced) Audio:", (RogueColor){160, 180, 220, 255});
        for (int i = 0; i < g_ab_state.opt_audio_unloaded_count; ++i)
            overlay_label(g_ab_state.opt_audio_unloaded[i]);
    }
    overlay_label("Hints: consider downscaling oversized textures or preloading lazy refs");
}
