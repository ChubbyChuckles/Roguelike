/* Unit: test_asset_placeholder_enforcement
   Verifies the placeholder asset existence probe succeeds for the canonical path
   and fails for an obviously invalid variant. Pure filesystem checks (no SDL). */

#include "util/asset_placeholder.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    /* Positive path: canonical placeholder must exist in repo. */
    int ok = rogue_asset_placeholder_exists();
    if (!ok)
    {
        fprintf(stderr, "ERROR: placeholder asset '%s' not found in any fallback prefix.\n",
                ROGUE_ASSET_PLACEHOLDER_PATH);
    }
    assert(ok && "placeholder.png must exist (add a tiny stub PNG under assets/)");

    /* Negative path: deliberately misspelled filename should not exist. */
    assert(!rogue_asset_path_exists("assets/placeholder_DOES_NOT_EXIST.png"));
    return 0;
}
