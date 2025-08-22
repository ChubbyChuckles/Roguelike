#ifndef ROGUE_VEGETATION_JSON_H
#define ROGUE_VEGETATION_JSON_H

#include "core/vegetation/vegetation.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Load vegetation defs from a JSON array (plants). Returns count added or -1 on error. */
    int rogue_vegetation_load_plants_json_text(const char* json_text, char* err, size_t err_cap);
    /* Load vegetation defs from a JSON array (trees). Returns count added or -1 on error. */
    int rogue_vegetation_load_trees_json_text(const char* json_text, char* err, size_t err_cap);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_VEGETATION_JSON_H */
