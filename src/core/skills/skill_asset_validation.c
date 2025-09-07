/* Implementation: skill asset validation helpers */
#include "skill_asset_validation.h"
#include "../../graphics/sprite.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int has_allowed_ext(const char* path)
{
    const char* dot = strrchr(path, '.');
    if (!dot || !*(dot + 1))
        return 0;
    char ext[8] = {0};
    size_t n = 0;
    for (const char* p = dot + 1; *p && n < sizeof(ext) - 1; ++p)
        ext[n++] = (char) tolower((unsigned char) *p);
    ext[n] = '\0';
    return (strcmp(ext, "png") == 0 || strcmp(ext, "tga") == 0 || strcmp(ext, "bmp") == 0);
}

int rogue_skill_asset_validate(const char* path, int* missing, int* load_failed, int* dim_err,
                               int* w, int* h, int* ext_warn)
{
    if (missing)
        *missing = 0;
    if (load_failed)
        *load_failed = 0;
    if (dim_err)
        *dim_err = 0;
    if (w)
        *w = 0;
    if (h)
        *h = 0;
    if (ext_warn)
        *ext_warn = 0;
    if (!path || !*path)
    {
        if (missing)
            *missing = 1;
        return 0;
    }
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, path, "rb");
#else
    f = fopen(path, "rb");
#endif
    if (!f)
    {
        if (missing)
            *missing = 1;
        return 0;
    }
    fclose(f);
    if (ext_warn && !has_allowed_ext(path))
        *ext_warn = 1;
    RogueTexture tex = {0};
    if (!rogue_texture_load(&tex, path))
    {
        if (load_failed)
            *load_failed = 1;
        return 0;
    }
    if (w)
        *w = tex.w;
    if (h)
        *h = tex.h;
    if (dim_err && (tex.w <= 0 || tex.h <= 0 || tex.w > 4096 || tex.h > 4096))
        *dim_err = 1;
    rogue_texture_destroy(&tex);
    return 0;
}

int rogue_visuals_infer_grid(int tex_w, int tex_h, int* grid_w, int* grid_h, int* frame_count)
{
    if (grid_w)
        *grid_w = 0;
    if (grid_h)
        *grid_h = 0;
    if (frame_count)
        *frame_count = 0;
    if (tex_w <= 0 || tex_h <= 0)
        return 0;
    const int candidates[] = {64, 48, 32, 24, 16, 12, 8};
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i)
    {
        int c = candidates[i];
        if (tex_w % c == 0 && tex_h % c == 0)
        {
            int gw = tex_w / c;
            int gh = tex_h / c;
            int fc = gw * gh;
            if (fc > 0 && fc <= 256)
            {
                if (grid_w)
                    *grid_w = gw;
                if (grid_h)
                    *grid_h = gh;
                if (frame_count)
                    *frame_count = fc;
                return 1;
            }
        }
    }
    return 0;
}
