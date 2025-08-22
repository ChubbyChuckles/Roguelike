/*
   NOTE: This test-side bootstrap for suppressing Windows error dialogs is disabled.
   The core module src/core/win_suppress_error_dialogs.c already provides a safe
   installer and an anchor symbol; linking rogue_core is sufficient to enable it.

   Historically, compiling this TU into many tests on MSVC correlated with
   widespread "unresolved external symbol main" link errors due to entry/CRT
   initialization interactions in certain configurations. To eliminate any
   side-effects, the code below is now gated behind an opt-in macro that is not
   defined by default. If you truly need to force early-install in a single test,
   define ROGUE_TEST_ENABLE_WIN_SUPPRESS_BOOTSTRAP for that one target/file.
*/

#if defined(ROGUE_TEST_ENABLE_WIN_SUPPRESS_BOOTSTRAP)
#if defined(_WIN32) && defined(ROGUE_SUPPRESS_WINDOWS_ERROR_DIALOGS)
#ifdef _MSC_VER
#pragma section(".CRT$XCU", read)
static void __cdecl rogue_test_bootstrap_install(void);
__declspec(allocate(".CRT$XCU")) void(__cdecl* rogue_test_bootstrap_ctor)(void) =
    rogue_test_bootstrap_install;
#endif

/* Forward declarations from core suppression TU (no public header to avoid coupling). */
extern void rogue_win_disable_error_dialogs_install_if_needed(void);
extern int rogue_win_disable_error_dialogs_anchor;

/* Ensure suppression is installed as early as possible for every test process,
   and force-link the core suppression TU via the anchor reference. */
static void __cdecl rogue_test_bootstrap_install(void)
{
    (void) rogue_win_disable_error_dialogs_anchor; /* force object inclusion */
    rogue_win_disable_error_dialogs_install_if_needed();
}
#endif /* _WIN32 && ROGUE_SUPPRESS_WINDOWS_ERROR_DIALOGS */
#endif /* ROGUE_TEST_ENABLE_WIN_SUPPRESS_BOOTSTRAP */
