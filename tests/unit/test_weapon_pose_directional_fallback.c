#define SDL_MAIN_HANDLED
#include "game/weapon_pose.h"
#include <stdio.h>
#include <stdlib.h>
#if defined(_MSC_VER)
#include <direct.h>
#endif
int main()
{
#if defined(_MSC_VER)
    _mkdir("assets");
    _mkdir("assets\\weapons");
#else
    mkdir("assets", 0755);
    mkdir("assets/weapons", 0755);
#endif
    /* Only legacy generic pose present; directional ensure should fallback */
    FILE* f = NULL;
    fopen_s(&f, "../../assets/weapons/weapon_2_pose.json", "wb");
    if (!f)
    {
        printf("open_fail\n");
        return 1;
    }
    fprintf(f, "{\n  \"weapon_id\":2,\n  \"frames\":[\n");
    for (int i = 0; i < 8; i++)
        fprintf(
            f,
            "    {\"dx\":1,\"dy\":%d,\"angle\":0,\"scale\":1,\"pivot_x\":0.5,\"pivot_y\":0.5}%s\n",
            i, (i == 7) ? "" : " ,");
    fprintf(f, "  ]\n}\n");
    fclose(f);
    if (!rogue_weapon_pose_ensure_dir(2, 0))
    {
        printf("ensure_dir_fail\n");
        return 2;
    }
    const RogueWeaponPoseFrame* fr = rogue_weapon_pose_get_dir(2, 0, 6);
    if (!fr)
    {
        printf("get_dir_fail\n");
        return 3;
    }
    if ((int) fr->dy != 6)
    {
        printf("fallback_mismatch %f\n", fr->dy);
        return 4;
    }
    printf("weapon_pose_directional_fallback_ok\n");
    return 0;
}
