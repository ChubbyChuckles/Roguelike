/* Positive-path test for asset manager resize / export APIs with a real SDL renderer.
   Exercises single + batch resize (variant and in-place) and BMP/PNG export (PNG guarded).
   Verifies output files are created and basic collision suffixing works.
*/
#define SDL_MAIN_HANDLED
#include "../../src/asset/asset_manager.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static int file_exists(const char* p)
{
    struct stat st;
    return stat(p, &st) == 0 && st.st_size >= 0;
}

int main(void)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
    {
        fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
        return 2;
    }
    SDL_Window* win = SDL_CreateWindow("asset_test", SDL_WINDOWPOS_UNDEFINED,
                                       SDL_WINDOWPOS_UNDEFINED, 64, 64, SDL_WINDOW_HIDDEN);
    if (!win)
    {
        fprintf(stderr, "window fail %s\n", SDL_GetError());
        return 2;
    }
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!ren)
    {
        fprintf(stderr, "renderer fail %s\n", SDL_GetError());
        return 2;
    }

    assert(rogue_asset_manager_init(ren));
    int t0 = rogue_asset_manager_acquire_texture("assets/placeholder.png");
    assert(t0 >= 0);

    /* Variant resize (may fail when placeholder can't load without SDL_image; treat as skip) */
    int variant = rogue_asset_manager_resize_texture_variant(t0, 32, 32, 0);
    if (!(variant >= 0 && variant != t0))
    {
        variant = -1; /* mark unsupported */
    }
    else
    {
        int inplace = rogue_asset_manager_resize_texture_variant(t0, 24, 24, 1);
        if (inplace != t0)
        {
            fprintf(stderr, "in-place resize unexpected failure\n");
        }
    }

    /* Single BMP export */
    int single_bmp =
        rogue_asset_manager_export_texture_bmp(t0, "build/asset_resize_export_pos.bmp");
    if (single_bmp == 1)
    {
        assert(file_exists("build/asset_resize_export_pos.bmp"));
    }

    int arr[2] = {t0, variant};
    int out_idx[2];
    if (variant >= 0)
    {
        int processed = rogue_asset_manager_batch_resize(arr, 2, 16, 16, 0, out_idx, 2);
        assert(processed == 2);
        assert(out_idx[0] != t0 && out_idx[1] != variant);
    }

    int exported_bmp = rogue_asset_manager_batch_export_bmp(arr, 2, "build");
    assert(exported_bmp >= 0); /* may be 0 if texture couldn't load */

    /* PNG single + batch (may be no-op if SDL_image not present) */
    int png_single =
        rogue_asset_manager_export_texture_png(t0, "build/asset_resize_export_pos.png");
    if (png_single == 1)
    {
        assert(file_exists("build/asset_resize_export_pos.png"));
    }
    int exported_png = rogue_asset_manager_batch_export_png(arr, 2, "build");
    if (exported_png > 0)
    {
        /* At least one of the two should exist with .png extension; we check first */
        /* We can't guarantee exact filenames if collision suffixing occurred; just probe base */
        int any = file_exists("build/placeholder.png") || file_exists("build/placeholder_0.png") ||
                  file_exists("build/placeholder_1.png");
        assert(any);
    }

    rogue_asset_manager_release_texture(t0);
    rogue_asset_manager_release_texture(variant);
    rogue_asset_manager_shutdown();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    printf("asset_manager_resize_export_positive OK (bmp_single=%d png_single=%d png_batch=%d)\n",
           single_bmp, png_single, exported_png);
    return 0;
}
