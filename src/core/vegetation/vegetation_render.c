/* Rendering for vegetation */
#include "core/scene_drawlist.h"
#include "core/vegetation/vegetation_internal.h"
#include "graphics/tile_sprites.h"
#include <string.h>

#ifdef ROGUE_HAVE_SDL
typedef struct VegSheetTex
{
    char path[128];
    RogueTexture tex;
} VegSheetTex;
static VegSheetTex g_sheet_textures[64];
static int g_sheet_tex_count = 0;
static RogueSprite g_veg_sprite_pool[ROGUE_MAX_VEG_INSTANCES];
static int g_veg_sprite_pool_used = 0;

static RogueTexture* veg_get_texture(const char* path)
{
    for (int i = 0; i < g_sheet_tex_count; i++)
        if (strcmp(g_sheet_textures[i].path, path) == 0)
            return &g_sheet_textures[i].tex;
    if (g_sheet_tex_count >= (int) (sizeof g_sheet_textures / sizeof g_sheet_textures[0]))
        return NULL;
    VegSheetTex* slot = &g_sheet_textures[g_sheet_tex_count];
    memset(slot, 0, sizeof *slot);
    strncpy_s(slot->path, sizeof slot->path, path, _TRUNCATE);
    if (!rogue_texture_load(&slot->tex, path))
    {
        slot->path[0] = '\0';
        return NULL;
    }
    g_sheet_tex_count++;
    return &slot->tex;
}

static void veg_queue_instance(RogueVegetationInstance* v)
{
    RogueVegetationDef* d = &g_defs[v->def_index];
    RogueTexture* tex = veg_get_texture(d->image);
    if (!tex || !tex->handle)
        return;
    int tiles_w = (int) (d->tile_x2 - d->tile_x + 1);
    int tiles_h = (int) (d->tile_y2 - d->tile_y + 1);
    int sprite_px_w = tiles_w * g_app.tile_size;
    int sprite_px_h = tiles_h * g_app.tile_size;
    if (g_veg_sprite_pool_used >= ROGUE_MAX_VEG_INSTANCES)
        return;
    RogueSprite* temp = &g_veg_sprite_pool[g_veg_sprite_pool_used++];
    memset(temp, 0, sizeof *temp);
    temp->tex = tex;
    temp->sx = (int) d->tile_x * g_app.tile_size;
    temp->sy = (int) d->tile_y * g_app.tile_size;
    temp->sw = sprite_px_w;
    temp->sh = sprite_px_h;
    int world_center_px_x = (int) (v->x * g_app.tile_size - g_app.cam_x);
    int world_center_px_y = (int) (v->y * g_app.tile_size - g_app.cam_y);
    int dst_x = world_center_px_x - sprite_px_w / 2;
    int dst_y = world_center_px_y - sprite_px_h;
    int y_base = world_center_px_y;
    rogue_scene_drawlist_push_sprite(temp, dst_x, dst_y, y_base, 0, 255, 255, 255, 255);
}
#endif

void rogue_vegetation_render(void)
{
#ifdef ROGUE_HAVE_SDL
    if (!g_app.renderer)
        return;
    g_veg_sprite_pool_used = 0;
    for (int i = 0; i < g_instance_count; i++)
        veg_queue_instance(&g_instances[i]);
#endif
}
