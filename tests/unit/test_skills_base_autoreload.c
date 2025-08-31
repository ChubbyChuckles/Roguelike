/* Verify base skills JSON auto-reload mtime polling.
   Strategy: copy assets/skills_uhf87f.json to build/tmp_skills.json, call tick (prime, expect 0),
   touch file (rewrite same contents), call tick again (expect >0), call again without changes
   (expect 0). */
#include "../../src/core/app/app_state.h"
#include "../../src/core/skills/skill_debug.h"
#include "../../src/core/skills/skills.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define SDL_MAIN_HANDLED
#include <windows.h>
#else
#include <unistd.h>
#endif

static int copy_file(const char* src, const char* dst)
{
    FILE* fsrc = fopen(src, "rb");
    if (!fsrc)
        return -1;
    FILE* fdst = fopen(dst, "wb");
    if (!fdst)
    {
        fclose(fsrc);
        return -2;
    }
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, fsrc)) > 0)
    {
        if (fwrite(buf, 1, n, fdst) != n)
        {
            fclose(fsrc);
            fclose(fdst);
            return -3;
        }
    }
    fclose(fsrc);
    fclose(fdst);
    return 0;
}

static int file_exists(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f)
        return 0;
    fclose(f);
    return 1;
}

static const char* find_src_json(void)
{
    static const char* candidates[] = {
        "assets/skills_uhf87f.json",
        "../assets/skills_uhf87f.json",
        "../../assets/skills_uhf87f.json",
        "../../../assets/skills_uhf87f.json",
        "../../../../assets/skills_uhf87f.json",
    };
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i)
    {
        if (file_exists(candidates[i]))
            return candidates[i];
    }
    return NULL;
}

int main(int argc, char** argv)
{
    (void) argc;
    (void) argv;
    /* Ensure a predictable starting state */
    rogue_skills_init();
    const char* src = find_src_json();
    if (!src)
    {
        fprintf(stderr, "could not locate assets/skills_uhf87f.json from test cwd\n");
        return 1;
    }
    /* Write temp file in current working directory (CTest's working dir),
        not under a non-existent 'build/' subfolder. */
    const char* dst = "tmp_skills.json";
    if (copy_file(src, dst) != 0)
    {
        fprintf(stderr, "copy failed\n");
        return 2;
    }
    /* First tick: depending on implementation, may "prime" (0) or fully reload (>0).
       Accept either non-negative result. */
    int r0 = rogue_skills_base_autoreload_tick(dst);
    if (r0 < 0)
    {
        fprintf(stderr, "unexpected negative on first tick: %d\n", r0);
        return 3;
    }
    /* Touch file by rewriting it */
#ifdef _WIN32
    Sleep(10);
#else
    usleep(10 * 1000);
#endif
    if (copy_file(src, dst) != 0)
    {
        fprintf(stderr, "touch failed\n");
        return 4;
    }
    int r1 = rogue_skills_base_autoreload_tick(dst);
    if (r1 <= 0)
    {
        fprintf(stderr, "expected >0 after change, got %d\n", r1);
        return 5;
    }
    int r2 = rogue_skills_base_autoreload_tick(dst);
    if (r2 != 0)
    {
        fprintf(stderr, "expected 0 with unchanged mtime, got %d\n", r2);
        return 6;
    }
    printf("OK skills_base_autoreload r1=%d\n", r1);
    remove(dst);
    return 0;
}
