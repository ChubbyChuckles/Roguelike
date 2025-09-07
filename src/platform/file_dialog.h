/* file_dialog.h - Minimal platform file open helpers (Phase 2 Asset Overlay)
   Currently only implements Windows open file dialog (single selection). Other
   platforms return 0 (not implemented yet). Designed for debug overlay usage
   so we keep it lightweight and optional. */
#ifndef ROGUE_PLATFORM_FILE_DIALOG_H
#define ROGUE_PLATFORM_FILE_DIALOG_H

#include <stddef.h>

#ifdef _WIN32
int rogue_platform_open_file_dialog(const char* filter, char* out_path, size_t out_cap);
#else
static inline int rogue_platform_open_file_dialog(const char* filter, char* out_path,
                                                  size_t out_cap)
{
    (void) filter;
    (void) out_path;
    (void) out_cap;
    return 0; /* not implemented */
}
#endif

#endif /* ROGUE_PLATFORM_FILE_DIALOG_H */
