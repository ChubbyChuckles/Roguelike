#include "core/save_manager.h"
#include "core/app_state.h"
#include "core/buffs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

RogueAppState g_app; RoguePlayer g_exposed_player_for_stats; void rogue_player_recalc_derived(RoguePlayer* p){ (void)p; }

/* Buff stubs (match symbols required by save_manager) */
RogueBuff g_buffs_internal[4]; int g_buff_count_internal=4; int rogue_buffs_apply(RogueBuffType t,int mag,double dur,double now){ (void)t;(void)mag;(void)dur;(void)now; return 0; }

int main(void){
    memset(&g_app,0,sizeof g_app);
    rogue_save_manager_init();
    rogue_register_core_save_components();
    g_app.player.level = 1;
    for(int i=0;i<10;i++){
        g_app.player.level = i+1;
        int rc = rogue_save_manager_autosave(i);
        assert(rc==0);
    }
    /* Only ring size files expected */
    int present=0;
    for(int r=0;r<ROGUE_AUTOSAVE_RING;r++){
        char path[64]; snprintf(path,sizeof path,"autosave_%d.sav", r);
        FILE* f=fopen(path,"rb"); if(f){ present++; fclose(f);} }
    assert(present==ROGUE_AUTOSAVE_RING);
    printf("AUTOSAVE_RING_OK count=%d\n", present);
    return 0;
}
