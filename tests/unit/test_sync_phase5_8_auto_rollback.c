#include "../../src/core/integration/rollback_manager.h"
#include "../../src/core/integration/snapshot_manager.h"
#include "../../src/core/integration/transaction_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    int v;
} ARState;
static ARState g_state = {7};
static uint32_t g_ver = 0;

static int cap(void* u, void** out_data, size_t* out_size, uint32_t* out_version)
{
    (void) u;
    *out_size = sizeof(ARState);
    *out_data = malloc(*out_size);
    if (!*out_data)
        return -1;
    memcpy(*out_data, &g_state, *out_size);
    *out_version = ++g_ver;
    return 0;
}
static int restore(void* u, const void* data, size_t size, uint32_t v)
{
    (void) u;
    (void) v;
    if (size != sizeof(ARState))
        return -1;
    memcpy(&g_state, data, size);
    return 0;
}

// Tx participant that will fail prepare to trigger abort
static int prep_fail(void* u, int tx, char* e, size_t c, uint32_t* ov)
{
    (void) u;
    (void) tx;
    (void) ov;
    if (c)
        strncpy(e, "prep fail", c - 1);
    return -1;
}
static int prep_ok(void* u, int tx, char* e, size_t c, uint32_t* ov)
{
    (void) u;
    (void) tx;
    (void) e;
    (void) c;
    *ov = g_ver;
    return 0;
}
static int commit_ok(void* u, int tx)
{
    (void) u;
    (void) tx;
    return 0;
}
static int on_abort(void* u, int tx)
{
    (void) u;
    (void) tx;
    return 0;
}
static uint32_t getv(void* u)
{
    (void) u;
    return g_ver;
}

int main(void)
{
    // Register snapshot system and configure rollback
    RogueSnapshotDesc d = {.system_id = 201,
                           .name = "AR",
                           .capture = cap,
                           .restore = restore,
                           .user = NULL,
                           .max_size = sizeof(ARState)};
    if (rogue_snapshot_register(&d) != 0)
    {
        fprintf(stderr, "snap reg fail\n");
        return 2;
    }
    if (rogue_rollback_configure(201, 4) != 0)
    {
        fprintf(stderr, "rb cfg fail\n");
        return 2;
    }
    // map participant -> system for auto rollback
    if (rogue_rollback_map_participant(31, 201) != 0)
    {
        fprintf(stderr, "map fail\n");
        return 2;
    }

    // capture baseline and then mutate state
    g_state.v = 100;
    if (rogue_rollback_capture(201) != 0)
    {
        fprintf(stderr, "cap0 fail\n");
        return 2;
    }
    int baseline = g_state.v;
    g_state.v = -5;
    if (rogue_rollback_capture(201) != 0)
    {
        fprintf(stderr, "cap1 fail\n");
        return 2;
    }

    // Register two participants, one will fail prepare
    rogue_tx_reset_all();
    RogueTxParticipantDesc POK = {.participant_id = 30,
                                  .name = "OK",
                                  .user = NULL,
                                  .on_prepare = prep_ok,
                                  .on_commit = commit_ok,
                                  .on_abort = on_abort,
                                  .get_version = getv};
    RogueTxParticipantDesc PFL = {.participant_id = 31,
                                  .name = "FL",
                                  .user = NULL,
                                  .on_prepare = prep_fail,
                                  .on_commit = commit_ok,
                                  .on_abort = on_abort,
                                  .get_version = getv};
    if (rogue_tx_register_participant(&POK) != 0 || rogue_tx_register_participant(&PFL) != 0)
    {
        fprintf(stderr, "tx reg fail\n");
        return 2;
    }

    int tx = rogue_tx_begin(ROGUE_TX_ISO_READ_COMMITTED, 1000);
    if (tx < 0)
    {
        fprintf(stderr, "tx begin fail\n");
        return 2;
    }
    rogue_tx_mark(tx, 30);
    rogue_tx_mark(tx, 31);
    int rc = rogue_tx_commit(tx);
    if (rc == 0 || rogue_tx_get_state(tx) != ROGUE_TX_STATE_ABORTED)
    {
        fprintf(stderr, "tx not aborted\n");
        return 1;
    }

    // Verify auto rollback event recorded and state rewound to latest captured snapshot
    RogueRollbackStats st = {0};
    rogue_rollback_get_stats(&st);
    if (st.auto_rollbacks == 0 || st.restores_performed == 0)
    {
        fprintf(stderr, "no auto rollback\n");
        return 1;
    }

    const RogueRollbackEvent* evs = NULL;
    size_t evn = 0;
    if (rogue_rollback_events_get(&evs, &evn) != 0 || evn == 0)
    {
        fprintf(stderr, "no events\n");
        return 1;
    }
    int found_auto = 0;
    for (size_t i = 0; i < evn; i++)
        if (evs[i].system_id == 201 && evs[i].auto_triggered)
            found_auto = 1;
    if (!found_auto)
    {
        fprintf(stderr, "no auto event\n");
        return 1;
    }

    // The restore should have brought state back to the last captured snapshot (value -5)
    if (g_state.v != -5)
    {
        fprintf(stderr, "state not restored\n");
        return 1;
    }

    printf("SYNC_5_8_AUTO_ROLLBACK_OK\n");
    return 0;
}
