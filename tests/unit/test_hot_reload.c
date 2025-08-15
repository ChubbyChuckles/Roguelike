#include "util/hot_reload.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int g_called = 0; static char g_last_path[256];
static void test_cb(const char* path, void* user){ (void)user; g_called++; strncpy(g_last_path,path,sizeof g_last_path -1); g_last_path[sizeof g_last_path -1]='\0'; }

static int write_tmp(const char* path, const char* contents){ FILE* f=NULL; 
#if defined(_MSC_VER)
    fopen_s(&f,path,"wb");
#else
    f=fopen(path,"wb");
#endif
    if(!f) return -1; fputs(contents,f); fclose(f); return 0; }

int main(void){
    rogue_hot_reload_reset();
    const char* tmp = "tmp_hot_reload_test.cfg";
    if(write_tmp(tmp,"alpha=1\n")!=0){ fprintf(stderr,"tmp write fail\n"); return 1; }
    if(rogue_hot_reload_register("tmp", tmp, test_cb, NULL)!=0){ fprintf(stderr,"register fail\n"); return 1; }
    if(rogue_hot_reload_tick()!=0){ fprintf(stderr,"unexpected initial fire\n"); return 1; }
    if(write_tmp(tmp,"alpha=2\n")!=0){ fprintf(stderr,"modify fail\n"); return 1; }
    int fired = rogue_hot_reload_tick();
    if(fired!=1){ fprintf(stderr,"tick fired %d (expected 1)\n", fired); return 1; }
    if(g_called!=1){ fprintf(stderr,"callback count mismatch %d\n", g_called); return 1; }
    if(rogue_hot_reload_force("tmp")!=0){ fprintf(stderr,"force fail\n"); return 1; }
    if(g_called!=2){ fprintf(stderr,"force did not increment callback (%d)\n", g_called); return 1; }
    /* duplicate id should fail */
    if(rogue_hot_reload_register("tmp", tmp, test_cb, NULL)==0){ fprintf(stderr,"duplicate allowed\n"); return 1; }
    remove(tmp);
    return 0;
}
