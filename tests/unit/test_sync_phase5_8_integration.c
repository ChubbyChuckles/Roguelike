#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../../src/core/integration/transaction_manager.h"

static int prep_ok(void* u,int tx,char* e,size_t c,uint32_t* ov){ (void)u;(void)tx;(void)e;(void)c; *ov=1; return 0; }
static int prep_fail(void* u,int tx,char* e,size_t c,uint32_t* ov){ (void)u;(void)tx;(void)ov; if(c) strncpy(e,"prep fail",c-1); return -1; }
static int commit_ok(void* u,int tx){ (void)u;(void)tx; return 0; }
static int abort_rec(void* u,int tx){ (void)u;(void)tx; return 0; }
static uint32_t getv(void* u){ (void)u; return 1; }

int main(void){
    rogue_tx_reset_all();
    RogueTxParticipantDesc P1={.participant_id=11,.name="P1",.user=NULL,.on_prepare=prep_ok,.on_commit=commit_ok,.on_abort=abort_rec,.get_version=getv};
    RogueTxParticipantDesc P2={.participant_id=12,.name="P2",.user=NULL,.on_prepare=prep_ok,.on_commit=commit_ok,.on_abort=abort_rec,.get_version=getv};
    rogue_tx_register_participant(&P1); rogue_tx_register_participant(&P2);
    // success
    int t1=rogue_tx_begin(ROGUE_TX_ISO_READ_COMMITTED,1000); rogue_tx_mark(t1,11); rogue_tx_mark(t1,12);
    if(rogue_tx_commit(t1)!=0 || rogue_tx_get_state(t1)!=ROGUE_TX_STATE_COMMITTED){ fprintf(stderr,"multi success failed\n"); return 1; }
    // failure prepare of P2
    RogueTxParticipantDesc P2b={.participant_id=13,.name="P2b",.user=NULL,.on_prepare=prep_fail,.on_commit=commit_ok,.on_abort=abort_rec,.get_version=getv};
    rogue_tx_register_participant(&P2b);
    int t2=rogue_tx_begin(ROGUE_TX_ISO_READ_COMMITTED,1000); rogue_tx_mark(t2,11); rogue_tx_mark(t2,13);
    if(rogue_tx_commit(t2)==0 || rogue_tx_get_state(t2)!=ROGUE_TX_STATE_ABORTED){ fprintf(stderr,"multi fail not aborted\n"); return 1; }
    printf("SYNC_5_8_INTEGRATION_OK\n");
    return 0;
}
