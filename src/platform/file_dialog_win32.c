#ifdef _WIN32
#include "file_dialog.h"
#define WIN32_LEAN_AND_MEAN
#include <commdlg.h>
#include <string.h>
#include <windows.h>

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
#endif /* _WIN32 */
