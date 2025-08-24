#include "../../src/core/app.h"
#include "../../src/core/app_state.h"
#include "../../src/core/localization.h"
#include <assert.h>

static void set_fake_locale(void)
{
    static RogueLocalePair fake[] = {
        {"menu_continue", "Weiter"},
        {"menu_new_game", "Neues Spiel"},
        {"menu_load", "Laden"},
        {"menu_settings", "Einstellungen"},
        {"menu_credits", "Mitwirkende"},
        {"menu_quit", "Beenden"},
        {"menu_seed", "Saat:"},
        {"tip_settings", "Einstellungen demnt"},
        {"tip_credits", "Mitwirkende demnt"},
        {"hint_accept_cancel", "Enter: auswhlen, Esc: zurcck"},
    };
    rogue_locale_set_table(fake, (int) (sizeof(fake) / sizeof(fake[0])));
}

int main(void)
{
    RogueAppConfig cfg = {
        "StartScreenLocale",       320, 180, 320, 180, 0, 0, 0, 1, ROGUE_WINDOW_WINDOWED,
        (RogueColor){0, 0, 0, 255}};
    assert(rogue_app_init(&cfg));

    /* Frame 1 default locale */
    rogue_app_step();
    extern const char* rogue_start_menu_label(int);
    const char* label_en = rogue_start_menu_label(1);
    assert(label_en && label_en[0]);

    /* Swap to fake locale and rebuild frame */
    set_fake_locale();
    rogue_app_step();
    const char* label_de = rogue_start_menu_label(1);
    assert(label_de && label_de[0]);

    /* Should differ between locales for New Game */
    int same = 0 == strcmp(label_en, label_de);
    if (same)
        return 1;

    rogue_app_shutdown();
    return 0;
}
