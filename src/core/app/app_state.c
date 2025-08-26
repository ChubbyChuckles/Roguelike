#include "app_state.h"
RogueAppState g_app; /* zero-initialized; explicit defaults below for clarity */
static void rogue_app_state_init_defaults(void)
{
    g_app.target_enemy_active = 0;
    g_app.target_enemy_level = 1;
    /* Start screen Phase 7 defaults */
    g_app.start_show_credits = 0;
    g_app.start_credits_tab = 0;
    g_app.start_credits_scroll = 0.0f;
    g_app.start_credits_vel = 0.0f;
}
#if defined(_MSC_VER)
/* Fallback explicit init call from app_init (if integrated), for now rely on zero-init plus manual
 * call guard */
static int g_app_state_inited = 0;
void rogue_app_state_maybe_init(void)
{
    if (!g_app_state_inited)
    {
        rogue_app_state_init_defaults();
        g_app_state_inited = 1;
    }
}
#else
__attribute__((constructor)) static void rogue_app_state_ctor(void)
{
    rogue_app_state_init_defaults();
}
#endif
RoguePlayer g_exposed_player_for_stats; /* referenced by combat */
