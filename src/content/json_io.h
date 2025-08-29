#ifndef ROGUE_CONTENT_JSON_IO_H
#define ROGUE_CONTENT_JSON_IO_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Read entire file into a newly allocated buffer (null-terminated for convenience).
     * Returns 0 on success, non-zero on error. On success, *out_data is owned by caller (free()).
     */
    int json_io_read_file(const char* path, char** out_data, size_t* out_len, char* err,
                          int err_cap);

    /* Atomic write: writes to a temporary file next to `path` then renames over `path`.
     * Returns 0 on success, non-zero on error. */
    int json_io_write_atomic(const char* path, const char* data, size_t len, char* err,
                             int err_cap);

    /* Get last modification time in milliseconds since epoch. Returns 0 on success. */
    int json_io_get_mtime_ms(const char* path, uint64_t* out_ms, char* err, int err_cap);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_CONTENT_JSON_IO_H */
