#include <math.h>
#include <string.h>
#include <stdio.h>
#include "start_screen.h"
#include "graphics/font.h"
#include "input/input.h"
#include "core/game_loop.h"

int rogue_start_screen_active(void){ return g_app.show_start_screen; }

void rogue_start_screen_update_and_render(void){
    if(!g_app.show_start_screen) return;
    g_app.title_time += g_app.dt;
    RogueColor white = {255,255,255,255};
    int pulse = (int)((sin(g_app.title_time*2.0)*0.5 + 0.5) * 255.0);
    RogueColor title_col = { (unsigned char)pulse, (unsigned char)pulse, 255, 255};
    rogue_font_draw_text(40, 60, "ROGUELIKE", 6, title_col);
    const char* menu_items[] = { "New Game", "Quit", "Seed:" };
    int base_y = 140;
    for(int i=0;i<3;i++){
        RogueColor c = (i==g_app.menu_index)?(RogueColor){255,255,0,255}:white;
        rogue_font_draw_text(50, base_y + i*20, menu_items[i], 2, c);
    }
    char seed_line[64]; snprintf(seed_line, sizeof seed_line, "%u", g_app.pending_seed);
    rogue_font_draw_text(140, base_y + 2*20, seed_line, 2, white);
    if (g_app.entering_seed)
        rogue_font_draw_text(140 + (int)strlen(seed_line)*12, base_y + 2*20, "_", 2, white);
    if (rogue_input_was_pressed(&g_app.input, ROGUE_KEY_DOWN)) g_app.menu_index = (g_app.menu_index+1)%3;
    if (rogue_input_was_pressed(&g_app.input, ROGUE_KEY_UP)) g_app.menu_index = (g_app.menu_index+2)%3;
    if (rogue_input_was_pressed(&g_app.input, ROGUE_KEY_ACTION)){
        if (g_app.menu_index == 0) g_app.show_start_screen = 0; 
        else if (g_app.menu_index == 1) rogue_game_loop_request_exit();
        else if (g_app.menu_index == 2) g_app.entering_seed = 1;
    }
    if (g_app.entering_seed){
        for(int i=0;i<g_app.input.text_len;i++){
            char ch = g_app.input.text_buffer[i];
            if(ch >= '0' && ch <= '9') g_app.pending_seed = g_app.pending_seed*10 + (unsigned)(ch - '0');
            else if (ch == 'b' || ch == 'B') g_app.pending_seed /= 10;
        }
    }
}
