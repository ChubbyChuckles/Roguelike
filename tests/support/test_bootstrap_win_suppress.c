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
