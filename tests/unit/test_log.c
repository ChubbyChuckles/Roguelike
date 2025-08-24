#include "../../src/util/log.h"
#include <stdio.h>

int main(void)
{
    /* Invoke each log macro for structural coverage */
    ROGUE_LOG_DEBUG("debug %d", 1);
    ROGUE_LOG_INFO("info %s", "x");
    ROGUE_LOG_WARN("warn");
    ROGUE_LOG_ERROR("error");
    return 0;
}
