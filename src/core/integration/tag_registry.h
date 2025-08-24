#ifndef ROGUE_TAG_REGISTRY_H
#define ROGUE_TAG_REGISTRY_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Validate tag_registry JSON text; returns 0 on success, <0 on error; err filled on failure. */
    int rogue_tag_registry_validate_text(const char* json_text, char* err, size_t err_cap);

    /* Convenience: load from file path and validate. Returns 0 on success, <0 on error. */
    int rogue_tag_registry_validate_file(const char* path, char* err, size_t err_cap);

#ifdef __cplusplus
}
#endif

#endif
