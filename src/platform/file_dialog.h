/* file_dialog.h - Minimal platform file open helpers (Phase 2 Asset Overlay)
   Currently only implements Windows open file dialog (single selection). Other
   platforms return 0 (not implemented yet). Designed for debug overlay usage
   so we keep it lightweight and optional. */
#ifndef ROGUE_PLATFORM_FILE_DIALOG_H
#define ROGUE_PLATFORM_FILE_DIALOG_H

#include <stddef.h>

/* Legacy synchronous helper (now implemented as no-op wrapper). */
int rogue_platform_open_file_dialog(const char* filter, char* out_path, size_t out_cap);

/* Asynchronous in-overlay file dialog API */
typedef enum RogueFileDialogMode
{
    ROGUE_FD_MODE_OPEN = 0,
    ROGUE_FD_MODE_SAVE = 1
} RogueFileDialogMode;

/* Show (or reopen) the modal picker inside the debug overlay. */
void rogue_file_dialog_show(RogueFileDialogMode mode, const char* filter_patterns,
                            const char* default_name);
/* Poll for result: returns 1 (path ready), 0 (pending), -1 (canceled). */
int rogue_file_dialog_poll_result(char* out_path, size_t out_cap);
/* Draw UI if active (call each frame from an overlay panel). */
void rogue_file_dialog_draw_overlay(void);

#endif /* ROGUE_PLATFORM_FILE_DIALOG_H */
