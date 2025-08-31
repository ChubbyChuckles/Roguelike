#include "../../src/core/app/app.h"
#include "../../src/core/integration/state_validation_manager.h"
#include "../../src/core/integration/validation_wiring.h"
#include <assert.h>
#include <stdio.h>

int main()
{
    RogueAppConfig cfg = {"ValSmoke",
                          160,
                          90,
                          160,
                          90,
                          0,
                          0,
                          0,
                          1,
                          ROGUE_WINDOW_WINDOWED,
                          (RogueColor){0, 0, 0, 255}};
    assert(rogue_app_init(&cfg));
    rogue_validation_register_default_checks();
    // Force run; should not crash and should produce stats (warnings allowed).
    (void) rogue_validation_run_now(1);
    RogueValidationStats st;
    rogue_validation_get_stats(&st);
    fprintf(stderr, "VAL_SMOKE runs=%llu warn=%llu corrupt=%llu\n",
            (unsigned long long) st.runs_completed, (unsigned long long) st.warnings,
            (unsigned long long) st.corruptions_detected);
    // Basic sanity: at least one run completed.
    assert(st.runs_completed >= 1);
    rogue_app_shutdown();
    return 0;
}
