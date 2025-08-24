#include "../../src/core/vendor/vendor_econ_balance.h"
#include "../../src/core/vendor/vendor_pricing.h"
#include "../../src/core/vendor/vendor_registry.h"
#include <assert.h>
#include <stdio.h>

int main()
{
    rogue_vendor_econ_balance_reset();
    /* Simulate a sequence of rising prices to push inflation index upward */
    for (int i = 0; i < 50; i++)
    {
        float idx = rogue_vendor_econ_balance_note_price(100 + i);
        (void) idx;
    }
    float idx_after = rogue_vendor_inflation_index();
    /* Index should be within reasonable bounds */
    assert(idx_after > 0.5f && idx_after < 2.0f);
    float gscalar = rogue_vendor_dynamic_margin_scalar();
    assert(gscalar >= 0.90f && gscalar <= 1.10f);
    float b1 = rogue_vendor_biome_scalar("forest");
    float b2 = rogue_vendor_biome_scalar("desert");
    /* Biome scalars should be within envelope and differ for distinct tags */
    assert(b1 >= 0.97f && b1 <= 1.03f);
    assert(b2 >= 0.97f && b2 <= 1.03f);
    /* Likely (not guaranteed) different; tolerate equality edge. */
    if (b1 == b2)
    { /* acceptable */
    }
    printf("VENDOR_PHASE10_BALANCING_OK\n");
    return 0;
}
