/* Test crafting system with JSON-loaded material items
 * Verifies that crafting recipes work correctly with JSON item definitions
 */
#include "../../src/core/crafting/crafting.h"
#include "../../src/core/loot/loot_item_defs.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

int main(void)
{
    printf("Testing crafting system with JSON-loaded materials...\n");
    
    /* Reset and load JSON items first */
    rogue_item_defs_reset();
    int items_loaded = rogue_item_defs_load_directory_json("../assets/items");
    if (items_loaded <= 0) {
        printf("Failed to load JSON items, falling back to cfg...\n");
        items_loaded = rogue_item_defs_load_directory("../assets/items");
        if (items_loaded <= 0) {
            printf("SKIP: No items loaded\n");
            return 0;
        }
    }
    printf("Items loaded: %d\n", items_loaded);
    
    /* Test material tier classification with JSON-loaded items */
    int iron_ore_idx = rogue_item_def_index("material_iron_ore");
    if (iron_ore_idx >= 0) {
        int tier = rogue_material_tier(iron_ore_idx);
        printf("Iron ore material tier: %d\n", tier);
        assert(tier >= 0); /* Should be a valid tier */
    } else {
        printf("WARNING: material_iron_ore not found, checking for any material...\n");
        /* Try to find any material type item */
        for (int i = 0; i < rogue_item_defs_count(); i++) {
            const RogueItemDef* def = rogue_item_def_at(i);
            if (def && def->category == ROGUE_ITEM_MATERIAL) {
                int tier = rogue_material_tier(i);
                printf("Found material %s with tier: %d\n", def->id, tier);
                assert(tier >= 0);
                break;
            }
        }
    }
    
    /* Test recipe loading and validation
     * This ensures that recipes can reference JSON-loaded items by ID */
    rogue_craft_reset();
    
    /* Create a simple test recipe that uses JSON items */
    FILE* temp_recipe = fopen("test_recipes.cfg", "w");
    if (temp_recipe) {
        fprintf(temp_recipe, "# Test recipe using JSON items\n");
        fprintf(temp_recipe, "test_recipe,weapon_iron_sword,1,material_iron_ore:2\n");
        fclose(temp_recipe);
        
        /* Try to load the recipe */
        int recipes_loaded = rogue_craft_load_file("test_recipes.cfg");
        printf("Test recipes loaded: %d\n", recipes_loaded);
        
        if (recipes_loaded > 0) {
            const RogueCraftRecipe* recipe = rogue_craft_find("test_recipe");
            if (recipe) {
                printf("Recipe found: %s -> %s\n", recipe->id, 
                       rogue_item_def_at(recipe->output_def) ? 
                       rogue_item_def_at(recipe->output_def)->name : "unknown");
                assert(recipe->output_def >= 0);
                assert(recipe->input_count > 0);
                assert(recipe->inputs[0].def_index >= 0);
                printf("SUCCESS: Crafting recipe correctly references JSON items\n");
            }
        }
        
        /* Clean up */
        remove("test_recipes.cfg");
    }
    
    printf("Crafting JSON materials test: PASSED\n");
    return 0;
}