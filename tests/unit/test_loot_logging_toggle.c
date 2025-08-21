#include "core/loot/loot_logging.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    /* Set via API */
    rogue_loot_log_set_level(0);
    assert(rogue_loot_log_level() == 0);
    ROGUE_LOOT_LOG_INFO("should NOT appear (level 0)");
    ROGUE_LOOT_LOG_DEBUG("should NOT appear (level 0)");
    rogue_loot_log_set_level(2);
    assert(rogue_loot_log_level() == 2);
    ROGUE_LOOT_LOG_INFO("should appear (level 2)");
    ROGUE_LOOT_LOG_DEBUG("should appear (level 2)");
    /* Now test env override */
#if defined(_WIN32)
    _putenv("ROGUE_LOOT_LOG=1");
#else
    setenv("ROGUE_LOOT_LOG", "1", 1);
#endif
    rogue_loot_log_set_level(2); /* set to 2 then env should override to 1 */
    rogue_loot_logging_init_from_env();
    assert(rogue_loot_log_level() == 1);
    printf("LOOT_LOGGING_TOGGLE_OK level=%d\n", rogue_loot_log_level());
    return 0;
}
