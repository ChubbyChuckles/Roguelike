#include "overlay_core.h"
#if ROGUE_ENABLE_DEBUG_OVERLAY

/* Ensure we can toggle SDL blend mode while rendering overlay backdrops */
#include "../core/app/app_state.h"
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

#define OVERLAY_MAX_PANELS 32
static OverlayPanel g_panels[OVERLAY_MAX_PANELS];
static int g_panel_count = 0;
static int g_enabled = 0;
static float g_last_dt = 0.0f;
static int g_last_w = 0;
static int g_last_h = 0;

void overlay_init(void)
{
    g_panel_count = 0;
    g_enabled = 0;
}

void overlay_shutdown(void)
{
    g_panel_count = 0;
    g_enabled = 0;
}

int overlay_register_panel(const char* id, const char* name, void (*fn)(void*), void* user)
{
    if (g_panel_count >= OVERLAY_MAX_PANELS)
        return -1;
    g_panels[g_panel_count].id = id;
    g_panels[g_panel_count].name = name;
    g_panels[g_panel_count].fn = fn;
    g_panels[g_panel_count].user = user;
    g_panel_count++;
    return g_panel_count - 1;
}

void overlay_new_frame(float dt, int screen_w, int screen_h)
{
    (void) dt;
    (void) screen_w;
    (void) screen_h;
    g_last_dt = dt;
    g_last_w = screen_w;
    g_last_h = screen_h;
}

void overlay_render(void)
{
    if (!g_enabled)
        return;
        /* Enable alpha blending so translucent panel backgrounds are visible */
#ifdef ROGUE_HAVE_SDL
    SDL_BlendMode prev_mode = SDL_BLENDMODE_NONE;
    int restore_blend = 0;
    if (!g_app.headless && g_app.renderer)
    {
        if (SDL_GetRenderDrawBlendMode(g_app.renderer, &prev_mode) == 0)
            restore_blend = 1;
        SDL_SetRenderDrawBlendMode(g_app.renderer, SDL_BLENDMODE_BLEND);
    }
#endif
    /* Invoke panel callbacks; panels draw using SDL primitives and fonts */
    for (int i = 0; i < g_panel_count; ++i)
    {
        if (g_panels[i].fn)
            g_panels[i].fn(g_panels[i].user);
    }
    /* Restore previous blend mode to avoid side effects on game renderer */
#ifdef ROGUE_HAVE_SDL
    if (restore_blend && g_app.renderer)
        SDL_SetRenderDrawBlendMode(g_app.renderer, prev_mode);
#endif
    (void) g_last_dt;
    (void) g_last_w;
    (void) g_last_h;
}

void overlay_set_enabled(int enabled) { g_enabled = enabled ? 1 : 0; }

int overlay_is_enabled(void) { return g_enabled; }

float overlay_last_dt(void) { return g_last_dt; }

#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
