#include "../../src/core/app.h"
#include "../../src/core/app_state.h"
#include <assert.h>
#include <stdint.h>

static uint64_t mix_u64(uint64_t h, uint64_t x)
{
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    h ^= x + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    return h;
}

static uint64_t start_screen_first_frame_hash(void)
{
    /* Quantize fade t for cross-platform stability */
    unsigned int t_quant = (unsigned int) (g_app.start_state_t * 1000.0f + 0.5f);
    uint64_t h = 1469598103934665603ULL; /* FNV-ish seed */
    h = mix_u64(h, (uint64_t) g_app.show_start_screen);
    h = mix_u64(h, (uint64_t) g_app.start_state);
    h = mix_u64(h, (uint64_t) t_quant);
    h = mix_u64(h, (uint64_t) g_app.menu_index);
    h = mix_u64(h, (uint64_t) g_app.entering_seed);
    h = mix_u64(h, (uint64_t) g_app.pending_seed);
    /* Include viewport to make scaling changes visible */
    h = mix_u64(h, (uint64_t) ((g_app.viewport_w << 16) ^ g_app.viewport_h));
    return h;
}

int main(void)
{
    RogueAppConfig cfg = {
        "StartScreenSnapshot",     320, 180, 320, 180, 0, 0, 0, 1, ROGUE_WINDOW_WINDOWED,
        (RogueColor){0, 0, 0, 255}};

    /* Run 1 */
    assert(rogue_app_init(&cfg));
    rogue_app_step();
    uint64_t h1 = start_screen_first_frame_hash();
    rogue_app_shutdown();

    /* Run 2 (fresh init) */
    assert(rogue_app_init(&cfg));
    rogue_app_step();
    uint64_t h2 = start_screen_first_frame_hash();
    rogue_app_shutdown();

    /* Deterministic across runs */
    if (h1 != h2)
        return 1;
    return 0;
}
