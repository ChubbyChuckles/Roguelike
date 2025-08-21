#ifndef ROGUE_INVENTORY_TAGS_H
#define ROGUE_INVENTORY_TAGS_H
#include <stdint.h>
#include <stdio.h> /* FILE */
#ifdef __cplusplus
extern "C"
{
#endif
    /* Phase 3.1/3.2 Inventory metadata: favorite/lock flags + string tags.
       Metadata stored per item definition id (def_index). Applies to whole stack quantity. */

#define ROGUE_INV_TAG_MAX_DEFS 4096
#define ROGUE_INV_TAG_MAX_TAGS_PER_DEF 4
#define ROGUE_INV_TAG_SHORT_LEN 24

/* Bit flags */
#define ROGUE_INV_FLAG_FAVORITE 0x1u
#define ROGUE_INV_FLAG_LOCKED 0x2u

    int rogue_inv_tags_init(void);
    int rogue_inv_tags_set_flags(int def_index, unsigned flags); /* replaces flags */
    unsigned rogue_inv_tags_get_flags(int def_index);
    int rogue_inv_tags_add_tag(int def_index, const char* tag); /* returns 0 or -1 full */
    int rogue_inv_tags_remove_tag(int def_index, const char* tag);
    int rogue_inv_tags_list(int def_index, const char** out_tags,
                            int cap); /* returns total stored (can exceed cap provided) */
    int rogue_inv_tags_has(int def_index, const char* tag);
    int rogue_inv_tags_can_salvage(int def_index); /* false if locked or favorite (policy) */

    /* Persistence helpers (Phase 3.2) */
    int rogue_inv_tags_serialize(FILE* f); /* text debug format (optional) */

#ifdef __cplusplus
}
#endif
#endif
