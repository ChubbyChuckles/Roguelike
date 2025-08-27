#include "hud_overlays.h"
#include "../../graphics/font.h"
#include "../../util/metrics.h"
#include <string.h>
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

/** @brief Structure representing an alert message in the HUD.
 *
 * This struct holds the state of an individual alert, including its activity status,
 * time-to-live, message text, and color components.
 */
typedef struct RogueAlert
{
    int active;   /**< Flag indicating if the alert is currently active (1) or inactive (0). */
    float ttl_ms; /**< Time-to-live in milliseconds; the alert fades out as this decreases. */
    char msg[64]; /**< Null-terminated string containing the alert message (max 63 characters). */
    unsigned char r, g, b, a; /**< Color components: red, green, blue, and alpha (transparency). */
} RogueAlert;

/** @brief Global array of alerts, supporting up to 4 simultaneous alerts. */
static RogueAlert g_alerts[4];

/** @brief Resets all alerts to inactive state.
 *
 * This function clears the global alerts array, deactivating all alerts.
 */
void rogue_alerts_reset(void) { memset(g_alerts, 0, sizeof g_alerts); }

/** @brief Internal function to push a new alert into the queue.
 *
 * Searches for an inactive slot in the global alerts array and populates it
 * with the provided message, TTL, and color. If no slot is available, the alert is ignored.
 *
 * @param msg The message string to display (truncated if longer than 63 characters).
 * @param ttl_ms Time-to-live in milliseconds before the alert fades out.
 * @param r Red color component (0-255).
 * @param g Green color component (0-255).
 * @param b Blue color component (0-255).
 */
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

/** @brief Pushes a "Level Up!" alert.
 *
 * Displays a level-up notification with a specific color and duration.
 */
void rogue_alert_level_up(void) { push_alert("Level Up!", 1800.0f, 255, 230, 90); }

/** @brief Pushes a "Low Health" alert.
 *
 * Displays a low health warning with a specific color and duration.
 */
void rogue_alert_low_health(void) { push_alert("Low Health", 1200.0f, 255, 80, 80); }

/** @brief Pushes a "Vendor Restocked" alert.
 *
 * Displays a vendor restock notification with a specific color and duration.
 */
void rogue_alert_vendor_restock(void) { push_alert("Vendor Restocked", 1600.0f, 120, 230, 255); }

/** @brief Updates and renders active alerts.
 *
 * Decrements the TTL of each active alert, deactivates expired ones,
 * and renders them on screen with fading alpha if SDL is available.
 * Updates the global count of rendered alerts.
 *
 * @param dt_ms Delta time in milliseconds since the last update.
 */
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

/** @brief Renders the metrics overlay if enabled.
 *
 * Displays FPS and frame time information at the bottom-left of the screen
 * if SDL is available and the overlay is toggled on. Updates the global
 * metrics rendered flag.
 */
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
