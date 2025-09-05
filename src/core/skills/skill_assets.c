#include "skill_assets.h"
#include "../../util/path_utils.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define ROGUE_PATH_SEP '\\'
#else
#include <dirent.h>
#define ROGUE_PATH_SEP '/'
#endif

static int path_join3(char* out, int out_sz, const char* a, const char* b, const char* c)
{
    int n = snprintf(out, out_sz, "%s%c%s%c%s", a, ROGUE_PATH_SEP, b, ROGUE_PATH_SEP, c);
    if (n < 0 || n >= out_sz)
        return 0;
    return 1;
}

int rogue_skill_assets_dir_for(const char* skill_name, const char* slot, char* out, int out_sz)
{
    if (!skill_name || !*skill_name || !slot || !*slot || !out || out_sz <= 0)
        return 0;
    /* relative path without assets/ prefix */
    int n =
        snprintf(out, out_sz, "skills%c%s%c%s", ROGUE_PATH_SEP, skill_name, ROGUE_PATH_SEP, slot);
    if (n < 0 || n >= out_sz)
        return 0;
    return 1;
}

static int file_exists(const char* path)
{
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, path, "rb");
#else
    f = fopen(path, "rb");
#endif
    if (f)
    {
        fclose(f);
        return 1;
    }
    return 0;
}

/* If rel like "icons/fireball.png", resolve via assets prefixes; if already absolute under
   assets/, just check existence. Writes full path (assets/ prefix) into out when found. */
static int resolve_asset_path(const char* rel_or_full, char* out, int out_sz)
{
    if (!rel_or_full || !*rel_or_full)
    {
        if (out && out_sz > 0)
            out[0] = '\0';
        return 0;
    }
    if (strncmp(rel_or_full, "assets/", 7) == 0)
    {
        /* Already full */
        if (file_exists(rel_or_full))
        {
#if defined(_MSC_VER)
            strncpy_s(out, out_sz, rel_or_full, _TRUNCATE);
#else
            strncpy(out, rel_or_full, out_sz - 1);
            out[out_sz - 1] = '\0';
#endif
            return 1;
        }
        if (out && out_sz > 0)
            out[0] = '\0';
        return 0;
    }
    /* relative: try assets/ prefixes */
    char full[512];
    if (rogue_find_asset_path(rel_or_full, full, (int) sizeof full))
    {
#if defined(_MSC_VER)
        strncpy_s(out, out_sz, full, _TRUNCATE);
#else
        strncpy(out, full, out_sz - 1);
        out[out_sz - 1] = '\0';
#endif
        return 1;
    }
    if (out && out_sz > 0)
        out[0] = '\0';
    return 0;
}

int rogue_skill_assets_validate(const char* skill_name, const RogueSkillVisualParams* vis,
                                RogueSkillAssetReport* rep)
{
    if (!skill_name || !*skill_name || !vis || !rep)
        return -1;
    memset(rep, 0, sizeof *rep);
    rep->cast_exists =
        resolve_asset_path(vis->cast_sprite_sheet, rep->cast_path, (int) sizeof rep->cast_path);
    rep->projectile_exists = resolve_asset_path(vis->projectile_sprite, rep->projectile_path,
                                                (int) sizeof rep->projectile_path);
    rep->impact_exists =
        resolve_asset_path(vis->impact_sprite, rep->impact_path, (int) sizeof rep->impact_path);
    rep->aoe_exists =
        resolve_asset_path(vis->aoe_sprite, rep->aoe_path, (int) sizeof rep->aoe_path);
    /* Icon convention: skills/<skill>/icon/icon.png by default if not explicitly set via vis */
    char icon_rel[256];
    char dir_rel[256];
    if (rogue_skill_assets_dir_for(skill_name, "icon", dir_rel, (int) sizeof dir_rel))
    {
        int n = snprintf(icon_rel, sizeof icon_rel, "%s%cicon.png", dir_rel, ROGUE_PATH_SEP);
        if (n > 0 && n < (int) sizeof icon_rel)
        {
            rep->icon_exists =
                resolve_asset_path(icon_rel, rep->icon_path, (int) sizeof rep->icon_path);
        }
    }
    return 0;
}

int rogue_skill_assets_count_png_sequence(const char* skill_name, const char* slot,
                                          const char* stem)
{
    if (!skill_name || !*skill_name || !slot || !*slot)
        return -1;
    char dir_rel[256];
    if (!rogue_skill_assets_dir_for(skill_name, slot, dir_rel, (int) sizeof dir_rel))
        return -1;
    /* Resolve directory under assets/ */
    char dir_full[512];
    char dir_rel_with_sep[300];
    int n = snprintf(dir_rel_with_sep, sizeof dir_rel_with_sep, "%s%c", dir_rel, ROGUE_PATH_SEP);
    if (n <= 0 || n >= (int) sizeof dir_rel_with_sep)
        return -1;
    /* Try to find this directory via assets path helper by probing a sentinel file */
    if (!rogue_find_asset_path(dir_rel_with_sep, dir_full, (int) sizeof dir_full))
    {
        /* Best-effort: prefix assets/ and hope CWD is project root or build/ */
#if defined(_MSC_VER)
        strncpy_s(dir_full, sizeof dir_full, "assets/", _TRUNCATE);
        strncat_s(dir_full, sizeof dir_full, dir_rel, _TRUNCATE);
#else
        strncpy(dir_full, "assets/", sizeof dir_full - 1);
        dir_full[sizeof dir_full - 1] = '\0';
        strncat(dir_full, dir_rel, sizeof dir_full - strlen(dir_full) - 1);
#endif
    }

    const char* base = (stem && *stem) ? stem : slot;
    int count = 0;
#ifdef _WIN32
    char pattern[640];
    snprintf(pattern, sizeof pattern, "%s%c%s_*.png", dir_full, ROGUE_PATH_SEP, base);
    struct _finddata_t fd;
    intptr_t h = _findfirst(pattern, &fd);
    if (h == -1)
        return 0;
    do
    {
        /* Basic suffix check _NNN.png */
        const char* name = fd.name;
        size_t len = strlen(name);
        if (len >= 8 && _stricmp(name + len - 4, ".png") == 0)
            count++;
    } while (_findnext(h, &fd) == 0);
    _findclose(h);
#else
    DIR* d = opendir(dir_full);
    if (!d)
        return 0;
    struct dirent* ent;
    char prefix[128];
    snprintf(prefix, sizeof prefix, "%s_", base);
    while ((ent = readdir(d)))
    {
        const char* name = ent->d_name;
        size_t len = strlen(name);
        if (len >= 8 && strncmp(name, prefix, strlen(prefix)) == 0 &&
            strcmp(name + len - 4, ".png") == 0)
            count++;
    }
    closedir(d);
#endif
    return count;
}
