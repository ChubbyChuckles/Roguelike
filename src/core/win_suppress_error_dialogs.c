#if defined(_WIN32) && defined(ROGUE_SUPPRESS_WINDOWS_ERROR_DIALOGS)
#define WIN32_LEAN_AND_MEAN
#include <crtdbg.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

/* Ensure this runs before main() in MSVC by placing a function pointer in the C runtime initializer
   segment. Note: when built into a static library, the object may be discarded if no symbol is
   referenced by the EXE. To guarantee inclusion from tests, we also expose a callable installer and
   an anchor symbol referenced by core code. */
#ifdef _MSC_VER
#pragma section(".CRT$XCU", read)
static void __cdecl rogue_win_disable_error_dialogs(void);
__declspec(allocate(".CRT$XCU")) void(__cdecl* rogue_win_disable_error_dialogs_init)(void) =
    rogue_win_disable_error_dialogs;
#endif

static void configure_crt_reports(void)
{
#ifdef _MSC_VER
    /* Route CRT reports to stderr, not popups. */
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
    /* Prevent abort() from showing the debug dialog. */
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _set_error_mode(_OUT_TO_STDERR);
#endif
}

static void configure_windows_error_mode(void)
{
    /* Disable Windows Error Reporting UI and critical-error boxes for this process */
    UINT mode = SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX;
    SetErrorMode(SetErrorMode(0) | mode);
}

static void __cdecl rogue_win_disable_error_dialogs(void)
{
    configure_windows_error_mode();
    configure_crt_reports();
}

/* Public no-op idempotent installer to force-link this TU and allow late installation as soon as
 * any core API is used. */
void rogue_win_disable_error_dialogs_install_if_needed(void)
{
    static int installed = 0;
    if (installed)
        return;
    installed = 1;
    configure_windows_error_mode();
    configure_crt_reports();
}

/* Anchor symbol referenced from core to pull this object file from the static library when linking
 * tests. */
int rogue_win_disable_error_dialogs_anchor = 0;

#endif /* _WIN32 */
