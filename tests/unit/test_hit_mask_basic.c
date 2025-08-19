#include "game/hit_pixel_mask.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifdef main
#undef main
#endif

int main(void){
    RogueHitPixelMaskSet* set = rogue_hit_pixel_masks_ensure(0);
    assert(set && set->ready && set->frame_count==8);
    int populated=0; for(int i=0;i<8;i++){ const RogueHitPixelMaskFrame* f=&set->frames[i]; assert(f->bits); int any=0; for(int y=0;y<f->height && !any;y++){ for(int x=0;x<f->width;x++){ if(rogue_hit_mask_test(f,x,y)){ any=1; break; } } } if(any) populated++; }
    assert(populated==8);
    const RogueHitPixelMaskFrame* f7=&set->frames[7]; int found_far=0; for(int x=f7->width-1; x>=0 && !found_far; --x){ for(int y=0;y<f7->height;y++){ if(rogue_hit_mask_test(f7,x,y)){ if(x>=28) found_far=1; break; } } }
    assert(found_far);
    printf("hit_mask_basic: PASS\n");
    rogue_hit_pixel_masks_reset_all();
    return 0;
}
