/* Pixel-based hit detection (Slice A: structures + loader scaffolding) */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RogueHitPixelMaskFrame {
    int width;
    int height;
    int origin_x;
    int origin_y;
    int pitch_words; /* 32-bit words per scanline */
    uint32_t* bits;  /* packed bits (row-major) */
} RogueHitPixelMaskFrame;

typedef struct RogueHitPixelMaskSet {
    int weapon_id;
    int frame_count; /* expected 8 */
    RogueHitPixelMaskFrame frames[8];
    int ready;
} RogueHitPixelMaskSet;

/* Global toggle (debug key will flip) */
extern int g_hit_use_pixel_masks;

/* Ensure mask set for weapon id (lazy generate simple placeholder if assets not yet integrated). */
RogueHitPixelMaskSet* rogue_hit_pixel_masks_ensure(int weapon_id);

/* Release all allocated masks (test teardown) */
void rogue_hit_pixel_masks_reset_all(void);

/* Utility: set bit (x,y) */
static inline void rogue_hit_mask_set(RogueHitPixelMaskFrame* f,int x,int y){ if(!f||!f->bits) return; if((unsigned)x>=(unsigned)f->width || (unsigned)y>=(unsigned)f->height) return; int idx = y * f->pitch_words + (x>>5); uint32_t m = 1u << (x & 31); f->bits[idx] |= m; }
/* Utility: test bit (x,y) */
static inline int rogue_hit_mask_test(const RogueHitPixelMaskFrame* f,int x,int y){ if(!f||!f->bits) return 0; if((unsigned)x>=(unsigned)f->width || (unsigned)y>=(unsigned)f->height) return 0; int idx = y * f->pitch_words + (x>>5); uint32_t m = 1u << (x & 31); return (f->bits[idx] & m)!=0; }

#ifdef __cplusplus
}
#endif
