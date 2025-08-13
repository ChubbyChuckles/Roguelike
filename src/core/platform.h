/* Platform/window initialization abstraction extracted from app.c */
#ifndef ROGUE_CORE_PLATFORM_H
#define ROGUE_CORE_PLATFORM_H

#include <stdbool.h>
#include "core/app.h"

bool rogue_platform_init(const RogueAppConfig* cfg);
void rogue_platform_apply_window_mode(void);
void rogue_platform_shutdown(void);

#endif
