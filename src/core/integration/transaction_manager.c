#include "transaction_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ROGUE_TX_MAX
#define ROGUE_TX_MAX 256
#endif
#ifndef ROGUE_TX_PARTICIPANT_MAX
#define ROGUE_TX_PARTICIPANT_MAX 64
#endif

typedef struct TxParticipant
{
    RogueTxParticipantDesc desc;
} TxParticipant;

typedef struct TxRecord
{
    int id;
    RogueTxState state;
    RogueTxIsolation isolation;
    uint32_t timeout_ms;
    uint64_t start_ms;
    uint32_t participant_mask; // bit per participant index
    uint32_t read_versions[ROGUE_TX_PARTICIPANT_MAX];
    uint32_t prepare_versions[ROGUE_TX_PARTICIPANT_MAX];
    char abort_reason[128];
} TxRecord;

static TxParticipant g_participants[ROGUE_TX_PARTICIPANT_MAX];
static uint32_t g_participant_count = 0;
static TxRecord g_txs[ROGUE_TX_MAX];
static int g_next_tx_id = 1;
static RogueTxStats g_stats;

// logging
typedef struct LogEntry
{
    RogueTxLogEntry e;
} LogEntry;
static LogEntry* g_log = NULL;
static size_t g_log_cap = 0;
static size_t g_log_count = 0;
static size_t g_log_head = 0;

static uint64_t default_time_fn(void) { return 0; }
static RogueTxTimeFn g_time_fn = default_time_fn;

static TxRecord* find_tx(int tx_id)
{
    for (int i = 0; i < ROGUE_TX_MAX; i++)
        if (g_txs[i].id == tx_id)
            return &g_txs[i];
    return NULL;
}
static int participant_index(int participant_id)
{
    for (uint32_t i = 0; i < g_participant_count; i++)
        if (g_participants[i].desc.participant_id == participant_id)
            return (int) i;
    return -1;
}

static void log_state(int tx_id, RogueTxState from, RogueTxState to, RogueTxIsolation iso,
                      uint32_t participants)
{
    if (!g_log)
        return;
    LogEntry* le = &g_log[g_log_head];
    le->e.tx_id = tx_id;
    le->e.from_state = from;
    le->e.to_state = to;
    le->e.isolation = iso;
    le->e.timestamp_ms = g_time_fn();
    le->e.participants_marked = participants;
    g_log_head = (g_log_head + 1) % g_log_cap;
    if (g_log_count < g_log_cap)
        g_log_count++;
    g_stats.log_entries = g_log_count;
}

int rogue_tx_register_participant(const RogueTxParticipantDesc* desc)
{
    if (!desc || !desc->on_prepare || !desc->on_commit ||
        g_participant_count >= ROGUE_TX_PARTICIPANT_MAX)
        return -1;
    for (uint32_t i = 0; i < g_participant_count; i++)
        if (g_participants[i].desc.participant_id == desc->participant_id)
            return -2;
    g_participants[g_participant_count++].desc = *desc;
    return 0;
}

int rogue_tx_begin(RogueTxIsolation isolation, uint32_t timeout_ms)
{
    int slot = -1;
    for (int i = 0; i < ROGUE_TX_MAX; i++)
        if (g_txs[i].id == 0)
        {
            slot = i;
            break;
        }
    if (slot < 0)
        return -1;
    TxRecord* r = &g_txs[slot];
    memset(r, 0, sizeof(*r));
    r->id = g_next_tx_id++;
    r->state = ROGUE_TX_STATE_ACTIVE;
    r->isolation = isolation;
    r->timeout_ms = timeout_ms;
    r->start_ms = g_time_fn();
    g_stats.started++; // active_peak recalculated
    uint64_t active = 0;
    for (int i = 0; i < ROGUE_TX_MAX; i++)
        if (g_txs[i].id && g_txs[i].state == ROGUE_TX_STATE_ACTIVE)
            active++;
    if (active > g_stats.active_peak)
        g_stats.active_peak = active;
    log_state(r->id, ROGUE_TX_STATE_UNUSED, r->state, isolation, 0);
    return r->id;
}

int rogue_tx_mark(int tx_id, int participant_id)
{
    TxRecord* r = find_tx(tx_id);
    if (!r || r->state != ROGUE_TX_STATE_ACTIVE)
        return -1;
    int pi = participant_index(participant_id);
    if (pi < 0)
        return -2;
    r->participant_mask |= (1u << pi);
    return 0;
}

int rogue_tx_read(int tx_id, int participant_id, uint32_t* out_version)
{
    TxRecord* r = find_tx(tx_id);
    if (!r || r->state != ROGUE_TX_STATE_ACTIVE)
        return -1;
    int pi = participant_index(participant_id);
    if (pi < 0)
        return -2;
    uint32_t v = 0;
    if (g_participants[pi].desc.get_version)
        v = g_participants[pi].desc.get_version(g_participants[pi].desc.user);
    if (out_version)
        *out_version = v;
    if (r->isolation == ROGUE_TX_ISO_REPEATABLE_READ)
        r->read_versions[pi] = v;
    return 0;
}

static int check_timeout(TxRecord* r)
{
    if (!r || r->state != ROGUE_TX_STATE_ACTIVE)
        return 0;
    if (r->timeout_ms == 0)
        return 0;
    uint64_t now = g_time_fn();
    if (now - r->start_ms > r->timeout_ms)
    {
        r->state = ROGUE_TX_STATE_TIMED_OUT;
        g_stats.timeouts++;
        log_state(r->id, ROGUE_TX_STATE_ACTIVE, r->state, r->isolation, r->participant_mask);
        return -1;
    }
    return 0;
}

int rogue_tx_commit(int tx_id)
{
    TxRecord* r = find_tx(tx_id);
    if (!r)
        return -1;
    if (r->state != ROGUE_TX_STATE_ACTIVE)
        return -2;
    if (check_timeout(r) < 0)
        return -3; // isolation validation
    for (uint32_t i = 0; i < g_participant_count; i++)
    {
        if (r->participant_mask & (1u << i))
        {
            if (r->isolation == ROGUE_TX_ISO_REPEATABLE_READ && r->read_versions[i])
            {
                uint32_t cur = 0;
                if (g_participants[i].desc.get_version)
                    cur = g_participants[i].desc.get_version(g_participants[i].desc.user);
                if (cur != r->read_versions[i])
                {
                    g_stats.isolation_violations++;
                    rogue_tx_abort(tx_id, "isolation violation");
                    return -4;
                }
            }
        }
    }
    // prepare phase
    r->state = ROGUE_TX_STATE_PREPARING;
    log_state(r->id, ROGUE_TX_STATE_ACTIVE, r->state, r->isolation, r->participant_mask);
    for (uint32_t i = 0; i < g_participant_count; i++)
    {
        if (r->participant_mask & (1u << i))
        {
            uint32_t ver = 0;
            char err[64] = {0};
            int pr = g_participants[i].desc.on_prepare(g_participants[i].desc.user, r->id, err,
                                                       sizeof(err), &ver);
            if (pr != 0)
            {
                g_stats.prepare_failures++;
                size_t ml = strlen(err);
                if (ml >= sizeof(r->abort_reason))
                    ml = sizeof(r->abort_reason) - 1;
                memcpy(r->abort_reason, err, ml);
                r->abort_reason[ml] = '\0';
                rogue_tx_abort(tx_id, "prepare failure");
                return -5;
            }
            r->prepare_versions[i] = ver;
        }
    }
    // commit phase
    r->state = ROGUE_TX_STATE_COMMITTING;
    log_state(r->id, ROGUE_TX_STATE_PREPARING, r->state, r->isolation, r->participant_mask);
    for (uint32_t i = 0; i < g_participant_count; i++)
    {
        if (r->participant_mask & (1u << i))
        {
            int cr = g_participants[i].desc.on_commit(g_participants[i].desc.user, r->id);
            if (cr != 0)
            {
                rogue_tx_abort(tx_id, "commit failure");
                return -6;
            }
        }
    }
    r->state = ROGUE_TX_STATE_COMMITTED;
    g_stats.committed++;
    log_state(r->id, ROGUE_TX_STATE_COMMITTING, r->state, r->isolation, r->participant_mask);
    return 0;
}

int rogue_tx_abort(int tx_id, const char* reason)
{
    TxRecord* r = find_tx(tx_id);
    if (!r)
        return -1;
    if (r->state == ROGUE_TX_STATE_ABORTED || r->state == ROGUE_TX_STATE_COMMITTED)
        return 0;
    RogueTxState prev = r->state;
    r->state = ROGUE_TX_STATE_ABORTED;
    if (reason)
    {
        size_t ml = strlen(reason);
        if (ml >= sizeof(r->abort_reason))
            ml = sizeof(r->abort_reason) - 1;
        memcpy(r->abort_reason, reason, ml);
        r->abort_reason[ml] = '\0';
    }
    for (uint32_t i = 0; i < g_participant_count; i++)
    {
        if (r->participant_mask & (1u << i))
        {
            if (g_participants[i].desc.on_abort)
                g_participants[i].desc.on_abort(g_participants[i].desc.user, r->id);
        }
    }
    g_stats.aborted++;
    g_stats.rollback_invocations++;
    log_state(r->id, prev, r->state, r->isolation, r->participant_mask);
    return 0;
}

RogueTxState rogue_tx_get_state(int tx_id)
{
    TxRecord* r = find_tx(tx_id);
    return r ? r->state : ROGUE_TX_STATE_UNUSED;
}
void rogue_tx_set_time_source(RogueTxTimeFn fn) { g_time_fn = fn ? fn : default_time_fn; }
void rogue_tx_get_stats(RogueTxStats* out)
{
    if (!out)
        return;
    *out = g_stats;
}
int rogue_tx_log_get(const RogueTxLogEntry** out_entries, size_t* out_count)
{
    if (!out_entries || !out_count)
        return -1;
    if (!g_log)
    {
        *out_entries = NULL;
        *out_count = 0;
        return 0;
    }
    *out_entries = &g_log[0].e;
    *out_count = g_log_count;
    return 0;
}
int rogue_tx_log_enable(size_t capacity)
{
    if (g_log)
    {
        free(g_log);
        g_log = NULL;
        g_log_cap = g_log_count = g_log_head = 0;
    }
    if (capacity == 0)
        return 0;
    g_log = calloc(capacity, sizeof(LogEntry));
    if (!g_log)
        return -1;
    g_log_cap = capacity;
    return 0;
}
void rogue_tx_dump(void* fptr)
{
    FILE* f = fptr ? (FILE*) fptr : stdout;
    fprintf(f,
            "[tx] started=%llu committed=%llu aborted=%llu prep_fail=%llu iso_vio=%llu "
            "timeouts=%llu rollback=%llu peak_active=%llu log=%llu\n",
            (unsigned long long) g_stats.started, (unsigned long long) g_stats.committed,
            (unsigned long long) g_stats.aborted, (unsigned long long) g_stats.prepare_failures,
            (unsigned long long) g_stats.isolation_violations,
            (unsigned long long) g_stats.timeouts,
            (unsigned long long) g_stats.rollback_invocations,
            (unsigned long long) g_stats.active_peak, (unsigned long long) g_stats.log_entries);
    for (int i = 0; i < ROGUE_TX_MAX; i++)
    {
        if (g_txs[i].id)
        {
            fprintf(f, " tx id=%d state=%d iso=%d mask=%u reason=%s\n", g_txs[i].id, g_txs[i].state,
                    g_txs[i].isolation, g_txs[i].participant_mask, g_txs[i].abort_reason);
        }
    }
}
void rogue_tx_reset_all(void)
{
    memset(g_txs, 0, sizeof(g_txs));
    memset(&g_stats, 0, sizeof(g_stats));
    g_next_tx_id = 1;
    if (g_log)
    {
        memset(g_log, 0, sizeof(LogEntry) * g_log_cap);
        g_log_count = g_log_head = 0;
    }
    g_participant_count = 0;
    memset(g_participants, 0, sizeof(g_participants));
}
