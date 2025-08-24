#include "../../src/game/dialogue.h"
#include <assert.h>
#include <stdio.h>

/* Minimal test: ensure style JSON loads and script JSON registers lines */
static int test_dialogue_json_run(void)
{
    int r = rogue_dialogue_style_load_from_json("assets/dialogue/style_default.json");
    assert(r == 0);
    r = rogue_dialogue_load_script_from_json_file("assets/dialogue/script_intro.json");
    assert(r == 0);
    const RogueDialogueScript* sc = rogue_dialogue_get(100);
    assert(sc && sc->line_count == 3);
    /* start and advance through lines */
    assert(rogue_dialogue_start(100) == 0);
    const RogueDialoguePlayback* pb = rogue_dialogue_playback();
    assert(pb && pb->line_index == 0);
    assert(rogue_dialogue_advance() == 1);
    assert(rogue_dialogue_advance() == 1);
    int adv = rogue_dialogue_advance();
    assert(adv == 0); /* closed */
    return 0;
}

int main(void) { return test_dialogue_json_run(); }
