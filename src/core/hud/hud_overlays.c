#include "hud_overlays.h"
#include "../../graphics/font.h"
#include "../../util/metrics.h"
#include <string.h>
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

typedef struct RogueAlert
{
    int active;
    float ttl_ms;
    char msg[64];
    unsigned char r, g, b, a;
} RogueAlert;
static RogueAlert g_alerts[4];

void rogue_alerts_reset(void) { memset(g_alerts, 0, sizeof g_alerts); }

static void push_alert(const char* msg, float ttl_ms, unsigned char r, unsigned char g,
                       unsigned char b)
{
    for (int i = 0; i < 4; i++)
        if (!g_alerts[i].active)
        {
            g_alerts[i].active = 1;
            g_alerts[i].ttl_ms = ttl_ms;
            size_t n = 0;
            while (msg && msg[n] && n < sizeof(g_alerts[i].msg) - 1)
            {
                g_alerts[i].msg[n] = msg[n];
                n++;
            }
            g_alerts[i].msg[n] = 0;
            g_alerts[i].r = r;
            g_alerts[i].g = g;
            g_alerts[i].b = b;
            g_alerts[i].a = 255;
            return;
        }
}

void rogue_alert_level_up(void) { push_alert("Level Up!", 1800.0f, 255, 230, 90); }
void rogue_alert_low_health(void) { push_alert("Low Health", 1200.0f, 255, 80, 80); }
void rogue_alert_vendor_restock(void) { push_alert("Vendor Restocked", 1600.0f, 120, 230, 255); }

void rogue_alerts_update_and_render(float dt_ms)
{
    int rendered = 0;
    for (int i = 0; i < 4; i++)
        if (g_alerts[i].active)
        {
            g_alerts[i].ttl_ms -= dt_ms;
            if (g_alerts[i].ttl_ms <= 0)
            {
                g_alerts[i].active = 0;
                continue;
            }
            rendered++;
#ifdef ROGUE_HAVE_SDL
            if (g_app.renderer)
            {
                float alpha_scale = 1.0f;
                if (g_alerts[i].ttl_ms < 400.0f)
                {
                    alpha_scale = g_alerts[i].ttl_ms / 400.0f;
                }
                unsigned char a = (unsigned char) (alpha_scale * 255.0f);
                if (a > 255)
                    a = 255;
                int y = 48 + (rendered - 1) * 18;
                rogue_font_draw_text((g_app.viewport_w / 2) - 60, y, g_alerts[i].msg, 1,
                                     (RogueColor){g_alerts[i].r, g_alerts[i].g, g_alerts[i].b, a});
            }
#endif
        }
    g_app.last_alerts_rendered = rendered;
}

void rogue_metrics_overlay_render(void)
{
#ifdef ROGUE_HAVE_SDL
    if (!g_app.renderer)
        return;
    if (!g_app.show_metrics_overlay)
        return;
    double fps, frame_ms, avg_ms;
    rogue_metrics_get(&fps, &frame_ms, &avg_ms);
    char buf[96];
    snprintf(buf, sizeof buf, "FPS %.1f (%.2fms avg %.2f)", fps, frame_ms, avg_ms);
    rogue_font_draw_text(8, g_app.viewport_h - 20, buf, 1, (RogueColor){180, 255, 180, 255});
    g_app.last_metrics_rendered = 1;
#endif
}
