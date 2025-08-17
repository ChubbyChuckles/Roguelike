/* Test v8 replay hash component */
#include "core/save_manager.h"
#include <stdio.h>
#include <string.h>

/* Buff stub (deduplicated) */
/* Use real buff system */

int main(void){
    if(ROGUE_SAVE_FORMAT_VERSION < 8u){ printf("REPLAY_SKIP v=%u\n", (unsigned)ROGUE_SAVE_FORMAT_VERSION); return 0; }
    rogue_save_manager_reset_for_tests();
    rogue_save_manager_init();
    rogue_register_core_save_components();
    /* Record deterministic fake input events */
    for(uint32_t i=0;i<10;i++){ if(rogue_save_replay_record_input(60*i, (i%3)+1, (int32_t)(i*7-3))!=0){ printf("REPLAY_FAIL record\n"); return 1; } }
    int save_rc = rogue_save_manager_save_slot(0);
    if(save_rc!=0){ printf("REPLAY_FAIL save rc=%d\n", save_rc); return 1; }
    unsigned char h1[32]; memcpy(h1, rogue_save_last_replay_hash(), 32);
    uint32_t cnt1 = rogue_save_last_replay_event_count();
    if(cnt1!=10){ printf("REPLAY_FAIL count pre=%u\n", cnt1); return 1; }
    /* load back */
    int lrc = rogue_save_manager_load_slot(0);
    if(lrc!=0){ printf("REPLAY_FAIL load rc=%d\n", lrc); return 1; }
    unsigned char h2[32]; memcpy(h2, rogue_save_last_replay_hash(), 32);
    if(memcmp(h1,h2,32)!=0){ printf("REPLAY_FAIL hash mismatch\n"); return 1; }
    if(rogue_save_last_replay_event_count()!=10){ printf("REPLAY_FAIL count post=%u\n", rogue_save_last_replay_event_count()); return 1; }
    char hex[65]; rogue_save_last_replay_hash_hex(hex); if(strlen(hex)!=64){ printf("REPLAY_FAIL hex len\n"); return 1; }
    printf("REPLAY_OK count=%u hash=%s\n", rogue_save_last_replay_event_count(), hex);
    return 0; }
