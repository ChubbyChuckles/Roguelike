/* Internal utilities for dialogue system: lightweight JSON extractors and file IO */
#ifndef ROGUE_DIALOGUE_UTIL_H
#define ROGUE_DIALOGUE_UTIL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Read entire file into a newly malloc'd buffer (binary). Returns 0 on success. */
    int rogue_dialogue__read_all(const char* path, char** out_buf, int* out_len);

    /* Extract a JSON string value by key from a flat object buffer. Returns 0 on success. */
    int rogue_dialogue__json_extract_string(const char* json, const char* key, char* out,
                                            size_t cap);

    /* Extract a JSON int value by key from a flat object buffer. Returns 0 on success. */
    int rogue_dialogue__json_extract_int(const char* json, const char* key, int* out);

    /* Parse color from "#RRGGBB", 0xAARRGGBB, or decimal integer. Returns 0 on success. */
    int rogue_dialogue__parse_color(const char* s, unsigned int* out);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_DIALOGUE_UTIL_H */
