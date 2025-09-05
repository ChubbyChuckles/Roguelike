#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "../../src/core/skills/skill_assets.h"

static void make_dirs(const char* path)
{
#ifdef _WIN32
    _mkdir("assets");
    _mkdir("assets\\skills");
    char buf[256];
    snprintf(buf, sizeof buf, "assets\\skills\\%s", path);
    _mkdir(buf);
#else
    mkdir("assets", 0755);
    mkdir("assets/skills", 0755);
    char buf[256];
    snprintf(buf, sizeof buf, "assets/skills/%s", path);
    mkdir(buf, 0755);
#endif
}

static void write_dummy_png(const char* path)
{
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, path, "wb");
#else
    f = fopen(path, "wb");
#endif
    if (f)
    {
        /* Not a real PNG, but we only count files, not parse them */
        const char* data = "PNG";
        fwrite(data, 1, 3, f);
        fclose(f);
    }
}

int main(void)
{
    /* Dir builder */
    char rel[128];
    int ok = rogue_skill_assets_dir_for("fireball", "cast", rel, (int) sizeof rel);
    if (!ok || strstr(rel, "fireball") == NULL)
        return 1;

    /* Create a fake sequence: assets/skills/fireball/cast/cast_001.png, _002, _010 */
    make_dirs("fireball");
#ifdef _WIN32
    _mkdir("assets\\skills\\fireball\\cast");
    _mkdir("assets\\skills\\fireball\\icon");
    write_dummy_png("assets\\skills\\fireball\\cast\\cast_001.png");
    write_dummy_png("assets\\skills\\fireball\\cast\\cast_002.png");
    write_dummy_png("assets\\skills\\fireball\\cast\\cast_010.png");
    write_dummy_png("assets\\skills\\fireball\\icon\\icon.png");
#else
    mkdir("assets/skills/fireball/cast", 0755);
    mkdir("assets/skills/fireball/icon", 0755);
    write_dummy_png("assets/skills/fireball/cast/cast_001.png");
    write_dummy_png("assets/skills/fireball/cast/cast_002.png");
    write_dummy_png("assets/skills/fireball/cast/cast_010.png");
    write_dummy_png("assets/skills/fireball/icon/icon.png");
#endif

    int count = rogue_skill_assets_count_png_sequence("fireball", "cast", NULL);
    if (count < 3)
        return 2;

    RogueSkillVisualParams vis;
    memset(&vis, 0, sizeof vis);
    /* Set relative path that should resolve via assets/ */
    strncpy(vis.cast_sprite_sheet, "skills/fireball/cast/cast_001.png",
            sizeof vis.cast_sprite_sheet - 1);
    RogueSkillAssetReport rep;
    if (rogue_skill_assets_validate("fireball", &vis, &rep) != 0)
        return 3;
    if (!rep.cast_exists)
        return 4;
    if (!rep.icon_exists)
        return 5;
    return 0;
}
