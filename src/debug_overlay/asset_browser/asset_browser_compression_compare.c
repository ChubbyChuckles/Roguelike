/* asset_browser_compression_compare.c - extracted Compression Comparison UI */
#include "asset_browser_compression_compare.h"
#include "../../asset/asset_manager.h"
#include "../overlay_core.h"
#include "../widgets/overlay_widgets.h"
#include "asset_browser_state.h"
#include "asset_browser_util.h"
#include <string.h>
#ifdef _WIN32
#include <sys/stat.h>
#else
#include <sys/stat.h>
#endif

#if ROGUE_ENABLE_DEBUG_OVERLAY
#define g_ab_state (*rogue_asset_browser_state())
static void ab_copy_safe(char* dst, size_t cap, const char* src)
{
    size_t i = 0;
    if (!dst || cap == 0)
        return;
    if (!src)
    {
        dst[0] = '\0';
        return;
    }
    while (src[i] && i + 1 < cap)
    {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}
void rogue_asset_browser_draw_compression_compare(RogueAssetManager* m)
{
    if (!g_ab_state.show_compression_compare || g_ab_state.tab_index != 1)
        return;
    overlay_label("[Compression Comparison] Probe alternative on-disk formats (.ktx2/.ktx/.dds)");
    int sel = g_ab_state.selected_row;
    if (sel >= 0 && (uint32_t) sel < m->texture_count)
    {
        const RogueAssetTexture* tex = &m->textures[sel];
        char original[260];
        ab_copy_safe(original, sizeof original, tex->path);
        const char* dot = strrchr(original, '.');
        size_t base_len = dot ? (size_t) (dot - original) : strlen(original);
        char base[260];
        if (base_len >= sizeof base)
            base_len = sizeof base - 1;
        memcpy(base, original, base_len);
        base[base_len] = '\0';
        const char* exts[] = {".ktx2", ".ktx", ".dds"};
        uint64_t sizes[3] = {0, 0, 0};
        uint64_t orig_size = 0;
        for (int pass = -1; pass < 3; ++pass)
        {
            char path[320];
            if (pass == -1)
            {
                ab_copy_safe(path, sizeof path, original);
            }
            else
            {
                size_t bl = strlen(base);
                size_t el = strlen(exts[pass]);
                if (bl + el + 1 < sizeof path)
                {
                    memcpy(path, base, bl);
                    memcpy(path + bl, exts[pass], el + 1);
                }
                else
                    path[0] = '\0';
            }
            if (path[0] && rogue_asset_file_exists(path))
            {
#ifdef _WIN32
                struct _stat s;
                if (_stat(path, &s) == 0)
                {
                    if (pass == -1)
                        orig_size = (uint64_t) s.st_size;
                    else
                        sizes[pass] = (uint64_t) s.st_size;
                }
#else
                struct stat s;
                if (stat(path, &s) == 0)
                {
                    if (pass == -1)
                        orig_size = (uint64_t) s.st_size;
                    else
                        sizes[pass] = (uint64_t) s.st_size;
                }
#endif
            }
        }
        char line2[160];
        snprintf(line2, sizeof line2, "Original (%s) size: %llu bytes", original,
                 (unsigned long long) orig_size);
        overlay_label(line2);
        for (int i = 0; i < 3; i++)
        {
            if (sizes[i])
            {
                double pct = (orig_size && orig_size > sizes[i])
                                 ? (100.0 - (double) sizes[i] * 100.0 / (double) orig_size)
                                 : 0.0;
                snprintf(line2, sizeof line2, "%s present: %llu bytes (%.1f%% smaller)", exts[i],
                         (unsigned long long) sizes[i], pct);
            }
            else
            {
                snprintf(line2, sizeof line2, "%s missing", exts[i]);
            }
            overlay_label(line2);
        }
        if (orig_size && (sizes[0] || sizes[1] || sizes[2]))
            overlay_label(
                "Toggle 'Prefer Compressed' in metrics panel to auto-substitute where found.");
        else
            overlay_label("No alternative compressed variants found next to this texture.");
    }
    else
    {
        overlay_label("Select a texture row to compare.");
    }
}
#endif
