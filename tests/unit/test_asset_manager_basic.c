/* test_asset_manager_basic.c - validates initial Phase 3 asset manager scaffold + SDL_image/audio
 * extensions */
#include "../../src/asset/asset_manager.h"
#include "../../src/asset/path_utils.h"
#include <stdio.h>
#include <string.h>

static int assert_true(int expr, const char* msg)
{
    if (!expr)
    {
        printf("FAIL: %s\n", msg);
        return 0;
    }
    return 1;
}

int main(void)
{
    if (!rogue_asset_manager_init(NULL))
    {
        printf("FAIL: init\n");
        return 1;
    }
    int idx = rogue_asset_manager_acquire_texture("assets/placeholder.png");
    if (!assert_true(idx >= 0, "acquire placeholder"))
        return 1;
    const RogueAssetTexture* tex = rogue_asset_manager_get(idx);
    if (!assert_true(tex && tex->ref_count == 1, "placeholder refcount 1"))
        return 1;
    int idx2 = rogue_asset_manager_acquire_texture("assets/placeholder.png");
    if (!assert_true(idx2 == idx, "dedupe same id"))
        return 1;
    tex = rogue_asset_manager_get(idx);
    if (!assert_true(tex && tex->ref_count == 2, "refcount increment"))
        return 1;
    rogue_asset_manager_release_texture(idx);
    tex = rogue_asset_manager_get(idx);
    if (!assert_true(tex && tex->ref_count == 1, "refcount decremented"))
        return 1;
    rogue_asset_manager_release_texture(idx);
    /* entry should be removed */
    tex = rogue_asset_manager_get(idx);
    if (!assert_true(!tex, "entry removed after final release"))
        return 1;

    /* Audio acquire (will negative-cache fail if file missing) */
    int aud_missing = rogue_asset_manager_acquire_audio("assets/missing_does_not_exist.wav");
    if (!assert_true(aud_missing >= 0, "audio slot allocated even if missing"))
        return 1;
    const RogueAssetAudio* au = rogue_asset_manager_get_audio(aud_missing);
    if (!assert_true(au != NULL, "audio entry present"))
        return 1;

    char norm[128];
    rogue_path_join("assets\\sprites", "player.png", norm, sizeof(norm));
    if (!assert_true(strstr(norm, "assets/sprites/player.png") != NULL, "path join normalize"))
        return 1;

    printf("OK test_asset_manager_basic\n");
    return 0;
}
