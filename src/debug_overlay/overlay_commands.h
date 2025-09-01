#ifndef ROGUE_DEBUG_OVERLAY_COMMANDS_H
#define ROGUE_DEBUG_OVERLAY_COMMANDS_H

#ifdef __cplusplus
extern "C"
{
#endif

    typedef void (*OverlayCommandFn)(void* user);

    int overlay_command_register(const char* name, OverlayCommandFn fn, void* user);
    /* Invoke by name (exact match). Returns 0 on success. Headless-safe. */
    int overlay_invoke_action(const char* name);
    /* Render command palette UI when toggled; non-modal. */
    void overlay_commands_render(void);
    /* Toggle palette open/close. */
    void overlay_commands_toggle(int open);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_DEBUG_OVERLAY_COMMANDS_H */
