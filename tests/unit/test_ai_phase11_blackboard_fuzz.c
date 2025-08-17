#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include "ai/core/blackboard.h"

/* Simple xorshift for deterministic fuzz */
static uint32_t prng(uint32_t* s){ uint32_t x=*s; x^=x<<13; x^=x>>17; x^=x<<5; *s=x; return x; }

typedef struct ModelEntry { const char* key; int present; int i; float f; } ModelEntry;
#define MODEL_MAX 32
static ModelEntry model[MODEL_MAX]; static int model_count=0;
static ModelEntry* model_get(const char* key){ for(int i=0;i<model_count;i++){ if(model[i].key==key) return &model[i]; } if(model_count<MODEL_MAX){ model[model_count].key=key; model[model_count].present=0; return &model[model_count++]; } return NULL; }

int main(void){
    RogueBlackboard bb; rogue_bb_init(&bb);
    const char* keys[]={"a","b","c","d","e","f","g","h"}; int key_count=8;
    uint32_t seed=0xC0FFEEu;
    for(int iter=0; iter<5000; ++iter){
        uint32_t r=prng(&seed);
        const char* k = keys[r % key_count];
        int op = (r>>3) % 6; /* 0:set_int 1:set_float 2:write_int 3:write_float 4:set_timer 5:set_vec2 */
        int val = (int)(r>>9) & 0xFF;
        float fval = (float)( (r>>11) % 1000 ) / 10.0f; /* 0..99.9 */
        ModelEntry* me=model_get(k); assert(me);
        switch(op){
            case 0: rogue_bb_set_int(&bb,k,val); me->i=val; me->present=1; break;
            case 1: rogue_bb_set_float(&bb,k,fval); me->f=fval; me->present=1; break;
            case 2: {
                RogueBBWritePolicy pol = (RogueBBWritePolicy)((r>>17)%4);
                /* mimic policy for int */
                if(!me->present){ me->i=0; me->present=1; }
                int before=me->i;
                int changed=0; if(pol==ROGUE_BB_WRITE_SET){ me->i=val; changed=1; }
                else if(pol==ROGUE_BB_WRITE_MAX){ if(val>me->i){ me->i=val; changed=1; } }
                else if(pol==ROGUE_BB_WRITE_MIN){ if(val<me->i){ me->i=val; changed=1; } }
                else if(pol==ROGUE_BB_WRITE_ACCUM){ me->i+=val; changed=1; }
                rogue_bb_write_int(&bb,k,val,pol);
                /* verify blackboard state matches model for int */
                int got; if(rogue_bb_get_int(&bb,k,&got)){ assert(got==me->i); } else { assert(!changed); }
            } break;
            case 3: {
                RogueBBWritePolicy pol=(RogueBBWritePolicy)((r>>19)%4);
                if(!me->present){ me->f=0.f; me->present=1; }
                float before=me->f; int changed=0; if(pol==ROGUE_BB_WRITE_SET){ me->f=fval; changed=1; }
                else if(pol==ROGUE_BB_WRITE_MAX){ if(fval>me->f){ me->f=fval; changed=1; } }
                else if(pol==ROGUE_BB_WRITE_MIN){ if(fval<me->f){ me->f=fval; changed=1; } }
                else if(pol==ROGUE_BB_WRITE_ACCUM){ me->f+=fval; changed=1; }
                rogue_bb_write_float(&bb,k,fval,pol);
                float got; if(rogue_bb_get_float(&bb,k,&got)){ assert(fabsf(got-me->f) < 0.0001f); } else { assert(!changed); }
            } break;
            case 4: rogue_bb_set_timer(&bb,k,(float)(val%10)); break;
            case 5: rogue_bb_set_vec2(&bb,k,(float)(val%50),(float)((val*3)%50)); break;
        }
        /* ensure we never exceed capacity */
        assert(bb.count <= ROGUE_BB_MAX_ENTRIES);
    }
    printf("AI_PHASE11_FUZZ_OK entries=%d\n", bb.count);
    return 0;
}
