#define SDL_MAIN_HANDLED 1
#include <stdio.h>
#include <string.h>
#include "ui/core/ui_context.h"

int main(){
    RogueUIContext ctx; RogueUIContextConfig cfg={128,42, 16*1024};
    if(!rogue_ui_init(&ctx,&cfg)){ printf("FAIL init\n"); return 1; }
    rogue_ui_begin(&ctx,16.0);
    RogueUIRect r={10,10,80,24}; int b=rogue_ui_button(&ctx,r,"AnimBtn",0x202020FF,0xFFFFFFFF);
    if(b<0){ printf("FAIL button create\n"); return 1; }
    uint32_t id = ctx.nodes[b].id_hash;
    rogue_ui_entrance(&ctx,id,300.0f,ROGUE_EASE_CUBIC_OUT);
    rogue_ui_button_press_pulse(&ctx,id);
    float s0 = rogue_ui_anim_scale(&ctx,id);
    if(s0 < 1.0f){ printf("FAIL initial scale %f < 1\n",s0); return 1; }
    rogue_ui_end(&ctx);
    /* Advance frames */
    for(int i=0;i<25;i++){ rogue_ui_begin(&ctx,16.0); rogue_ui_end(&ctx); }
    float s_mid = rogue_ui_anim_scale(&ctx,id);
    float a_mid = rogue_ui_anim_alpha(&ctx,id);
    if(a_mid <= 0.1f){ printf("FAIL alpha mid %f\n",a_mid); return 1; }
    rogue_ui_set_time_scale(&ctx,0.1f);
    rogue_ui_begin(&ctx,16.0); rogue_ui_end(&ctx);
    float s_slow = rogue_ui_anim_scale(&ctx,id);
    if(s_slow < 0.7f){ printf("FAIL scale slow %f\n",s_slow); return 1; }
    /* restore normal time scale for exit animation */
    rogue_ui_set_time_scale(&ctx,1.0f);
    rogue_ui_exit(&ctx,id,300.0f,ROGUE_EASE_CUBIC_IN);
    /* step once so exit animation begins advancing */
    rogue_ui_begin(&ctx,16.0); rogue_ui_end(&ctx);
    for(int i=0;i<22;i++){ rogue_ui_begin(&ctx,16.0); rogue_ui_end(&ctx); }
    float a_exit = rogue_ui_anim_alpha(&ctx,id);
    if(a_exit >= 0.95f){ printf("FAIL exit alpha %f\n",a_exit); return 1; }
    rogue_ui_shutdown(&ctx);
    printf("test_ui_phase8_animation: OK\n"); return 0;
}
