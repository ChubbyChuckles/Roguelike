/* test_dialogue_phase2_tokens.c - Phase 2 token expansion test */
#include <stdio.h>
#include <string.h>
#include "core/dialogue.h"

static const char* sample =
    "narrator|Hello ${player_name}! Seed=${run_seed}.\n"
    "narrator|Unknown token ${does_not_exist} stays literal.\n";

int main(void){
    rogue_dialogue_reset();
    rogue_dialogue_set_player_name("Aria");
    rogue_dialogue_set_run_seed(4242u);
    if(rogue_dialogue_register_from_buffer(55, sample, (int)strlen(sample))!=0){ printf("FAIL register\n"); return 1; }
    if(rogue_dialogue_start(55)!=0){ printf("FAIL start\n"); return 1; }
    char buf[256];
    if(rogue_dialogue_current_text(buf,sizeof buf)<0){ printf("FAIL curr0\n"); return 1; }
    if(strstr(buf,"Aria")==NULL){ printf("FAIL player_name replace got: %s\n", buf); return 1; }
    if(strstr(buf,"4242")==NULL){ printf("FAIL run_seed replace got: %s\n", buf); return 1; }
    if(strstr(buf,"${player_name}")!=NULL){ printf("FAIL token not replaced\n"); return 1; }
    if(rogue_dialogue_advance()!=1){ printf("FAIL advance to line2\n"); return 1; }
    if(rogue_dialogue_current_text(buf,sizeof buf)<0){ printf("FAIL curr1\n"); return 1; }
    if(strstr(buf,"${does_not_exist}")==NULL){ printf("FAIL unknown token should remain literal\n"); return 1; }
    if(rogue_dialogue_advance()!=0){ printf("FAIL final close\n"); return 1; }
    if(rogue_dialogue_playback()!=NULL){ printf("FAIL playback not closed\n"); return 1; }
    printf("OK test_dialogue_phase2_tokens\n");
    return 0;
}
