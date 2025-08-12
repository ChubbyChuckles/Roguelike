#include "core/app.h"
#include "util/log.h"

int main(void)
{
    RogueAppConfig cfg = {
        .window_title = "Roguelike", .window_width = 640, .window_height = 360, .target_fps = 60};
    if (!rogue_app_init(&cfg))
    {
        ROGUE_LOG_ERROR("Failed to initialize app");
        return 1;
    }
    rogue_app_run();
    rogue_app_shutdown();
    return 0;
}
