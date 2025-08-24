#include "../../src/core/crafting/crafting.h"
#include "../../src/core/crafting/crafting_automation.h"
#include "../../src/core/crafting/material_refine.h"
#include "../../src/core/crafting/material_registry.h"
#include "../../src/core/inventory/inventory.h"
#include <stdio.h>
#include <string.h>

/* Minimal stubs / helpers already provided by core when linking rogue_core. */

static void seed_minimal_recipe_if_none(void)
{
    if (rogue_craft_recipe_count() == 0)
    { /* rely on existing helper in crafting.c via recipe count side effect */
        (void) rogue_craft_recipe_at(0);
    }
}

int main(void)
{
    seed_minimal_recipe_if_none();
    const RogueCraftRecipe* r = rogue_craft_recipe_at(0);
    if (!r)
    {
        printf("P9_FAIL no recipe\n");
        return 1;
    }
    RogueCraftPlanReq reqs[16];
    int rc = rogue_craft_plan_requirements(r, 2, 0, 0, reqs, 16);
    if (rc <= 0)
    {
        printf("P9_FAIL planner0\n");
        return 1;
    }
    int any_missing = 0;
    for (int i = 0; i < rc; i++)
    {
        if (reqs[i].missing > 0)
        {
            any_missing = 1;
            break;
        }
    }
    if (!any_missing && rc > 0 && reqs[0].have > 0)
    {
        rogue_inventory_consume(reqs[0].item_def, 1);
    }
    int idle_mat = rogue_craft_idle_recommend_material();
    if (rogue_material_count() > 0)
    {
        int mdef = 0;
        rogue_material_quality_add(mdef, 0, 40);
        rogue_material_quality_add(mdef, 5, 20);
    }
    RogueRefineSuggestion sugs[8];
    int sc = rogue_craft_refine_suggestions(50, 10, 5, sugs, 8);
    if (sc < 0)
    {
        printf("P9_FAIL refine\n");
        return 1;
    }
    double sval = 0, cnet = 0;
    int rec = rogue_craft_decision_salvage_vs_craft(r->output_def, 1, r, &sval, &cnet);
    if (rec < 0)
        rec = 0;
    int nodes[8];
    int nrc = rogue_craft_gather_route(nodes, 8);
    if (nrc < 0)
    {
        printf("P9_FAIL route\n");
        return 1;
    }
    fprintf(stderr, "[TEST] Phase9 about to print token\n");
    printf("CRAFT_P9_OK planner=%d refine=%d decision=%d route=%d idle=%d\n", rc, sc, rec, nrc,
           idle_mat >= 0 ? idle_mat : 0);
    fflush(stdout);
    fflush(stderr);
    return 0;
}
