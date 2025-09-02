#include "../../src/debug_overlay/overlay_core.h"
#include "../../src/debug_overlay/overlay_input.h"
#include "../../src/debug_overlay/widgets/overlay_widgets.h"
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static double now_ms(void)
{
    static LARGE_INTEGER freq = {0};
    LARGE_INTEGER t;
    if (freq.QuadPart == 0)
        QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t);
    return (double) t.QuadPart * 1000.0 / (double) freq.QuadPart;
}
#else
#include <time.h>
static double now_ms(void)
{
    struct timespec ts;
#ifdef CLOCK_MONOTONIC
    clock_gettime(CLOCK_MONOTONIC, &ts);
#else
    clock_gettime(CLOCK_REALTIME, &ts);
#endif
    return (double) ts.tv_sec * 1000.0 + (double) ts.tv_nsec / 1.0e6;
}
#endif

static void run_bench_config(const char* label, int row_h, int row_pad, int visible_rows,
                             int frames, int total_rows)
{
    int sort_col = 0, sort_dir = 1, selected = -1;
    const char* headers[] = {"A", "B", "C"};
    const int cols = 3;
    double t0 = now_ms();
    for (int f = 0; f < frames; ++f)
    {
        overlay_input_begin_frame();
        if (overlay_begin_panel("Perf", 10, 10, 800))
        {
            (void) overlay_table_begin("bench", headers, cols, &sort_col, &sort_dir, NULL);
            overlay_table_set_row_style(row_h, row_pad);
            for (int i = 0; i < visible_rows; ++i)
            {
                char a[16], b[16], c[16];
                snprintf(a, sizeof a, "a%d", i);
                snprintf(b, sizeof b, "b%d", i);
                snprintf(c, sizeof c, "c%d", i);
                const char* row[] = {a, b, c};
                (void) overlay_table_row(row, cols, i, &selected);
            }
            (void) overlay_table_scrollbar(total_rows, visible_rows, &selected);
            overlay_table_end();
            overlay_end_panel();
        }
    }
    double t1 = now_ms();
    double total_ms = t1 - t0;
    double avg_ms = (frames > 0) ? (total_ms / (double) frames) : 0.0;
    printf("OVERLAY_TABLE_PERF %s: frames=%d vis=%d row_h=%d pad=%d total_ms=%.3f avg_ms=%.3f\n",
           label, frames, visible_rows, row_h, row_pad, total_ms, avg_ms);
}

int main(void)
{
#if ROGUE_ENABLE_DEBUG_OVERLAY
    overlay_set_enabled(1);
    /* Keep this short and deterministic; no assertions, just metrics. */
    const int total_rows = 50000;
    const int frames = 200; /* small but enough to average */
    const int visible = 50; /* moderate table height */

    run_bench_config("baseline", 18, 2, visible, frames, total_rows);
    run_bench_config("tuned", 16, 1, visible, frames, total_rows);
#endif
    return 0;
}
