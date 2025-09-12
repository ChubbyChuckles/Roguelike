/* test_hit_mask_compression_auto.c
   Verifies compression analytics and AUTO codec selection (none vs RLE).
*/
#include "../../src/game/hit_pixel_mask.h"
#include "../../src/game/pixel_mask_loader.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(ROGUE_HAVE_SDL)
#include <SDL.h>
#endif

static void make_surface_pattern(SDL_Surface** out, int w, int h, int bpp32, int pattern)
{
#if defined(ROGUE_HAVE_SDL)
    Uint32 fmt = bpp32 ? SDL_PIXELFORMAT_RGBA32 : SDL_PIXELFORMAT_RGB24;
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, w, h, bpp32 ? 32 : 24, fmt);
    assert(s);
    SDL_LockSurface(s);
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            Uint8 r = 0, g = 0, b = 0, a = 255;
            int on = 0;
            switch (pattern)
            {
            case 0: /* sparse: single vertical stripe every 8 px */
                on = (x % 8) == 0;
                break;
            case 1: /* checkerboard 1x1 (dense changes) */
                on = ((x ^ y) & 1) != 0;
                break;
            default:
                on = 0;
            }
            if (on)
                a = 255;
            else
                a = 0;
            Uint32 px = SDL_MapRGBA(s->format, r, g, b, a);
            if (s->format->BytesPerPixel == 4)
                ((Uint32*) ((Uint8*) s->pixels + y * s->pitch))[x] = px;
            else
            {
                Uint8 rr, gg, bb, aa;
                SDL_GetRGBA(px, s->format, &rr, &gg, &bb, &aa);
                Uint8* p = ((Uint8*) s->pixels + y * s->pitch) + x * 3;
                p[0] = rr;
                p[1] = gg;
                p[2] = bb;
            }
        }
    }
    SDL_UnlockSurface(s);
    *out = s;
#else
    (void) out;
    (void) w;
    (void) h;
    (void) bpp32;
    (void) pattern;
#endif
}

static int run_case(int pattern)
{
#if !defined(ROGUE_HAVE_SDL)
    return 0;
#else
    SDL_Surface* surf = NULL;
    make_surface_pattern(&surf, 64, 64, 32, pattern);
    RoguePixelMaskLoadConfig cfg = rogue_pixel_mask_load_config_default();
    cfg.mipmap_levels = 1;

    RogueHitPixelMaskFrame frame_auto = {0};
    RoguePixelMaskMetrics m_auto = {0};
    cfg.compression_level = -1; /* AUTO */
    int ok = rogue_pixel_mask_build_from_surface(surf, &cfg, &frame_auto, &m_auto);
    assert(ok);

    RogueHitPixelMaskFrame frame_rle = {0};
    RoguePixelMaskMetrics m_rle = {0};
    cfg.compression_level = 1; /* force RLE */
    ok = rogue_pixel_mask_build_from_surface(surf, &cfg, &frame_rle, &m_rle);
    assert(ok);

    RogueHitPixelMaskFrame frame_none = {0};
    RoguePixelMaskMetrics m_none = {0};
    cfg.compression_level = 0; /* force none */
    ok = rogue_pixel_mask_build_from_surface(surf, &cfg, &frame_none, &m_none);
    assert(ok);

    /* All builds should have identical occupancy metrics */
    assert(m_auto.total_pixels == m_rle.total_pixels);
    assert(m_auto.collision_pixels == m_rle.collision_pixels);
    assert(m_auto.collision_pixels == m_none.collision_pixels);

    size_t raw = frame_none.pitch_words * frame_none.height * sizeof(uint32_t);

    if (pattern == 0)
    {
        /* Sparse vertical runs: RLE should help */
        assert(frame_rle.compressed && frame_rle.compressed_size > 0);
        assert(frame_rle.compressed_size < raw);
        /* AUTO should pick RLE */
        assert(frame_auto.compressed_format == 1);
        assert(frame_auto.compressed_size == frame_rle.compressed_size);
        assert(m_auto.compressed_size == frame_rle.compressed_size);
        assert(m_auto.compression_ratio < 1.0f);
    }
    else if (pattern == 1)
    {
        /* Checkerboard changes frequently: RLE is worse or equal; AUTO should choose none */
        /* Accept non-strict: RLE may be slightly larger; ensure AUTO picks none */
        if (frame_rle.compressed_size >= raw)
        {
            assert(frame_auto.compressed_format == 0);
            assert(frame_auto.compressed_size == 0);
            assert(m_auto.compression_ratio == 1.0f);
        }
    }

    SDL_FreeSurface(surf);
    rogue_hit_mask_frame_reset(&frame_auto);
    rogue_hit_mask_frame_reset(&frame_rle);
    rogue_hit_mask_frame_reset(&frame_none);
    return 0;
#endif
}

int main(void)
{
#if defined(ROGUE_HAVE_SDL)
    SDL_SetHint(SDL_HINT_VIDEODRIVER, "dummy");
    SDL_SetHint(SDL_HINT_AUDIODRIVER, "dummy");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0)
        return 1;
#endif
    if (run_case(0) != 0)
        return 1;
    if (run_case(1) != 0)
        return 1;
#if defined(ROGUE_HAVE_SDL)
    SDL_Quit();
#endif
    return 0;
}
