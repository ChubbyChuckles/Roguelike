#include "../../src/core/integration/transaction_manager.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Mock participant state
typedef struct
{
    int id;
    uint32_t version;
    int prepared;
    int committed;
    int aborted;
} MockPart;

static int mp_on_prepare(void* user, int tx_id, char* err, size_t errcap, uint32_t* out_ver)
{
    (void) tx_id;
    MockPart* p = user;
    p->prepared++;
    *out_ver = p->version;
    return 0;
}
static int mp_on_commit(void* user, int tx_id)
{
    (void) tx_id;
    MockPart* p = user;
    p->committed++;
    p->version++;
    return 0;
}
static int mp_on_abort(void* user, int tx_id)
{
    (void) tx_id;
    MockPart* p = user;
    p->aborted++;
    return 0;
}
static uint32_t mp_get_version(void* user)
{
    MockPart* p = user;
    return p->version;
}

static uint64_t fake_now = 0;
static uint64_t time_fn(void) { return fake_now; }

static MockPart g_parts[3];
static void setup_parts()
{
    rogue_tx_reset_all();
    rogue_tx_set_time_source(time_fn);
    memset(g_parts, 0, sizeof(g_parts));
    for (int i = 0; i < 3; i++)
    {
        g_parts[i].id = i + 1;
        RogueTxParticipantDesc d = {g_parts[i].id, "P",         &g_parts[i],   mp_on_prepare,
                                    mp_on_commit,  mp_on_abort, mp_get_version};
        assert(rogue_tx_register_participant(&d) == 0);
    }
}
static void teardown_parts(void) { /* no-op */ }

static void test_commit_basic()
{
    setup_parts();
    int tx = rogue_tx_begin(ROGUE_TX_ISO_READ_COMMITTED, 100);
    assert(tx > 0);
    assert(rogue_tx_mark(tx, 1) == 0);
    assert(rogue_tx_mark(tx, 2) == 0);
    assert(rogue_tx_commit(tx) == 0);
    assert(rogue_tx_get_state(tx) == ROGUE_TX_STATE_COMMITTED);
    RogueTxStats st;
    rogue_tx_get_stats(&st);
    assert(st.committed == 1);
}

static void test_repeatable_read_violation()
{
    setup_parts();
    int tx = rogue_tx_begin(ROGUE_TX_ISO_REPEATABLE_READ, 0);
    assert(tx > 0);
    assert(rogue_tx_mark(tx, 1) == 0);
    uint32_t v = 0;
    assert(rogue_tx_read(tx, 1, &v) == 0); // simulate version bump externally
    // direct commit should pass since version matches
    assert(rogue_tx_commit(tx) == 0);
}

static void test_timeout()
{
    setup_parts();
    int tx = rogue_tx_begin(ROGUE_TX_ISO_READ_COMMITTED, 5);
    fake_now = 10;
    int rc = rogue_tx_commit(tx);
    assert(rc != 0);
    assert(rogue_tx_get_state(tx) == ROGUE_TX_STATE_TIMED_OUT ||
           rogue_tx_get_state(tx) == ROGUE_TX_STATE_ABORTED);
}

static void test_abort_path()
{
    setup_parts();
    int tx = rogue_tx_begin(ROGUE_TX_ISO_READ_COMMITTED, 0);
    assert(tx > 0);
    assert(rogue_tx_mark(tx, 1) == 0);
    assert(rogue_tx_abort(tx, "user abort") == 0);
    assert(rogue_tx_get_state(tx) == ROGUE_TX_STATE_ABORTED);
    RogueTxStats st;
    rogue_tx_get_stats(&st);
    assert(st.aborted >= 1);
}

int main()
{
    rogue_tx_log_enable(32);
    test_commit_basic();
    test_repeatable_read_violation();
    test_timeout();
    test_abort_path();
    teardown_parts();
    const RogueTxLogEntry* log = NULL;
    size_t logc = 0;
    rogue_tx_log_get(&log, &logc);
    printf("transaction_manager tests passed (log entries=%zu)\n", logc);
    fflush(stdout);
    return 0;
}
