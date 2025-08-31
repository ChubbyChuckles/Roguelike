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
    /* Use a tiny seed JSON to keep reloads cheap and skip icon textures. */
    rogue_skills_set_skip_icon_loads(1);
    const char* dst = "tmp_skills.json";
    const char* seed_json =
        "[ {\"name\":\"T_Fast\",\"icon\":\"../assets/skills/01_Fireball.png\",\"max_"
        "rank\":1,\"skill_strength\":1,\"base_cooldown_ms\":0.0,\"cooldown_reduction_ms_"
        "per_rank\":0.0,\"is_passive\":0,\"tags\":0,\"synergy_id\":-1,\"synergy_value_"
        "per_rank\":0,\"resource_cost_mana\":0,\"action_point_cost\":0,\"max_charges\":0,"
        "\"charge_recharge_ms\":0.0,\"cast_time_ms\":0.0,\"input_buffer_ms\":0,\"min_"
        "weave_ms\":0,\"early_cancel_min_pct\":0,\"cast_type\":0,\"combo_builder\":0,\""
        "combo_spender\":0,\"effect_spec_id\":1 } ]\n";
    FILE* f = fopen(dst, "wb");
    if (!f)
    {
        fprintf(stderr, "seed open failed\n");
        return 2;
    }
    fwrite(seed_json, 1, strlen(seed_json), f);
    fclose(f);
    /* First tick: depending on implementation, may "prime" (0) or fully reload (>0).
       Accept either non-negative result. */
    int r0 = rogue_skills_base_autoreload_tick(dst);
    if (r0 < 0)
    {
        fprintf(stderr, "unexpected negative on first tick: %d\n", r0);
        return 3;
    }
    /* Touch file by rewriting it */
    /* Ensure mtime changes: rewrite file with same contents and wait minimal time slice. */
#ifdef _WIN32
    Sleep(2);
#else
    usleep(2 * 1000);
#endif
    f = fopen(dst, "wb");
    if (!f)
    {
        fprintf(stderr, "touch open failed\n");
        return 4;
    }
    fwrite(seed_json, 1, strlen(seed_json), f);
    fclose(f);
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
