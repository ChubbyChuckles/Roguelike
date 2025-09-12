/* pixel_mask_loader.h - Phase 1 (Milestone 1.1) minimal implementation
 * Provides basic SDL_Surface / file -> RogueHitPixelMaskFrame conversion with
 * alpha threshold + metrics collection. Advanced features (SDF, mipmaps,
 * compression, multi-threading) are deferred to later roadmap slices.
 */
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Configuration for pixel mask loading. Fields beyond alpha_threshold are
     * placeholders for future phases and currently ignored (kept to align with
     * roadmap and avoid churn in call sites that may start populating them). */
    typedef struct RoguePixelMaskLoadConfig
    {
        float alpha_threshold;     /* 0..1 inclusive; pixels with alpha >= threshold become solid */
        int edge_smoothing_passes; /* reserved (future smoothing) */
        int compression_level;     /* 0 = none, 1 = fast RLE (current slice) */
        int mipmap_levels;         /* number of mip levels to generate (>=1). 1 disables. Max 6 */
        int generate_distance_fields; /* 0=none, 1=generate signed distance field (int16 grid) */
        float alpha_gamma; /* Gamma to apply to normalized alpha before threshold; 1.0 = none */
        int derive_alpha_from_luma; /* 1: if no alpha channel present, compute alpha from luma of
                                       RGB */
    } RoguePixelMaskLoadConfig;

    typedef struct RoguePixelMaskMetrics
    {
        uint32_t total_pixels;     /* w*h */
        uint32_t collision_pixels; /* bits set */
        float solid_ratio;         /* collision_pixels / total_pixels */
        uint64_t build_time_ns;    /* optional timing (0 if unavailable) */
        size_t memory_footprint;   /* bytes allocated for bit mask */
        size_t compressed_size;    /* bytes of compressed buffer (if any) */
        int mipmap_levels;         /* number of mip levels actually generated */
    } RoguePixelMaskMetrics;

    /* Forward declare frame type to avoid including hit_pixel_mask.h here (keeps
     * dependency minimal & prevents circular include when used inside that unit). */
    struct RogueHitPixelMaskFrame; /* defined in hit_pixel_mask.h */

    /* Initialize config with defaults */
    static inline RoguePixelMaskLoadConfig rogue_pixel_mask_load_config_default(void)
    {
        RoguePixelMaskLoadConfig c;
        c.alpha_threshold = 0.5f;
        c.edge_smoothing_passes = 0;
        c.compression_level = 1; /* enable light RLE by default */
        c.mipmap_levels = 1;     /* caller can raise to request chain */
        c.generate_distance_fields = 0;
        c.alpha_gamma = 1.0f;
        c.derive_alpha_from_luma = 0;
        return c;
    }

    /* Build a mask from an existing SDL surface. Returns 1 on success, 0 on failure.
     * Out frame is allocated (bits) and caller owns memory (freed via existing
     * reset path or manual free of frame->bits). Frame contents are zeroed before
     * population. */
    int rogue_pixel_mask_build_from_surface(void* sdl_surface, const RoguePixelMaskLoadConfig* cfg,
                                            struct RogueHitPixelMaskFrame* out_frame,
                                            RoguePixelMaskMetrics* out_metrics);

    /* Convenience: load image file (currently supports formats SDL_image / WIC
     * path already handles) and build mask. Returns 1 on success. */
    int rogue_pixel_mask_load_from_file(const char* path, const RoguePixelMaskLoadConfig* cfg,
                                        struct RogueHitPixelMaskFrame* out_frame,
                                        RoguePixelMaskMetrics* out_metrics);

    /* Asynchronous build helper. If a thread pool has been registered via
        rogue_pixel_mask_set_thread_pool() and use_thread_pool!=0, the work
        (including compression/mips/distance-fields) is queued and this returns 1 immediately.
        Falls back to synchronous build otherwise (also returns 1 on success). */
    int rogue_pixel_mask_build_async(void* sdl_surface, const RoguePixelMaskLoadConfig* cfg,
                                     struct RogueHitPixelMaskFrame* out_frame,
                                     RoguePixelMaskMetrics* out_metrics, int use_thread_pool);

    /* Register a shared thread pool to enable async builds. Safe to pass NULL to disable. */
    struct RogueThreadPool; /* forward decl */
    void rogue_pixel_mask_set_thread_pool(struct RogueThreadPool* tp);

#ifdef __cplusplus
}
#endif
