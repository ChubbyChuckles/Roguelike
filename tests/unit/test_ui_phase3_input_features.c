#include <assert.h>
#include <string.h>
#include "ui/core/ui_context.h"

static void frame(RogueUIContext* ui, RogueUIInputState in){
    rogue_ui_set_input(ui,&in);
    rogue_ui_begin(ui,16.0);
}

int main(){
    RogueUIContextConfig cfg={0}; cfg.max_nodes=128; cfg.arena_size=4096; cfg.seed=42;
    RogueUIContext ui; assert(rogue_ui_init(&ui,&cfg));

    /* Test clipboard paste into text input */
    char buf[32] = {0};
    rogue_ui_clipboard_set("Hello");
    frame(&ui,(RogueUIInputState){0});
    int ti = rogue_ui_text_input(&ui,(RogueUIRect){0,0,100,20},buf,(int)sizeof(buf),0,0);
    ui.hot_index=ti; ui.input.mouse_pressed=1; // focus it
    rogue_ui_text_input(&ui,(RogueUIRect){0,0,100,20},buf,(int)sizeof(buf),0,0);
    rogue_ui_end(&ui);

    frame(&ui,(RogueUIInputState){.key_paste=1});
    ui.focus_index=ti; /* ensure still focused for paste */
    rogue_ui_text_input(&ui,(RogueUIRect){0,0,100,20},buf,(int)sizeof(buf),0,0);
    assert(strcmp(buf,"Hello")==0);
    rogue_ui_end(&ui);

    /* Test chord registration and detection (Ctrl+K then Ctrl+X) */
    rogue_ui_register_chord(&ui,'k','x',99);
    ui.chord_timeout_ms = 500.0; /* ensure large window */

    frame(&ui,(RogueUIInputState){.key_ctrl=1,.key_char='k'});
    rogue_ui_navigation_update(&ui); // process chord start
    rogue_ui_end(&ui);
    assert(ui.pending_chord=='k');

    frame(&ui,(RogueUIInputState){.key_ctrl=1,.key_char='x'});
    rogue_ui_navigation_update(&ui); // process chord finish
    rogue_ui_end(&ui);
    assert(rogue_ui_last_command(&ui)==99);

    /* Test input replay recording & playback of a simple char entry sequence into text input */
    memset(buf,0,sizeof buf);
    rogue_ui_replay_start_record(&ui);
    /* Manually push a few input states to record buffer */
    for(int i=0;i<5;i++){
        RogueUIInputState s={0}; s.text_char = 'a'+i; ui.replay_buffer[ui.replay_count++]=s; }
    rogue_ui_replay_stop_record(&ui);
    rogue_ui_replay_start_playback(&ui);

    for(;;){
        frame(&ui,(RogueUIInputState){0}); // will be overwritten by replay step
        rogue_ui_navigation_update(&ui); // apply replay input to ctx->input
        int t2 = rogue_ui_text_input(&ui,(RogueUIRect){0,0,100,20},buf,(int)sizeof(buf),0,0);
        ui.focus_index = t2; // ensure focused
        rogue_ui_text_input(&ui,(RogueUIRect){0,0,100,20},buf,(int)sizeof(buf),0,0);
        rogue_ui_end(&ui);
        if(!ui.replay_playing) break;
    }
    assert(strcmp(buf,"abcde")==0);

    return 0;
}
