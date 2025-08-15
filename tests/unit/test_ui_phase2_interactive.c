/* UI Phase 2.2 Interactive widgets: Button, Toggle, Slider, TextInput */
#include "ui/core/ui_context.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

static void inject(RogueUIContext* ctx, float mx, float my, int down, int pressed, int released, char ch, int back){
    RogueUIInputState in={0};
    in.mouse_x=mx; in.mouse_y=my; in.mouse_down=down; in.mouse_pressed=pressed; in.mouse_released=released; in.text_char=ch; in.backspace=back;
    rogue_ui_set_input(ctx,&in);
}

static void test_interactive_widgets(){
    RogueUIContext ctx; RogueUIContextConfig cfg={.max_nodes=64,.seed=1u,.arena_size=8192};
    assert(rogue_ui_init(&ctx,&cfg));
    int toggle_state=0; float slider_val=5.0f; char textbuf[32]="";

    /* Frame 1: initial build, click button */
    rogue_ui_begin(&ctx,16.6);
    inject(&ctx,10,10,0,0,0,0,0);
    int btn = rogue_ui_button(&ctx,(RogueUIRect){0,0,80,20},"Click",0x303030FFu,0xFFFFFFFFu); assert(btn==0);
    int tog = rogue_ui_toggle(&ctx,(RogueUIRect){0,25,80,20},"Tog",&toggle_state,0x800000FFu,0x008000FFu,0xFFFFFFFFu); assert(tog==1);
    int sld = rogue_ui_slider(&ctx,(RogueUIRect){0,50,100,12},0.0f,10.0f,&slider_val,0x202020FFu,0x00FFFFFFu); assert(sld==2);
    int tin = rogue_ui_text_input(&ctx,(RogueUIRect){0,70,120,20},textbuf,32,0x101010FFu,0xFFFFFFFFu); assert(tin==3);
    rogue_ui_end(&ctx);

    /* Click toggle */
    rogue_ui_begin(&ctx,16.6);
    inject(&ctx,10,30,1,1,0,0,0); /* press over toggle */
    rogue_ui_button(&ctx,(RogueUIRect){0,0,80,20},"Click",0x303030FFu,0xFFFFFFFFu);
    rogue_ui_toggle(&ctx,(RogueUIRect){0,25,80,20},"Tog",&toggle_state,0x800000FFu,0x008000FFu,0xFFFFFFFFu);
    rogue_ui_slider(&ctx,(RogueUIRect){0,50,100,12},0.0f,10.0f,&slider_val,0x202020FFu,0x00FFFFFFu);
    rogue_ui_text_input(&ctx,(RogueUIRect){0,70,120,20},textbuf,32,0x101010FFu,0xFFFFFFFFu);
    rogue_ui_end(&ctx); assert(toggle_state==0); /* no release yet */

    /* Release click -> toggle flips */
    rogue_ui_begin(&ctx,16.6);
    inject(&ctx,10,30,0,0,1,0,0); /* release */
    rogue_ui_button(&ctx,(RogueUIRect){0,0,80,20},"Click",0x303030FFu,0xFFFFFFFFu);
    rogue_ui_toggle(&ctx,(RogueUIRect){0,25,80,20},"Tog",&toggle_state,0x800000FFu,0x008000FFu,0xFFFFFFFFu);
    rogue_ui_slider(&ctx,(RogueUIRect){0,50,100,12},0.0f,10.0f,&slider_val,0x202020FFu,0x00FFFFFFu);
    rogue_ui_text_input(&ctx,(RogueUIRect){0,70,120,20},textbuf,32,0x101010FFu,0xFFFFFFFFu);
    rogue_ui_end(&ctx); assert(toggle_state==1);

    /* Drag slider to ~75% (value near 7.5) */
    rogue_ui_begin(&ctx,16.6);
    inject(&ctx,10,55,1,1,0,0,0); /* press on slider start */
    rogue_ui_button(&ctx,(RogueUIRect){0,0,80,20},"Click",0x303030FFu,0xFFFFFFFFu);
    rogue_ui_toggle(&ctx,(RogueUIRect){0,25,80,20},"Tog",&toggle_state,0x800000FFu,0x008000FFu,0xFFFFFFFFu);
    rogue_ui_slider(&ctx,(RogueUIRect){0,50,100,12},0.0f,10.0f,&slider_val,0x202020FFu,0x00FFFFFFu);
    rogue_ui_text_input(&ctx,(RogueUIRect){0,70,120,20},textbuf,32,0x101010FFu,0xFFFFFFFFu);
    rogue_ui_end(&ctx);
    rogue_ui_begin(&ctx,16.6);
    inject(&ctx,75,55,1,0,0,0,0); /* hold and move */
    rogue_ui_button(&ctx,(RogueUIRect){0,0,80,20},"Click",0x303030FFu,0xFFFFFFFFu);
    rogue_ui_toggle(&ctx,(RogueUIRect){0,25,80,20},"Tog",&toggle_state,0x800000FFu,0x008000FFu,0xFFFFFFFFu);
    rogue_ui_slider(&ctx,(RogueUIRect){0,50,100,12},0.0f,10.0f,&slider_val,0x202020FFu,0x00FFFFFFu);
    rogue_ui_text_input(&ctx,(RogueUIRect){0,70,120,20},textbuf,32,0x101010FFu,0xFFFFFFFFu);
    rogue_ui_end(&ctx);
    assert(slider_val>7.0f && slider_val<8.0f);
    /* Release */
    rogue_ui_begin(&ctx,16.6);
    inject(&ctx,75,55,0,0,1,0,0);
    rogue_ui_slider(&ctx,(RogueUIRect){0,50,100,12},0.0f,10.0f,&slider_val,0x202020FFu,0x00FFFFFFu);
    rogue_ui_end(&ctx);

    /* Focus text input and type 'A', then backspace */
    rogue_ui_begin(&ctx,16.6);
    inject(&ctx,10,75,1,1,0,0,0); /* press inside text input */
    rogue_ui_text_input(&ctx,(RogueUIRect){0,70,120,20},textbuf,32,0x101010FFu,0xFFFFFFFFu);
    rogue_ui_end(&ctx);
    rogue_ui_begin(&ctx,16.6);
    inject(&ctx,10,75,0,0,1,'A',0); /* release + char A */
    rogue_ui_text_input(&ctx,(RogueUIRect){0,70,120,20},textbuf,32,0x101010FFu,0xFFFFFFFFu);
    rogue_ui_end(&ctx); assert(strcmp(textbuf,"A")==0);
    rogue_ui_begin(&ctx,16.6);
    inject(&ctx,10,75,0,0,0,0,1); /* backspace */
    rogue_ui_text_input(&ctx,(RogueUIRect){0,70,120,20},textbuf,32,0x101010FFu,0xFFFFFFFFu);
    rogue_ui_end(&ctx); assert(textbuf[0]=='\0');

    rogue_ui_shutdown(&ctx);
}

int main(){
    test_interactive_widgets();
    printf("UI Phase2 interactive tests passed\n");
    return 0;
}
