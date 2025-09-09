#include "vendor_security.h"
#include <string.h>

#ifndef ROGUE_VENDOR_SECURITY_PURCHASE_RING
#define ROGUE_VENDOR_SECURITY_PURCHASE_RING 64
#endif

static uint32_t g_purchase_ts[ROGUE_VENDOR_SECURITY_PURCHASE_RING];
static int g_purchase_head = 0;
static int g_purchase_count = 0;

/* Negotiation spam per vendor: track recent attempts timestamps to apply extended lockouts. */
#ifndef ROGUE_VENDOR_SECURITY_MAX_VENDORS
#define ROGUE_VENDOR_SECURITY_MAX_VENDORS 128
#endif

typedef struct RogueVendorNegotiationGuard
{
    int vendor_def_index;
    uint32_t lockout_until_ms; /* extended lockout */
    uint32_t recent_attempts_ts[8];
    int attempts_count;
} RogueVendorNegotiationGuard;

static RogueVendorNegotiationGuard g_nego_guards[ROGUE_VENDOR_SECURITY_MAX_VENDORS];
static int g_nego_guard_count = 0;

static RogueVendorNegotiationGuard* guard_for(int vendor_def_index)
{
    for (int i = 0; i < g_nego_guard_count; ++i)
    {
        if (g_nego_guards[i].vendor_def_index == vendor_def_index)
            return &g_nego_guards[i];
    }
    if (g_nego_guard_count >= ROGUE_VENDOR_SECURITY_MAX_VENDORS)
        return 0;
    RogueVendorNegotiationGuard* g = &g_nego_guards[g_nego_guard_count++];
    memset(g, 0, sizeof *g);
    g->vendor_def_index = vendor_def_index;
    return g;
}

void rogue_vendor_security_reset(void)
{
    memset(g_purchase_ts, 0, sizeof g_purchase_ts);
    g_purchase_head = 0;
    g_purchase_count = 0;
    memset(g_nego_guards, 0, sizeof g_nego_guards);
    g_nego_guard_count = 0;
}

void rogue_vendor_security_note_purchase(uint32_t now_ms)
{
    g_purchase_ts[g_purchase_head] = now_ms;
    g_purchase_head = (g_purchase_head + 1) % ROGUE_VENDOR_SECURITY_PURCHASE_RING;
    if (g_purchase_count < ROGUE_VENDOR_SECURITY_PURCHASE_RING)
        g_purchase_count++;
}

float rogue_vendor_security_spree_scalar(void)
{
    /* If >10 purchases in last 10s, scale rises up to +5%. */
    const uint32_t WINDOW_MS = 10000u;
    const int THRESH = 10;
    const int CAP = 20; /* at 20 in window -> full +5% */
    int recent = 0;
    if (g_purchase_count == 0)
        return 1.0f;
    uint32_t now = g_purchase_ts[(g_purchase_head + ROGUE_VENDOR_SECURITY_PURCHASE_RING - 1) %
                                 ROGUE_VENDOR_SECURITY_PURCHASE_RING];
    for (int i = 0; i < g_purchase_count; ++i)
    {
        uint32_t ts = g_purchase_ts[i];
        if (now >= ts && now - ts <= WINDOW_MS)
            recent++;
    }
    if (recent <= THRESH)
        return 1.0f;
    if (recent > CAP)
        recent = CAP;
    float t = (float) (recent - THRESH) / (float) (CAP - THRESH); /* 0..1 */
    float inc = 0.05f * t;                                        /* up to +5% */
    return 1.0f + inc;
}

float rogue_vendor_security_exploit_scalar(void) { return rogue_vendor_security_spree_scalar(); }

int rogue_vendor_security_negotiation_allowed(int vendor_def_index, uint32_t now_ms)
{
    RogueVendorNegotiationGuard* g = guard_for(vendor_def_index);
    if (!g)
        return 1;
    if (now_ms < g->lockout_until_ms)
        return 0;
    /* count attempts in last 10s; if >5, apply 20s lockout */
    const uint32_t WINDOW_MS = 10000u;
    const int MAX_ATTEMPTS = 5;
    int recent = 0;
    for (int i = 0; i < g->attempts_count; ++i)
    {
        uint32_t ts = g->recent_attempts_ts[i];
        if (now_ms >= ts && now_ms - ts <= WINDOW_MS)
            recent++;
    }
    if (recent >= MAX_ATTEMPTS)
    {
        g->lockout_until_ms = now_ms + 20000u;
        g->attempts_count = 0; /* reset counter for next window */
        return 0;
    }
    /* record this attempt */
    if (g->attempts_count < (int) (sizeof g->recent_attempts_ts / sizeof g->recent_attempts_ts[0]))
        g->recent_attempts_ts[g->attempts_count++] = now_ms;
    else
    {
        /* simple shift left when full */
        memmove(&g->recent_attempts_ts[0], &g->recent_attempts_ts[1],
                (g->attempts_count - 1) * sizeof(uint32_t));
        g->recent_attempts_ts[g->attempts_count - 1] = now_ms;
    }
    return 1;
}

uint32_t rogue_vendor_security_negotiation_lockout_remaining(int vendor_def_index, uint32_t now_ms)
{
    RogueVendorNegotiationGuard* g = guard_for(vendor_def_index);
    if (!g)
        return 0;
    if (now_ms >= g->lockout_until_ms)
        return 0;
    return g->lockout_until_ms - now_ms;
}
