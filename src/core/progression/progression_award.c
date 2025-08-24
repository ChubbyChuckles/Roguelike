#include "progression_award.h"
#include "../app/app_state.h"
#include "../integration/event_bus.h"
#include <limits.h>
#include <string.h>

void rogue_award_xp(unsigned int xp_amount, unsigned int source_type, unsigned int source_id)
{
    if (xp_amount == 0)
        return;
    /* Publish XP_GAINED event */
    RogueEventPayload p;
    memset(&p, 0, sizeof(p));
    p.xp_gained.player_id = 0; /* single player */
    p.xp_gained.xp_amount = xp_amount;
    p.xp_gained.source_type = source_type;
    p.xp_gained.source_id = source_id;
    rogue_event_publish(ROGUE_EVENT_XP_GAINED, &p, ROGUE_EVENT_PRIORITY_NORMAL, 0x50524F47,
                        "progression");
    /* Apply XP */
    if ((unsigned long long) g_app.player.xp + (unsigned long long) xp_amount >
        (unsigned long long) INT_MAX)
        g_app.player.xp = INT_MAX;
    else
        g_app.player.xp += (int) xp_amount;
}
