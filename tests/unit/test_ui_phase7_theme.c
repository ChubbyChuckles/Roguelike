#define SDL_MAIN_HANDLED 1
#include <stdio.h>
#include <string.h>
#include "ui/core/ui_theme.h"

int main(){
    RogueUIThemePack a,b; if(!rogue_ui_theme_load("assets/ui_theme_default.cfg", &a)){
        if(!rogue_ui_theme_load("../assets/ui_theme_default.cfg", &a)){
            if(!rogue_ui_theme_load("../../assets/ui_theme_default.cfg", &a)){
                printf("FAIL load default all paths\n"); return 1; }
        }
    }
    b=a; b.button_bg = 0x111111FF; b.padding_small=6; unsigned diff = rogue_ui_theme_diff(&a,&b); if(diff==0){ printf("FAIL diff none\n"); return 1; }
    rogue_ui_theme_apply(&a);
    rogue_ui_colorblind_set_mode(ROGUE_COLOR_PROTANOPIA); uint32_t t = rogue_ui_colorblind_transform(0xFF0000FF); if(t==0xFF0000FF){ printf("FAIL protanopia unchanged\n"); return 1; }
    rogue_ui_colorblind_set_mode(ROGUE_COLOR_DEUTERANOPIA); uint32_t td = rogue_ui_colorblind_transform(0x00FF00FF); if(td==0x00FF00FF){ printf("FAIL deuteranopia unchanged\n"); return 1; }
    rogue_ui_colorblind_set_mode(ROGUE_COLOR_TRITANOPIA); uint32_t tt = rogue_ui_colorblind_transform(0x0000FFFF); if(tt==0x0000FFFF){ printf("FAIL tritanopia unchanged\n"); return 1; }
    rogue_ui_colorblind_set_mode(ROGUE_COLOR_NORMAL); uint32_t t2 = rogue_ui_colorblind_transform(0xFF0000FF); if(t2!=0xFF0000FF){ printf("FAIL normal mode altered color\n"); return 1; }
    if(rogue_ui_scale_px(10) <= 0){ printf("FAIL dpi scale helper\n"); return 1; }
    printf("test_ui_phase7_theme: OK\n"); return 0; }
