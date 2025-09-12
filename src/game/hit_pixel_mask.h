/* Pixel-based hit detection (Slice A: structures + loader scaffolding) */
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct RogueHitPixelMaskFrame
    {
        int width;
        int height;
        int origin_x;
        int origin_y;
        int pitch_words; /* 32-bit words per scanline */
        uint32_t* bits;  /* packed bits (row-major) */
        /* Optional advanced features (Phase 1 incremental additions) */
        int mipmap_count; /* includes base level (>=1) */
        struct RogueHitPixelMaskMipmapLevel*
            mipmaps;            /* array of (mipmap_count-1) levels; NULL if none */
        void* compressed;       /* optional compressed buffer (RLE) */
        size_t compressed_size; /* bytes in compressed buffer */
        int compressed_format;  /* 0 = none, 1 = simple RLE */
        /* Distance field (signed, chamfer approximation). Units scaled by distance_field_scale.
            Positive values = inside solid region, negative outside. 0 on boundary. */
        int16_t* distance_field;  /* array [width*height] or NULL if not generated */
        int distance_field_scale; /* scaling (e.g. 10 => value/10 = pixels) */
    } RogueHitPixelMaskFrame;

    typedef struct RogueHitPixelMaskMipmapLevel
    {
        int width;
        int height;
        int pitch_words;
        uint32_t* bits; /* same layout as base */
    } RogueHitPixelMaskMipmapLevel;

    typedef struct RogueHitPixelMaskSet
    {
        int weapon_id;
        int frame_count; /* expected 8 */
        RogueHitPixelMaskFrame frames[8];
        int ready;
    } RogueHitPixelMaskSet;

    /* Global toggle (debug key will flip) */
    extern int g_hit_use_pixel_masks;

    /* Ensure mask set for weapon id (lazy generate simple placeholder if assets not yet
     * integrated). */
    RogueHitPixelMaskSet* rogue_hit_pixel_masks_ensure(int weapon_id);

    /* Release all allocated masks (test teardown) */
    void rogue_hit_pixel_masks_reset_all(void);

    /* Utility: set bit (x,y) */
    static inline void rogue_hit_mask_set(RogueHitPixelMaskFrame* f, int x, int y)
    {
        if (!f || !f->bits)
            return;
        if ((unsigned) x >= (unsigned) f->width || (unsigned) y >= (unsigned) f->height)
            return;
        int idx = y * f->pitch_words + (x >> 5);
        uint32_t m = 1u << (x & 31);
        f->bits[idx] |= m;
    }
    /* Utility: test bit (x,y) */
    static inline int rogue_hit_mask_test(const RogueHitPixelMaskFrame* f, int x, int y)
    {
        if (!f || !f->bits)
            return 0;
        if ((unsigned) x >= (unsigned) f->width || (unsigned) y >= (unsigned) f->height)
            return 0;
        int idx = y * f->pitch_words + (x >> 5);
        uint32_t m = 1u << (x & 31);
        return (f->bits[idx] & m) != 0;
    }

    /* Utility: test bit on a given mipmap level; level 0 == base */
    static inline int rogue_hit_mask_test_level(const RogueHitPixelMaskFrame* f, int level, int x,
                                                int y)
    {
        if (!f)
            return 0;
        if (level <= 0)
            return rogue_hit_mask_test(f, x, y);
        if (f->mipmap_count <= 1 || !f->mipmaps)
            return rogue_hit_mask_test(f, x, y);
        int max_level = f->mipmap_count - 1; /* excluding base */
        if (level > max_level)
            level = max_level;
        const RogueHitPixelMaskMipmapLevel* ml = &f->mipmaps[level - 1];
        if (!ml->bits)
            return 0;
        if ((unsigned) x >= (unsigned) ml->width || (unsigned) y >= (unsigned) ml->height)
            return 0;
        int idx = y * ml->pitch_words + (x >> 5);
        uint32_t m = 1u << (x & 31);
        return (ml->bits[idx] & m) != 0;
    }

    /* Map base-space integer coords (cx,cy) to a mip level with floor division */
    static inline void rogue_hit_mask_level_coords(const RogueHitPixelMaskFrame* f, int level,
                                                   int cx, int cy, int* out_x, int* out_y)
    {
        if (!f || level <= 0)
        {
            if (out_x)
                *out_x = cx;
            if (out_y)
                *out_y = cy;
            return;
        }
        if (f->mipmap_count <= 1 || !f->mipmaps)
        {
            if (out_x)
                *out_x = cx;
            if (out_y)
                *out_y = cy;
            return;
        }
        int shift = level; /* 2x per level */
        if (shift < 0)
            shift = 0;
        if (out_x)
            *out_x = (cx >= 0) ? (cx >> shift) : -((-cx) >> shift);
        if (out_y)
            *out_y = (cy >= 0) ? (cy >> shift) : -((-cy) >> shift);
    }
    /* Compute frame axis-aligned bounds in local mask space (origin aligned) */
    void rogue_hit_mask_frame_aabb(const RogueHitPixelMaskFrame* f, int* out_w, int* out_h);
    /* Test enemy circle against mask frame with simple center+ring sampling; returns 1 on hit and
     * approximates impact local pixel (out_lx/out_ly). */
    int rogue_hit_mask_enemy_test(const RogueHitPixelMaskFrame* f, float enemy_cx_local,
                                  float enemy_cy_local, float enemy_radius, int* out_lx,
                                  int* out_ly);
    /* Convert local pixel (lx,ly) to world position (wx,wy) given player position, pose offsets,
     * scale & rotation (angle radians). */
    void rogue_hit_mask_local_pixel_to_world(const RogueHitPixelMaskFrame* f, int lx, int ly,
                                             float player_x, float player_y, float pose_dx,
                                             float pose_dy, float scale, float angle_rad,
                                             float* out_wx, float* out_wy);

    /* -------- Fast bitwise rectangle utilities (scalar + optional SIMD) -------- */
    /* Runtime toggle to allow tests to force scalar behavior even when SIMD compiled */
    void rogue_hit_mask_simd_set_enabled(int enabled);

    /* Returns 1 if any bit set within [x, x+w) x [y, y+h), clipped to frame bounds. */
    int rogue_hit_mask_any_set_in_rect(const RogueHitPixelMaskFrame* f, int x, int y, int w, int h);

    /* Returns 1 if any overlapping bit between two frames within a common rectangle region
        specified in each frame's local coordinates. This function assumes the regions are
        the same size (w,h) but independently positioned in each frame at (ax,ay) and (bx,by).
        Bits are ANDed positionally; rectangles are internally clipped to each frame. */
    int rogue_hit_mask_intersect_any_same_origin(const RogueHitPixelMaskFrame* a, int ax, int ay,
                                                 const RogueHitPixelMaskFrame* b, int bx, int by,
                                                 int w, int h);

#ifdef __cplusplus
}
#endif
