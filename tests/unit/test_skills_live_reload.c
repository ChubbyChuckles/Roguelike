#include "../../src/content/json_io.h"
#include "../../src/core/skills/skill_debug.h"
#include <stdio.h>
#include <string.h>

/* Minimal headless test: writes overrides file, ticks auto-reload, verifies non-negative apply. */
int main(void)
{
    const char* path = "skills_overrides_live.json"; /* cwd is build/ under ctest */
    /* Create a tiny overrides JSON that touches skill 0 if present; include name for tolerance. */
    const char* json = "[{\"skill_id\":0,\"name\":\"_\",\"base_cooldown_ms\":123.0,\"cd_red_ms_per_"
                       "rank\":1.0,\"cast_time_ms\":45.0}]";
    char err[256];
    int rc = json_io_write_atomic(path, json, (int) strlen(json), err, (int) sizeof err);
    if (rc != 0)
    {
        fprintf(stderr, "write failed: %s\n", err);
        return 1;
    }
    /* First tick should detect and apply (>=0). */
    int applied = rogue_skill_debug_autoreload_tick(path);
    if (applied < 0)
    {
        fprintf(stderr, "autoreload apply error: %d\n", applied);
        return 2;
    }
    /* Second tick should be a no-op (0) since mtime unchanged. */
    int applied2 = rogue_skill_debug_autoreload_tick(path);
    if (applied2 != 0)
    {
        fprintf(stderr, "expected no-op second tick, got %d\n", applied2);
        return 3;
    }
    printf("OK test_skills_live_reload: applied=%d no-op=%d\n", applied, applied2);
    return 0;
}
