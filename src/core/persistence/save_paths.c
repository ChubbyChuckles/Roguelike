#include "save_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <direct.h>
#include <process.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

static char slot_path[128];
static char autosave_path[128];
static char backup_path_buf[160];
static char json_path[128];
static char quicksave_path[128];

/* Optional prefix directory for save files (used to isolate tests). */
static char s_prefix[128] = ""; /* stored with trailing slash if non-empty */

static void rogue__ensure_prefix_dir(void)
{
    if (s_prefix[0] == '\0')
        return;
    /* Strip trailing slash for mkdir */
    char tmp[128];
#if defined(_MSC_VER)
    strncpy_s(tmp, sizeof tmp, s_prefix, _TRUNCATE);
#else
    strncpy(tmp, s_prefix, sizeof tmp);
    tmp[sizeof tmp - 1] = '\0';
#endif
    size_t n = strlen(tmp);
    while (n > 0 && (tmp[n - 1] == '/' || tmp[n - 1] == '\\'))
    {
        tmp[n - 1] = '\0';
        n--;
    }
#if defined(_WIN32)
    _mkdir(tmp);
#else
    mkdir(tmp, 0700);
#endif
}

void rogue_save_paths_set_prefix(const char* prefix)
{
    if (!prefix || !*prefix)
    {
        s_prefix[0] = '\0';
        return;
    }
    /* Copy and ensure it ends with a slash */
    size_t len = strlen(prefix);
    if (len >= sizeof s_prefix)
        len = sizeof s_prefix - 1;
    memcpy(s_prefix, prefix, len);
    s_prefix[len] = '\0';
    if (len > 0 && s_prefix[len - 1] != '/' && s_prefix[len - 1] != '\\')
    {
        if (len + 1 < sizeof s_prefix)
        {
            s_prefix[len] = '/';
            s_prefix[len + 1] = '\0';
        }
        else
        {
            /* Truncate without adding slash if no room */
        }
    }
    rogue__ensure_prefix_dir();
}

void rogue_save_paths_set_prefix_tests(void)
{
    const char* envp = NULL;
#if defined(_MSC_VER)
    char* dup = NULL;
    size_t dsz = 0;
    if (_dupenv_s(&dup, &dsz, "ROGUE_TEST_SAVE_DIR") == 0 && dup)
    {
        envp = dup;
    }
#else
    envp = getenv("ROGUE_TEST_SAVE_DIR");
#endif
    if (envp && *envp)
    {
        rogue_save_paths_set_prefix(envp);
        return;
    }
#if defined(_WIN32)
    unsigned pid = (unsigned) _getpid();
#else
    unsigned pid = (unsigned) getpid();
#endif
    /* Use a stable per-process directory under cwd */
    char buf[128];
    snprintf(buf, sizeof buf, "test_saves_%u", pid);
    rogue_save_paths_set_prefix(buf);
#if defined(_MSC_VER)
    if (dup)
        free(dup);
#endif
}

const char* rogue_build_slot_path(int slot)
{
    if (s_prefix[0])
        snprintf(slot_path, sizeof slot_path, "%ssave_slot_%d.sav", s_prefix, slot);
    else
        snprintf(slot_path, sizeof slot_path, "save_slot_%d.sav", slot);
    return slot_path;
}

const char* rogue_build_autosave_path(int logical)
{
    int ring = logical % ROGUE_AUTOSAVE_RING;
    if (s_prefix[0])
        snprintf(autosave_path, sizeof autosave_path, "%sautosave_%d.sav", s_prefix, ring);
    else
        snprintf(autosave_path, sizeof autosave_path, "autosave_%d.sav", ring);
    return autosave_path;
}

const char* rogue_build_backup_path(int slot, uint32_t ts)
{
    if (s_prefix[0])
        snprintf(backup_path_buf, sizeof backup_path_buf, "%ssave_slot_%d_%u.bak", s_prefix, slot,
                 ts);
    else
        snprintf(backup_path_buf, sizeof backup_path_buf, "save_slot_%d_%u.bak", slot, ts);
    return backup_path_buf;
}

const char* rogue_build_json_path(int slot)
{
    if (s_prefix[0])
        snprintf(json_path, sizeof json_path, "%ssave_slot_%d.json", s_prefix, slot);
    else
        snprintf(json_path, sizeof json_path, "save_slot_%d.json", slot);
    return json_path;
}

const char* rogue_build_quicksave_path(void)
{
    if (s_prefix[0])
        snprintf(quicksave_path, sizeof quicksave_path, "%squicksave.sav", s_prefix);
    else
        snprintf(quicksave_path, sizeof quicksave_path, "quicksave.sav");
    return quicksave_path;
}
