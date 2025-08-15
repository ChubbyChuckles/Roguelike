#include "util/hot_reload.h"
#include <stdio.h>
#include <string.h>

static int g_called = 0; static char g_last_path[256];
static void test_cb(const char* path, void* user){ (void)user; g_called++; strncpy(g_last_path,path,sizeof g_last_path -1); g_last_path[sizeof g_last_path -1]='\0'; }

int main(void){
    rogue_hot_reload_reset();
    int r = rogue_hot_reload_register("affixes","assets/affixes.cfg", test_cb, NULL);
    if(r!=0){ fprintf(stderr,"register fail\n"); return 1; }
    if(rogue_hot_reload_force("missing")==0){ fprintf(stderr,"unexpected success missing id\n"); return 1; }
    if(rogue_hot_reload_force("affixes")!=0){ fprintf(stderr,"force fail\n"); return 1; }
    if(g_called!=1){ fprintf(stderr,"callback count mismatch %d\n", g_called); return 1; }
    if(strcmp(g_last_path,"assets/affixes.cfg")!=0){ fprintf(stderr,"path mismatch %s\n", g_last_path); return 1; }
    /* duplicate id should fail */
    if(rogue_hot_reload_register("affixes","assets/affixes.cfg", test_cb, NULL)==0){ fprintf(stderr,"duplicate allowed\n"); return 1; }
    return 0;
}
