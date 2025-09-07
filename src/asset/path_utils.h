/* path_utils.h - Phase 3 minimal cross-platform path helpers */
#ifndef ROGUE_PATH_UTILS_H
#define ROGUE_PATH_UTILS_H

#include <stdbool.h>

/* Normalize in place (converts backslashes to forward slashes) */
void rogue_path_normalize(char* path);
/* Returns true if path appears absolute for the current platform */
bool rogue_path_is_absolute(const char* path);
/* Join two segments into out (safe for out==a when b appended) */
bool rogue_path_join(const char* a, const char* b, char* out, int out_cap);

#endif /* ROGUE_PATH_UTILS_H */
