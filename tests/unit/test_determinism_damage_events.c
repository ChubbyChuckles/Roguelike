/* Phase M4.2 property-based determinism + M4.3 golden master replay */
#include "core/app_state.h"
#include "game/combat.h"
#include "util/determinism.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

extern void rogue_damage_event_record(unsigned short, unsigned char, unsigned char, int, int, int,
                                      unsigned char);

static void generate_sequence(int variant)
{
    rogue_damage_events_clear();
    for (int i = 0; i < 10; i++)
    {
        int raw = 50 + variant * 3 + i;
        int mitig = raw - (raw / 10);
        int over = (i == 9) ? 5 : 0;
        unsigned char crit = (i % 3) == 0;
        unsigned char exec = (i == 9) ? 1 : 0;
        rogue_damage_event_record((unsigned short) (100 + variant), 0, crit, raw, mitig, over,
                                  exec);
    }
}

int main(void)
{
    generate_sequence(1);
    RogueDamageEvent buf[64];
    int n = rogue_damage_events_snapshot(buf, 64);
    assert(n == 10);
    uint64_t h1 = rogue_damage_events_hash(buf, n);
    generate_sequence(1);
    RogueDamageEvent buf2[64];
    int n2 = rogue_damage_events_snapshot(buf2, 64);
    assert(n2 == 10);
    uint64_t h2 = rogue_damage_events_hash(buf2, n2);
    assert(h1 == h2);
    generate_sequence(2);
    RogueDamageEvent buf3[64];
    int n3 = rogue_damage_events_snapshot(buf3, 64);
    uint64_t h3 = rogue_damage_events_hash(buf3, n3);
    assert(h3 != h1);
    assert(rogue_damage_events_write_text("damage_golden.txt", buf2, n2) == 0);
    RogueDamageEvent loaded[64];
    int ln = rogue_damage_events_load_text("damage_golden.txt", loaded, 64);
    assert(ln == 10);
    assert(memcmp(buf2, loaded, sizeof(RogueDamageEvent) * 10) == 0);
    return 0;
}
