/*
    Windows native (blocking) file dialog implementation (legacy).
    We now prefer the portable async overlay picker (file_dialog_portable.c).
    To re-enable the native dialog for comparison or performance tests,
    build with -DROGUE_USE_WIN32_NATIVE_DIALOG=1.
    (Kept minimal and isolated to avoid Windows header pollution.)
*/
#ifndef ROGUE_USE_WIN32_NATIVE_DIALOG
#define ROGUE_USE_WIN32_NATIVE_DIALOG 0
#endif

#if defined(_WIN32) && ROGUE_USE_WIN32_NATIVE_DIALOG
/* Intentionally do NOT define WIN32_LEAN_AND_MEAN here; <commdlg.h> may rely on
   broader Windows SDK declarations (e.g., via <prsht.h>). */
#include "file_dialog.h"
#include <commdlg.h>
#include <string.h>
#include <windows.h> /* MUST come before <commdlg.h> */

/* Compile-time guard: if someone reorders includes in build chain so commdlg.h
    appears before windows.h, detect it early. _WINDOWS_ is defined by windows.h,
    _INC_COMMDLG by commdlg.h. */
#if defined(_INC_COMMDLG) && !defined(_WINDOWS_)
#error "<windows.h> must be included before <commdlg.h> (see file_dialog_win32.c)"
#endif

int rogue_platform_open_file_dialog(const char* filter, char* out_path, size_t out_cap)
{
    if (!out_path || out_cap == 0)
        return 0;
    out_path[0] = '\0';
    char buf[512] = {0};
    OPENFILENAMEA ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = buf;
    ofn.nMaxFile = (DWORD) sizeof(buf);
    /* Caller supplies filter like "Images\0*.png;*.bmp;*.tga\0All Files\0*.*\0\0" or NULL */
    ofn.lpstrFilter = filter;
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn))
    {
        /* Safe copy without using MSVC's deprecated/secure variants to keep C11 portability */
        size_t len = strlen(buf);
        if (len >= out_cap)
        {
            len = out_cap - 1; /* truncate */
        }
        memcpy(out_path, buf, len);
        out_path[len] = '\0';
        return 1;
    }
    return 0;
}
#endif /* defined(_WIN32) && ROGUE_USE_WIN32_NATIVE_DIALOG */
