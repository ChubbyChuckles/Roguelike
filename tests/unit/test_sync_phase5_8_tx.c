#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../../src/core/integration/transaction_manager.h"

static uint32_t vA=1, vB=1; static int fail_prepare_B=0;
static int on_prepare_A(void* u,int tx,char* err,size_t cap,uint32_t* outv){ (void)u;(void)tx;(void)err;(void)cap; *outv=++vA; return 0; }
static int on_prepare_B(void* u,int tx,char* err,size_t cap,uint32_t* outv){ (void)u;(void)tx; *outv=++vB; if(fail_prepare_B){ strncpy(err,"B prepare fail",cap?cap-1:0); return -1; } return 0; }
static int on_commit(void* u,int tx){ (void)u;(void)tx; return 0; }
static int on_abort(void* u,int tx){ (void)u;(void)tx; return 0; }
static uint32_t get_version_A(void* u){ (void)u; return vA; }
static uint32_t get_version_B(void* u){ (void)u; return vB; }

static uint64_t fake_now(void){ static uint64_t t=0; return t+=10; }

int main(void){
    RogueTxParticipantDesc A={.participant_id=1,.name="A",.user=NULL,.on_prepare=on_prepare_A,.on_commit=on_commit,.on_abort=on_abort,.get_version=get_version_A};
    RogueTxParticipantDesc B={.participant_id=2,.name="B",.user=NULL,.on_prepare=on_prepare_B,.on_commit=on_commit,.on_abort=on_abort,.get_version=get_version_B};
    rogue_tx_reset_all();
    if(rogue_tx_register_participant(&A)!=0 || rogue_tx_register_participant(&B)!=0){ fprintf(stderr,"reg failed\n"); return 2; }
    // Isolation violation case
    int tx1 = rogue_tx_begin(ROGUE_TX_ISO_REPEATABLE_READ, 100);
    if(tx1<0){ fprintf(stderr,"begin failed\n"); return 2; }
    rogue_tx_mark(tx1,1); rogue_tx_mark(tx1,2);
    uint32_t rA=0,rB=0; rogue_tx_read(tx1,1,&rA); rogue_tx_read(tx1,2,&rB);
    // mutate B version to violate repeatable read
    vB += 1;
    int c1 = rogue_tx_commit(tx1);
    if(c1==0 || rogue_tx_get_state(tx1)!=ROGUE_TX_STATE_ABORTED){ fprintf(stderr,"iso violation not aborted\n"); return 1; }
    // Successful path
    int tx2 = rogue_tx_begin(ROGUE_TX_ISO_READ_COMMITTED, 100);
    rogue_tx_mark(tx2,1); rogue_tx_mark(tx2,2);
    int c2 = rogue_tx_commit(tx2);
    if(c2!=0 || rogue_tx_get_state(tx2)!=ROGUE_TX_STATE_COMMITTED){ fprintf(stderr,"commit failed\n"); return 1; }
    // Timeout path
    rogue_tx_set_time_source(fake_now);
    int tx3 = rogue_tx_begin(ROGUE_TX_ISO_READ_COMMITTED, 5);
    int c3 = rogue_tx_commit(tx3);
    if(c3==0 || rogue_tx_get_state(tx3)!=ROGUE_TX_STATE_TIMED_OUT){ fprintf(stderr,"timeout not detected\n"); return 1; }
    // Prepare failure path (abort)
    fail_prepare_B=1;
    int tx4 = rogue_tx_begin(ROGUE_TX_ISO_READ_COMMITTED, 1000);
    rogue_tx_mark(tx4,1); rogue_tx_mark(tx4,2);
    int c4 = rogue_tx_commit(tx4);
    if(c4==0 || rogue_tx_get_state(tx4)!=ROGUE_TX_STATE_ABORTED){ fprintf(stderr,"prepare failure not aborted\n"); return 1; }
    printf("SYNC_5_8_TX_OK\n");
    return 0;
}
