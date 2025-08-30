#include "map_debug.h"
#include "../../content/json_io.h"
#include "../app/app_state.h"
#include <stdlib.h>
#include <string.h>

extern struct RogueAppState g_app; /* defined in app_state.c */

static int clampi(int v, int lo, int hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

int rogue_map_debug_set_tile(int x, int y, unsigned char tile)
{
    if (!g_app.world_map.tiles)
        return -2;
    if (x < 0 || y < 0 || x >= g_app.world_map.width || y >= g_app.world_map.height)
        return -1;
    g_app.world_map.tiles[y * g_app.world_map.width + x] = tile;
    g_app.tile_sprite_lut_ready = 0; /* force lazy rebuild on next ensure */
    return 0;
}

int rogue_map_debug_brush_square(int cx, int cy, int radius, unsigned char tile)
{
    if (!g_app.world_map.tiles)
        return -2;
    if (radius < 0)
        radius = 0;
    int x0 = clampi(cx - radius, 0, g_app.world_map.width - 1);
    int y0 = clampi(cy - radius, 0, g_app.world_map.height - 1);
    int x1 = clampi(cx + radius, 0, g_app.world_map.width - 1);
    int y1 = clampi(cy + radius, 0, g_app.world_map.height - 1);
    for (int y = y0; y <= y1; ++y)
    {
        unsigned char* row = &g_app.world_map.tiles[y * g_app.world_map.width];
        for (int x = x0; x <= x1; ++x)
            row[x] = tile;
    }
    g_app.tile_sprite_lut_ready = 0;
    return 0;
}

int rogue_map_debug_brush_rect(int x0, int y0, int x1, int y1, unsigned char tile)
{
    if (!g_app.world_map.tiles)
        return -2;
    if (x0 > x1)
    {
        int t = x0;
        x0 = x1;
        x1 = t;
    }
    if (y0 > y1)
    {
        int t = y0;
        y0 = y1;
        y1 = t;
    }
    x0 = clampi(x0, 0, g_app.world_map.width - 1);
    x1 = clampi(x1, 0, g_app.world_map.width - 1);
    y0 = clampi(y0, 0, g_app.world_map.height - 1);
    y1 = clampi(y1, 0, g_app.world_map.height - 1);
    for (int y = y0; y <= y1; ++y)
    {
        unsigned char* row = &g_app.world_map.tiles[y * g_app.world_map.width];
        for (int x = x0; x <= x1; ++x)
            row[x] = tile;
    }
    g_app.tile_sprite_lut_ready = 0;
    return 0;
}

/* Very small JSON writer/reader for a compact map representation.
   Format: {"w":W,"h":H,"tiles":"<RLE>"}
   RLE string encodes runs as pairs: byte value and count (decimal) separated by ':' and ',' e.g.,
   "3:10,5:2,0:100". */

static int write_map_rle(char* buf, size_t cap)
{
    int w = g_app.world_map.width, h = g_app.world_map.height;
    if (!g_app.world_map.tiles || w <= 0 || h <= 0)
        return -1;
    size_t pos = 0;
    int n = snprintf(buf + pos, cap - pos, "{\"w\":%d,\"h\":%d,\"tiles\":\"", w, h);
    if (n < 0)
        return -1;
    pos += (size_t) n;
    size_t total = (size_t) w * (size_t) h;
    size_t i = 0;
    while (i < total)
    {
        unsigned char v = g_app.world_map.tiles[i];
        size_t run = 1;
        while (i + run < total && g_app.world_map.tiles[i + run] == v && run < 65535)
            run++;
        n = snprintf(buf + pos, cap - pos, "%u:%zu", (unsigned) v, run);
        if (n < 0 || (size_t) n >= cap - pos)
            return -2;
        pos += (size_t) n;
        i += run;
        if (i < total)
        {
            if (pos + 1 >= cap)
                return -2;
            buf[pos++] = ',';
        }
    }
    if (pos + 3 >= cap)
        return -2;
    buf[pos++] = '\"';
    buf[pos++] = '}';
    buf[pos] = '\0';
    return (int) pos;
}

int rogue_map_debug_save_json(const char* path)
{
    if (!path)
        return -1;
    char err[256];
    char* buf = (char*) malloc(64 * 1024);
    if (!buf)
        return -2;
    int n = write_map_rle(buf, 64 * 1024);
    if (n < 0)
    {
        free(buf);
        return -3;
    }
    int rc = json_io_write_atomic(path, buf, (size_t) n, err, (int) sizeof err);
    free(buf);
    return rc;
}

int rogue_map_debug_load_json(const char* path)
{
    if (!path)
        return -1;
    char* data = NULL;
    size_t len = 0;
    char err[256];
    if (json_io_read_file(path, &data, &len, err, (int) sizeof err) != 0)
        return -2;
    /* Minimal parse: expect keys w,h,tiles */
    int w = 0, h = 0;
    const char* p = data;
    const char* tstr = NULL;
    while (*p)
    {
        if (p[0] == '"' && strncmp(p + 1, "w\":", 3) == 0)
        {
            p += 4;
            w = atoi(p);
        }
        else if (p[0] == '"' && strncmp(p + 1, "h\":", 3) == 0)
        {
            p += 4;
            h = atoi(p);
        }
        else if (p[0] == '"' && strncmp(p + 1, "tiles\":\"", 8) == 0)
        {
            /* Advance to the first character inside the tiles RLE string. Pattern length from
               the opening quote is 9 (\"tiles\":\"), so add 9, not 10. */
            p += 9; /* advance to start of string after tiles":" */
            tstr = p;
            break;
        }
        ++p;
    }
    if (w <= 0 || h <= 0 || !tstr)
    {
        free(data);
        return -3;
    }
    /* Ensure world map buffer sized */
    if (g_app.world_map.width != w || g_app.world_map.height != h || !g_app.world_map.tiles)
    {
        /* Free and re-init to desired size */
        if (g_app.world_map.tiles)
        {
            free(g_app.world_map.tiles);
            g_app.world_map.tiles = NULL;
        }
        g_app.world_map.width = w;
        g_app.world_map.height = h;
        g_app.world_map.tiles = (unsigned char*) calloc((size_t) w * (size_t) h, 1);
        if (!g_app.world_map.tiles)
        {
            free(data);
            return -4;
        }
    }
    /* Decode RLE until closing quote */
    size_t total = (size_t) w * (size_t) h;
    size_t idx = 0;
    const char* s = tstr;
    while (*s && *s != '"')
    {
        /* parse value */
        unsigned val = 0;
        while (*s >= '0' && *s <= '9')
        {
            val = val * 10 + (unsigned) (*s - '0');
            ++s;
        }
        if (*s != ':')
            break;
        ++s;
        size_t run = 0;
        while (*s >= '0' && *s <= '9')
        {
            run = run * 10 + (size_t) (*s - '0');
            ++s;
        }
        if (idx + run > total)
        {
            free(data);
            return -5;
        }
        memset(&g_app.world_map.tiles[idx], (unsigned char) val, run);
        idx += run;
        if (*s == ',')
            ++s;
    }
    free(data);
    g_app.tile_sprite_lut_ready = 0;
    return idx == total ? 0 : -6;
}
