#ifndef ROGUE_WORLD_GEN_RESOURCE_JSON_H
#define ROGUE_WORLD_GEN_RESOURCE_JSON_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Load resource node definitions from a JSON array of objects and register them
     * via rogue_resource_register. Returns number added or -1 on error. */
    int rogue_resource_defs_load_json_text(const char* json_text, char* err, size_t err_cap);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_WORLD_GEN_RESOURCE_JSON_H */
