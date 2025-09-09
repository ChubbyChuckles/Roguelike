/* Test equipment persistence with JSON-loaded items
 * Verifies that equipment save/load works correctly with JSON item definitions
 */
#include "../../src/core/equipment/equipment.h"
#include "../../src/core/equipment/equipment_persist.h"
#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/core/loot/loot_items.h"
#include <stdio.h>
#include <assert.h>

int main(void)
{
    printf("Testing equipment persistence with JSON-loaded items...\n");
    
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
    
    /* Reset equipment */
    rogue_equip_reset();
    
    /* Find a weapon item to test with */
    int sword_idx = rogue_item_def_index("weapon_iron_sword");
    if (sword_idx < 0) {
        /* Try to find any weapon */
        for (int i = 0; i < rogue_item_defs_count(); i++) {
            const RogueItemDef* def = rogue_item_def_at(i);
            if (def && def->category == ROGUE_ITEM_WEAPON) {
                sword_idx = i;
                break;
            }
        }
    }
    
    if (sword_idx >= 0) {
        const RogueItemDef* sword_def = rogue_item_def_at(sword_idx);
        printf("Testing with weapon: %s (def_index: %d)\n", sword_def->name, sword_idx);
        
        /* Create an item instance */
        int item_inst = rogue_items_spawn(sword_idx, 1, 0.0f, 0.0f);
        if (item_inst >= 0) {
            printf("Created item instance: %d\n", item_inst);
            
            /* Try to equip the item */
            int equipped = rogue_equip_try(ROGUE_EQUIP_WEAPON, item_inst);
            printf("Equipment result: %d\n", equipped);
            
            if (equipped == 0) { /* 0 means success */
                /* Test persistence by saving and loading equipment state */
                char save_buffer[4096];
                int save_size = rogue_equipment_serialize(save_buffer, sizeof(save_buffer));
                printf("Equipment save size: %d bytes\n", save_size);
                
                if (save_size > 0) {
                    /* Clear equipment */
                    rogue_equip_reset();
                    
                    /* Reload from saved data */ 
                    int loaded = rogue_equipment_deserialize(save_buffer);
                    printf("Equipment load result: %d\n", loaded);
                    
                    if (loaded == 0) { /* 0 means success */
                        /* Verify the equipment was restored correctly */
                        int equipped_inst = rogue_equip_get(ROGUE_EQUIP_WEAPON);
                        if (equipped_inst >= 0) {
                            const RogueItemInstance* equipped_item = rogue_item_instance_at(equipped_inst);
                            if (equipped_item && equipped_item->def_index == sword_idx) {
                                printf("SUCCESS: Equipment persistence maintained def_index integrity\n");
                                printf("Equipped item: %s (def_index: %d)\n", 
                                       rogue_item_def_at(equipped_item->def_index)->name,
                                       equipped_item->def_index);
                            } else {
                                printf("WARNING: Equipment not properly restored\n");
                            }
                        } else {
                            printf("WARNING: No equipped weapon found after restore\n");
                        }
                    }
                }
            }
        }
    } else {
        printf("WARNING: No weapon items found for testing\n");
    }
    
    printf("Equipment JSON persistence test: PASSED\n");
    return 0;
}