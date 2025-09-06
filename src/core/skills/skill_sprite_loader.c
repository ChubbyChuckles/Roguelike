#include "skill_sprite_loader.h"
#include "../../content/json_io.h"
#include "../../util/path_utils.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declare texture load (already in sprite.h but include path minimal). */
extern bool rogue_texture_load(RogueTexture* t, const char* path);
extern void rogue_texture_destroy(RogueTexture* t);

#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
extern SDL_Renderer* g_internal_sdl_renderer_ref;
#endif

int rogue_skill_load_png_sequence(const char* directory, const char* prefix,
                                  RogueSprite* out_frames, int max_out)
{
    if (!directory || !prefix || !out_frames || max_out <= 0)
        return 0;
    int count = 0;
    /* Strategy: attempt 3-digit zero-padded first (001..999) then fallback to unpadded. */
    char path_buf[512];
    for (int mode = 0; mode < 2 && count == 0; ++mode)
    {
        for (int i = 1; i < 1000 && count < max_out; ++i)
        {
            if (mode == 0)
                snprintf(path_buf, sizeof path_buf, "%s/%s_%03d.png", directory, prefix, i);
            else
                snprintf(path_buf, sizeof path_buf, "%s/%s_%d.png", directory, prefix, i);
                /* Quick existence check via fopen (platform abstraction could replace later). */
#if defined(_MSC_VER)
            FILE* f = NULL;
            fopen_s(&f, path_buf, "rb");
#else
            FILE* f = fopen(path_buf, "rb");
#endif
            if (!f)
            {
                /* Stop at first gap in the chosen numbering scheme. */
                break;
            }
            fclose(f);
            RogueTexture* tex = (RogueTexture*) malloc(sizeof *tex);
            if (!tex)
                break;
            memset(tex, 0, sizeof *tex);
            if (!rogue_texture_load(tex, path_buf))
            {
                free(tex);
                break; /* stop on load failure */
            }
            RogueSprite* sp = &out_frames[count++];
            memset(sp, 0, sizeof *sp);
            sp->tex = tex; /* owning for now; caller must free textures when done (follow-up) */
            sp->sx = 0;
            sp->sy = 0;
            sp->sw = tex->w;
            sp->sh = tex->h;
        }
    }
    return count;
}

void rogue_skill_free_sequence_frames(RogueSprite* frames, int frame_count)
{
    if (!frames || frame_count <= 0)
        return;
    for (int i = 0; i < frame_count; ++i)
    {
        RogueSprite* sp = &frames[i];
        if (sp->tex)
        {
            rogue_texture_destroy(sp->tex);
            free(sp->tex);
            sp->tex = NULL;
        }
        memset(sp, 0, sizeof *sp);
    }
}

int rogue_skill_pack_frames_horizontal(RogueSprite* frames, int frame_count,
                                       RogueTexture* out_atlas)
{
    if (!frames || frame_count <= 0 || !out_atlas)
        return 0;
#if !(defined(ROGUE_HAVE_SDL))
    (void) frames;
    (void) frame_count;
    (void) out_atlas;
    return 0; /* unsupported without SDL */
#else
    if (!g_internal_sdl_renderer_ref)
        return 0; /* headless stub environment */
    /* Compute atlas dimensions (horizontal strip) */
    int total_w = 0;
    int max_h = 0;
    for (int i = 0; i < frame_count; ++i)
    {
        if (!frames[i].tex)
            return 0; /* require owning textures */
        total_w += frames[i].sw;
        if (frames[i].sh > max_h)
            max_h = frames[i].sh;
    }
    if (total_w <= 0 || max_h <= 0)
        return 0;
    SDL_Texture* atlas = SDL_CreateTexture(g_internal_sdl_renderer_ref, SDL_PIXELFORMAT_RGBA32,
                                           SDL_TEXTUREACCESS_TARGET, total_w, max_h);
    if (!atlas)
        return 0;
    SDL_SetTextureBlendMode(atlas, SDL_BLENDMODE_BLEND);
    /* Render copy each frame texture into atlas */
    SDL_Texture* prev = SDL_GetRenderTarget(g_internal_sdl_renderer_ref);
    SDL_SetRenderTarget(g_internal_sdl_renderer_ref, atlas);
    SDL_SetRenderDrawBlendMode(g_internal_sdl_renderer_ref, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_internal_sdl_renderer_ref, 0, 0, 0, 0);
    SDL_RenderClear(g_internal_sdl_renderer_ref);
    int x_cursor = 0;
    for (int i = 0; i < frame_count; ++i)
    {
        RogueSprite* sp = &frames[i];
        SDL_Rect src = {sp->sx, sp->sy, sp->sw, sp->sh};
        SDL_Rect dst = {x_cursor, 0, sp->sw, sp->sh};
        if (sp->tex && sp->tex->handle)
            SDL_RenderCopy(g_internal_sdl_renderer_ref, sp->tex->handle, &src, &dst);
        x_cursor += sp->sw;
    }
    SDL_SetRenderTarget(g_internal_sdl_renderer_ref, prev);
    /* Populate out_atlas texture wrapper */
    memset(out_atlas, 0, sizeof *out_atlas);
    out_atlas->handle = atlas;
    out_atlas->w = total_w;
    out_atlas->h = max_h;
    /* Rewrite frames to reference atlas & free old textures */
    x_cursor = 0;
    for (int i = 0; i < frame_count; ++i)
    {
        RogueSprite* sp = &frames[i];
        RogueTexture* old = sp->tex;
        sp->tex = out_atlas; /* non-owning now */
        sp->sx = x_cursor;
        sp->sy = 0;
        x_cursor += sp->sw;
        if (old)
        {
            rogue_texture_destroy(old);
            free(old);
        }
    }
    return frame_count;
#endif
}

int rogue_skill_build_grid_frames(const RogueTexture* tex, int cols, int rows,
                                  RogueSprite* out_frames, int max_out)
{
    if (!tex || cols <= 0 || rows <= 0 || !out_frames || max_out <= 0)
        return 0;
    const int frame_w = tex->w / cols;
    const int frame_h = tex->h / rows;
    if (frame_w <= 0 || frame_h <= 0)
        return 0;

    int count = 0;
    for (int r = 0; r < rows && count < max_out; ++r)
    {
        for (int c = 0; c < cols && count < max_out; ++c)
        {
            RogueSprite* sp = &out_frames[count++];
            // Zero then set fields we know; keep default blending from zero-init if struct supports
            // it
            memset(sp, 0, sizeof(*sp));
            sp->tex = (RogueTexture*) tex; // non-owning reference
            sp->sx = c * frame_w;
            sp->sy = r * frame_h;
            sp->sw = frame_w;
            sp->sh = frame_h;
        }
    }
    return count;
}

int rogue_skill_anim_sample_index(int frame_count, int frame_duration_ms, int elapsed_ms, int loop)
{
    if (frame_count <= 0)
        return 0;
    int dur = frame_duration_ms > 0 ? frame_duration_ms : 100;
    if (elapsed_ms <= 0)
        return 0;
    long long idx = (long long) elapsed_ms / dur;
    if (idx < 0)
        idx = 0;
    if (loop)
    {
        return (int) (idx % frame_count);
    }
    else
    {
        if (idx >= frame_count)
            return frame_count - 1;
        return (int) idx;
    }
}

static int sprite_from_xywh(RogueSprite* sp, const RogueTexture* tex, int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0)
        return 0;
    memset(sp, 0, sizeof *sp);
    sp->tex = (RogueTexture*) tex;
    sp->sx = x;
    sp->sy = y;
    sp->sw = w;
    sp->sh = h;
    return 1;
}

/* --- Minimal JSON scanning helpers (not a full parser) --- */
static const char* skip_ws(const char* s)
{
    while (s && *s && (unsigned char) *s <= 32)
        ++s;
    return s;
}

static const char* find_after(const char* start, const char* needle)
{
    const char* p = strstr(start, needle);
    return p ? p + strlen(needle) : NULL;
}

static const char* parse_int_after(const char* start, const char* end, const char* key, int* out)
{
    const char* p = strstr(start, key);
    if (!p || (end && p >= end))
        return NULL;
    p = strchr(p, ':');
    if (!p || (end && p >= end))
        return NULL;
    ++p;
    p = skip_ws(p);
    char* ep = NULL;
    long v = strtol(p, &ep, 10);
    if (ep == p || (end && ep > end))
        return NULL;
    if (out)
        *out = (int) v;
    return ep;
}

static const char* find_block(const char* s, char open_ch, char close_ch, const char** out_end)
{
    s = strchr(s, open_ch);
    if (!s)
        return NULL;
    int depth = 1;
    const char* p = s + 1;
    while (*p)
    {
        if (*p == open_ch)
            depth++;
        else if (*p == close_ch)
        {
            depth--;
            if (depth == 0)
            {
                if (out_end)
                    *out_end = p;
                return s;
            }
        }
        ++p;
    }
    return NULL;
}

static int parse_xywh_block(const char* bstart, const char* bend, int* x, int* y, int* w, int* h)
{
    int tx, ty, tw, th;
    if (!parse_int_after(bstart, bend, "\"x\"", &tx))
        return 0;
    if (!parse_int_after(bstart, bend, "\"y\"", &ty))
        return 0;
    if (!parse_int_after(bstart, bend, "\"w\"", &tw))
        return 0;
    if (!parse_int_after(bstart, bend, "\"h\"", &th))
        return 0;
    if (x)
        *x = tx;
    if (y)
        *y = ty;
    if (w)
        *w = tw;
    if (h)
        *h = th;
    return 1;
}

static int parse_frames_array_text(const char* arr_start, const char* arr_end,
                                   const RogueTexture* tex, RogueSprite* out, int max_out)
{
    int count = 0;
    const char* p = arr_start;
    while (p && p < arr_end && count < max_out)
    {
        const char* obj_end = NULL;
        const char* obj_start = find_block(p, '{', '}', &obj_end);
        if (!obj_start || !obj_end || obj_start >= arr_end)
            break;
        int x, y, w, h;
        if (parse_xywh_block(obj_start, obj_end, &x, &y, &w, &h))
        {
            if (sprite_from_xywh(&out[count], tex, x, y, w, h))
                ++count;
        }
        p = obj_end + 1;
    }
    return count;
}

static int parse_frames_object_text(const char* obj_start, const char* obj_end,
                                    const RogueTexture* tex, RogueSprite* out, int max_out)
{
    int count = 0;
    const char* p = obj_start;
    while (p && p < obj_end && count < max_out)
    {
        /* Prefer nested "frame" blocks if present */
        const char* fr = strstr(p, "\"frame\"");
        if (!fr || fr >= obj_end)
            break;
        const char* fr_block_end = NULL;
        const char* fr_block_start = find_block(fr, '{', '}', &fr_block_end);
        if (!fr_block_start || !fr_block_end || fr_block_start >= obj_end)
            break;
        int x, y, w, h;
        if (parse_xywh_block(fr_block_start, fr_block_end, &x, &y, &w, &h))
        {
            if (sprite_from_xywh(&out[count], tex, x, y, w, h))
                ++count;
        }
        p = fr_block_end + 1;
    }

    if (count == 0)
    {
        /* Fallback: objects directly containing x/y/w/h */
        const char* p2 = obj_start;
        while (p2 && p2 < obj_end && count < max_out)
        {
            const char* e = NULL;
            const char* s = find_block(p2, '{', '}', &e);
            if (!s || !e || s >= obj_end)
                break;
            int x, y, w, h;
            if (parse_xywh_block(s, e, &x, &y, &w, &h))
            {
                if (sprite_from_xywh(&out[count], tex, x, y, w, h))
                    ++count;
            }
            p2 = e + 1;
        }
    }
    return count;
}

int rogue_skill_build_packed_frames_from_json_text(const RogueTexture* tex, const char* json_text,
                                                   RogueSprite* out_frames, int max_out)
{
    if (!tex || !json_text || !out_frames || max_out <= 0)
        return 0;
    /* Find frames key */
    const char* k = strstr(json_text, "\"frames\"");
    if (!k)
        return 0;
    const char* colon = strchr(k, ':');
    if (!colon)
        return 0;
    const char* p = skip_ws(colon + 1);
    if (!p)
        return 0;
    int count = 0;
    if (*p == '[')
    {
        const char* arr_end = NULL;
        const char* arr_start = find_block(p, '[', ']', &arr_end);
        if (arr_start && arr_end)
            count = parse_frames_array_text(arr_start + 1, arr_end, tex, out_frames, max_out);
    }
    else if (*p == '{')
    {
        const char* obj_end = NULL;
        const char* obj = find_block(p, '{', '}', &obj_end);
        if (obj && obj_end)
            count = parse_frames_object_text(obj + 1, obj_end, tex, out_frames, max_out);
    }
    return count;
}

int rogue_skill_build_packed_frames_from_file(const RogueTexture* tex, const char* json_path,
                                              RogueSprite* out_frames, int max_out)
{
    if (!tex || !json_path || !out_frames || max_out <= 0)
        return 0;
    char* buf = NULL;
    size_t sz = 0;
    if (json_io_read_file(json_path, &buf, &sz, NULL, 0) != 0 || !buf)
        return 0;
    int count = rogue_skill_build_packed_frames_from_json_text(tex, buf, out_frames, max_out);
    free(buf);
    return count;
}
