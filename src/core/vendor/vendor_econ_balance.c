#include "core/vendor/vendor_econ_balance.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

/* Inflation index computed as EWMA of observed basket average relative to baseline nominal price
 * (we treat nominal baseline as implicit 1). */
static double g_inflation_index = 1.0; /* start neutral */
static double g_price_ewma = 0.0;
static int g_price_count = 0;

static double ewma_alpha(void) { return 0.05; }

void rogue_vendor_econ_balance_reset(void)
{
    g_inflation_index = 1.0;
    g_price_ewma = 0.0;
    g_price_count = 0;
}

float rogue_vendor_econ_balance_note_price(int price)
{
    if (price <= 0)
        return (float) g_inflation_index;
    double p = (double) price;
    if (g_price_count == 0)
    {
        g_price_ewma = p;
        g_price_count = 1;
    }
    else
    {
        double a = ewma_alpha();
        g_price_ewma = g_price_ewma * (1.0 - a) + p * a;
        g_price_count++;
    }
    /* Inflation index scaled by ewma relative to moving nominal anchor (first price). */
    double anchor = (g_price_count > 0 ? (g_price_ewma > 0 ? g_price_ewma : 1.0) : 1.0);
    g_inflation_index = g_price_ewma / (anchor > 0 ? anchor : 1.0);
    if (g_inflation_index < 0.1)
        g_inflation_index = 0.1;
    if (g_inflation_index > 5.0)
        g_inflation_index = 5.0;
    return (float) g_inflation_index;
}

float rogue_vendor_inflation_index(void) { return (float) g_inflation_index; }

float rogue_vendor_dynamic_margin_scalar(void)
{ /* If inflation index drifts above 1, we gently reduce margin; below 1 we gently increase. */
    double idx = g_inflation_index;
    double dev = idx - 1.0; /* clamp adjustment range */
    if (dev > 0.5)
        dev = 0.5;
    if (dev < -0.5)
        dev = -0.5; /* map dev [-0.5,0.5] -> scalar ~ [0.95,1.05] using linear slope 0.1 */
    double scalar = 1.0 - dev * 0.1;
    if (scalar < 0.90)
        scalar = 0.90;
    if (scalar > 1.10)
        scalar = 1.10;
    return (float) scalar;
}

/* Simple FNV1a hash of biome tags string to derive deterministic small variance in [0.97,1.03]. */
static unsigned int fnv1a_str(const char* s)
{
    unsigned int h = 0x811C9DC5u;
    if (!s)
        return h;
    while (*s)
    {
        h ^= (unsigned char) (*s++);
        h *= 0x01000193u;
    }
    return h;
}
float rogue_vendor_biome_scalar(const char* biome_tags)
{
    unsigned int h = fnv1a_str(biome_tags);
    unsigned int r = (h >> 8) & 0xFFFFu;
    double t = (double) r / 65535.0; /* t in [0,1] */
    return (float) (0.97 + t * 0.06);
}
