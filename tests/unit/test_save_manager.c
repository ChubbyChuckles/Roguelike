#include "core/save_manager.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int dummy_write(FILE* f){ const char data[] = "ABC"; fwrite(data,1,sizeof data,f); return 0; }
static int dummy_read(FILE* f, size_t size){ char buf[8]={0}; fread(buf,1,size<sizeof buf?size:sizeof buf,f); return (buf[0]=='A')?0:-1; }
static RogueSaveComponent DUMMY={ 10, dummy_write, dummy_read, "dummy" };

/* Provide minimal buff symbols required by save_manager.c */
typedef struct RogueBuff { int type; int active; int magnitude; double end_ms; } RogueBuff;
RogueBuff g_buffs_internal[4];
int g_buff_count_internal = 4;
int rogue_buffs_apply(int t,int m,double end,double now){ (void)t;(void)m;(void)end;(void)now; return 0; }
int rogue_buffs_get_total(int type){ (void)type; return 0; }
void rogue_buffs_init(void){}
void rogue_buffs_update(double now){ (void)now; }

int main(){
    rogue_save_manager_init();
    rogue_save_manager_register(&DUMMY);
    int rc = rogue_save_manager_save_slot(0);
    assert(rc==0);
    rc = rogue_save_manager_load_slot(0);
    assert(rc==0);
    printf("save_manager basic test passed\n");
    return 0;
}
