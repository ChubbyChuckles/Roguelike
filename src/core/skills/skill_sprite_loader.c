#include "skill_sprite_loader.h"
#include <string.h>

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
