#ifndef ROGUE_CORE_APP_H
#define ROGUE_CORE_APP_H

#include <stdbool.h>

typedef struct RogueAppConfig {
    const char *window_title;
    int window_width;
    int window_height;
    int target_fps; /* 0 = uncapped */
} RogueAppConfig;

bool rogue_app_init(const RogueAppConfig *cfg);
/* Run full loop until exit requested */
void rogue_app_run(void);
/* Execute exactly one frame (for tests & tools) */
void rogue_app_step(void);
/* Total frames processed since init */
int rogue_app_frame_count(void);
void rogue_app_shutdown(void);

#endif
