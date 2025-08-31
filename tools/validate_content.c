#include "../src/core/app/app.h"
#include "../src/core/integration/state_validation_manager.h"
#include "../src/core/integration/validation_wiring.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char* argv0)
{
    fprintf(stderr,
            "Usage: %s [--force-all] [--fail-on-warn] [--quiet]\n"
            "Runs registered content and cross-rule validators and reports results.\n"
            "Exit codes: 0=OK, 1=Warnings (when --fail-on-warn), 2=Corruption detected.\n",
            argv0);
}

static const char* sev_name(RogueValidationSeverity s)
{
    switch (s)
    {
    case ROGUE_VALID_OK:
        return "OK";
    case ROGUE_VALID_WARN:
        return "WARN";
    case ROGUE_VALID_CORRUPT:
        return "CORRUPT";
    default:
        return "?";
    }
}

int main(int argc, char** argv)
{
    int force_all = 0;
    int fail_on_warn = 0;
    int quiet = 0;
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--force-all") == 0)
            force_all = 1;
        else if (strcmp(argv[i], "--fail-on-warn") == 0)
            fail_on_warn = 1;
        else if (strcmp(argv[i], "--quiet") == 0)
            quiet = 1;
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            print_usage(argv[0]);
            return 0;
        }
        else
        {
            fprintf(stderr, "Unknown arg: %s\n", argv[i]);
            print_usage(argv[0]);
            return 3;
        }
    }

    RogueAppConfig cfg = {"ValidateContent",
                          320,
                          180,
                          320,
                          180,
                          0, /* uncapped */
                          0, /* vsync off */
                          0, /* allow resize */
                          1, /* integer scale */
                          ROGUE_WINDOW_WINDOWED,
                          (RogueColor){0, 0, 0, 255}};
    if (!rogue_app_init(&cfg))
    {
        fprintf(stderr, "Failed to init app for validation.\n");
        return 4;
    }

    // Ensure default validators are registered.
    rogue_validation_register_default_checks();

    // Run immediately.
    (void) rogue_validation_run_now(force_all);

    RogueValidationStats stats;
    rogue_validation_get_stats(&stats);

    const RogueValidationEvent* evs = NULL;
    size_t n = 0;
    (void) rogue_validation_events_get(&evs, &n);

    if (!quiet)
    {
        fprintf(stdout,
                "Validation summary: systems=%llu skipped=%llu cross=%llu warn=%llu corrupt=%llu "
                "runs=%llu/%llu time_ns=%llu\n",
                (unsigned long long) stats.system_validations_run,
                (unsigned long long) stats.system_validations_skipped_unchanged,
                (unsigned long long) stats.cross_rule_runs, (unsigned long long) stats.warnings,
                (unsigned long long) stats.corruptions_detected,
                (unsigned long long) stats.runs_completed,
                (unsigned long long) stats.runs_initiated,
                (unsigned long long) stats.total_ns_spent);
        for (size_t i = 0; i < n; ++i)
        {
            const RogueValidationEvent* e = &evs[i];
            fprintf(stdout, "[%s] sys=%d code=%u msg=%s\n", sev_name(e->severity), e->system_id,
                    e->code, e->message);
        }
    }

    int exit_code = 0;
    if (stats.corruptions_detected > 0)
        exit_code = 2;
    else if (fail_on_warn && stats.warnings > 0)
        exit_code = 1;

    rogue_app_shutdown();
    return exit_code;
}
