#define SDL_MAIN_HANDLED
#include "../../src/game/weapon_pose.h"
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
    /* Write directional side pose file */
    FILE* f = NULL;
    fopen_s(&f, "../../assets/weapons/weapon_1_side_pose.json", "wb");
    if (!f)
    {
        printf("open_fail\n");
        return 1;
    }
    fprintf(f, "{\n  \"weapon_id\":1,\n  \"direction\":\"side\",\n  \"frames\":[\n");
    for (int i = 0; i < 8; i++)
        fprintf(
            f,
            "    {\"dx\":%d,\"dy\":0,\"angle\":0,\"scale\":1,\"pivot_x\":0.5,\"pivot_y\":0.5}%s\n",
            i * 2, (i == 7) ? "" : " ,");
    fprintf(f, "  ]\n}\n");
    fclose(f);
    if (!rogue_weapon_pose_ensure_dir(1, 2))
    {
        printf("ensure_dir_fail\n");
        return 2;
    }
    const RogueWeaponPoseFrame* fr = rogue_weapon_pose_get_dir(1, 2, 4);
    if (!fr)
    {
        printf("get_dir_fail\n");
        return 3;
    }
    if ((int) fr->dx != 8)
    {
        printf("dx_mismatch %f\n", fr->dx);
        return 4;
    }
    float flipped = rogue_weapon_pose_effective_dx(fr, 1); /* facing left */
    if ((int) flipped != -8)
    {
        printf("mirror_fail %f\n", flipped);
        return 5;
    }
    printf("weapon_pose_directional_ok\n");
    return 0;
}
