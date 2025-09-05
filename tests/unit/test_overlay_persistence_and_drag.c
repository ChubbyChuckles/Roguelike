#ifdef ROGUE_HAVE_SDL
#include "../../src/content/json_io.h"
#include "../../src/core/app/app_state.h"
#include "../../src/debug_overlay/overlay_core.h"
#include "../../src/debug_overlay/overlay_input.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

static void dummy_panel(void* user) { (void) user; }

static int file_contains(const char* path, const char* needle)
{
    char* data = NULL;
    size_t len = 0;
    char err[128];
    if (json_io_read_file(path, &data, &len, err, (int) sizeof err) != 0 || !data)
        return 0;
    int ok = (strstr(data, needle) != NULL);
    free(data);
    return ok;
}

int main(void)
{
    /* Start from a clean slate: it's okay if removal fails. */
    remove("build/overlay_layout.json");
    /* Ensure output directory exists for json_io writes. */
#ifdef _WIN32
    _mkdir("build");
#else
    mkdir("build", 0755);
#endif

    /* First run: register panel and toggle visibility to force a save. */
    overlay_init();
    int idx = overlay_register_panel("foo", "Foo", dummy_panel, NULL);
    assert(idx >= 0);
    /* Default should be hidden on first run. */
    assert(overlay_get_panel_visible("foo") == 0);
    /* Toggle to visible then hidden to persist visibility=0 with default geometry. */
    assert(overlay_set_panel_visible("foo", 1) == 0);
    assert(overlay_set_panel_visible("foo", 0) == 0);

    /* Ensure layout file was created and contains our panel id. */
    assert(file_contains("build/overlay_layout.json", "\"id\": \"foo\""));
    /* And visible flag recorded as 0. */
    assert(file_contains("build/overlay_layout.json", "\"visible\": 0"));
    overlay_shutdown();

    /* Second run: persisted hidden state should be honored on registration. */
    overlay_init();
    idx = overlay_register_panel("foo", "Foo", dummy_panel, NULL);
    assert(idx >= 0);
    assert(overlay_get_panel_visible("foo") == 0);

    /* Now test drag persistence when visible. */
    assert(overlay_set_panel_visible("foo", 1) == 0);
    /* Provide a viewport to clamp within; renderer can be null in headless. */
    g_app.viewport_w = 1280;
    g_app.viewport_h = 720;

    /* Simulate clicking on the title bar near default position (10,10). */
    overlay_input_begin_frame();
    overlay_input_simulate_mouse(15, 15, 1, 1); /* down+clicked */
    /* Begin the panel; should latch drag state. */
    assert(overlay_begin_panel_auto("foo", "Foo", 10, 10, 360) == 1);

    /* Drag to a new location while holding mouse. */
    overlay_input_begin_frame();
    overlay_input_simulate_mouse(100, 100, 1, 0); /* still down */
    assert(overlay_begin_panel_auto("foo", "Foo", 10, 10, 360) == 1);

    /* Release mouse to end drag and trigger save. */
    overlay_input_begin_frame();
    overlay_input_simulate_mouse(100, 100, 0, 0);
    assert(overlay_begin_panel_auto("foo", "Foo", 10, 10, 360) == 1);

    /* Verify saved layout reflects new position (approximate expected 100-5 offset). */
    assert(overlay_get_panel_visible("foo") == 1);
    /* We clicked at (15,15) with default (10,10), so drag offset is (5,5) -> saved x/y 95. */
    assert(file_contains("build/overlay_layout.json", "\"x\": 95, \"y\": 95"));

    overlay_shutdown();
    return 0;
}
#else
int main(void) { return 0; }
#endif
