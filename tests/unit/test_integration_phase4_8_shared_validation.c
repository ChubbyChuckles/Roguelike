// Phase 4.8 Tests: Shared Data Structure Validation
// Covers roadmap items 4.8.1 - 4.8.7 (subset implemented where underlying modules exist)
// Implemented entirely in C (no C++) and deterministic.

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "core/integration/entity_id.h"
#include "core/integration/resource_lock.h"
#include "core/integration/cow.h"

#ifdef _WIN32
#include <windows.h>
static DWORD WINAPI stress_thread(LPVOID param){ RogueMutex *m=(RogueMutex*)param; for(int i=0;i<1000;i++){ if(rogue_mutex_acquire(m, ROGUE_LOCK_PRIORITY_NORMAL, -1)==0){ rogue_mutex_release(m);} } return 0; }
#else
#include <pthread.h>
#include <unistd.h>
static void* stress_thread(void* param){ RogueMutex *m=(RogueMutex*)param; for(int i=0;i<1000;i++){ if(rogue_mutex_acquire(m, ROGUE_LOCK_PRIORITY_NORMAL, -1)==0){ rogue_mutex_release(m);} } return NULL; }
#endif

static void test_4_8_1_entity_id_uniqueness(){
    const int N=2000; // moderate for CI speed
    RogueEntityId *ids = (RogueEntityId*)malloc(sizeof(RogueEntityId)*N);
    for(int i=0;i<N;i++){ ids[i]=rogue_entity_id_generate(ROGUE_ENTITY_ENEMY); assert(rogue_entity_id_validate(ids[i])); }
    for(int i=0;i<N;i++){
        for(int j=i+1;j<N;j++){
            assert(ids[i]!=ids[j]);
        }
    }
    free(ids);
}

static void test_4_8_5_cow_concurrent(){
    const char base[] = "abcdefghijklmnopqrstuvwxyz012345"; // 32 bytes
    RogueCowBuffer *a = rogue_cow_create_from_bytes(base,sizeof(base),32);
    RogueCowBuffer *b = rogue_cow_clone(a);
    const char mod1[] = "HELLO"; const char mod2[] = "WORLD";
    assert(rogue_cow_write(a, 2, mod1, sizeof(mod1)-1)==0);
    assert(rogue_cow_write(b, 10, mod2, sizeof(mod2)-1)==0);
    char r1[6]={0}; char r2[6]={0};
    assert(rogue_cow_read(a,2,r1,5)==0); assert(rogue_cow_read(b,10,r2,5)==0);
    assert(strcmp(r1,"HELLO")==0); assert(strcmp(r2,"WORLD")==0);
    char chk[6]={0};
    assert(rogue_cow_read(a,10,chk,5)==0); assert(strcmp(chk,"klmno")==0);
    assert(rogue_cow_read(b,2,chk,5)==0); assert(strncmp(chk,"cdefg",5)==0);
    rogue_cow_destroy(a); rogue_cow_destroy(b);
}

static void test_4_8_7_resource_contention_stress(){
    RogueMutex *m = rogue_mutex_create(50,"stress_mtx"); assert(m);
    #define PHASE4_8_THREADS 4
#ifdef _WIN32
    HANDLE th[PHASE4_8_THREADS];
    for(int i=0;i<PHASE4_8_THREADS;i++){ th[i]=CreateThread(NULL,0,stress_thread,m,0,NULL); assert(th[i]); }
    for(int i=0;i<PHASE4_8_THREADS;i++){ WaitForSingleObject(th[i],INFINITE); CloseHandle(th[i]); }
#else
    pthread_t th[PHASE4_8_THREADS];
    for(int i=0;i<PHASE4_8_THREADS;i++){ pthread_create(&th[i],NULL,stress_thread,m); }
    for(int i=0;i<PHASE4_8_THREADS;i++){ pthread_join(th[i],NULL); }
#endif
    RogueLockStats st; memset(&st,0,sizeof(st)); rogue_mutex_get_stats(m,&st);
    assert(st.acquisitions >= 1000); // basic sanity
    rogue_mutex_destroy(m);
}

int main(){
    test_4_8_1_entity_id_uniqueness();
    test_4_8_5_cow_concurrent();
    test_4_8_7_resource_contention_stress();
    printf("[phase4_8] tests complete\n");
    return 0;
}
