/* Phase 4 dialogue persistence test: save active mid-script, reset, restore */
#include <stdio.h>
#include <string.h>
#include "core/dialogue.h"

static const char* script =
    "npc|Line A\n"
    "npc|Line B\n"
    "npc|Line C\n";

int main(void){
    rogue_dialogue_reset();
    if(rogue_dialogue_register_from_buffer(101, script, (int)strlen(script))!=0){ printf("FAIL register\n"); return 1; }
    if(rogue_dialogue_start(101)!=0){ printf("FAIL start\n"); return 1; }
    if(rogue_dialogue_advance()!=1){ printf("FAIL advance to line1\n"); return 1; }
    if(rogue_dialogue_advance()!=1){ printf("FAIL advance to line2\n"); return 1; }
    /* Now at line index 2 (last) */
    RogueDialoguePersistState st; int r = rogue_dialogue_capture(&st); if(r!=1){ printf("FAIL capture r=%d\n", r); return 1; }
    if(st.line_index!=2 || st.script_id!=101){ printf("FAIL state mismatch idx=%d script=%d\n", st.line_index, st.script_id); return 1; }
    rogue_dialogue_reset();
    /* Re-register script to simulate loading assets before restore */
    if(rogue_dialogue_register_from_buffer(101, script, (int)strlen(script))!=0){ printf("FAIL re-register\n"); return 1; }
    if(rogue_dialogue_restore(&st)!=0){ printf("FAIL restore\n"); return 1; }
    const RogueDialoguePlayback* pb = rogue_dialogue_playback(); if(!pb){ printf("FAIL playback null after restore\n"); return 1; }
    if(pb->line_index!=2 || pb->script_id!=101){ printf("FAIL restored indices mismatch\n"); return 1; }
    /* Advance should close */
    if(rogue_dialogue_advance()!=0){ printf("FAIL final close after restore\n"); return 1; }
    printf("OK test_dialogue_phase4_persistence\n");
    return 0;
}
