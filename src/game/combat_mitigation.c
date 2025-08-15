#include "game/combat.h"
#include <math.h>

static int clampi(int v,int lo,int hi){ if(v<lo) return lo; if(v>hi) return hi; return v; }
static int rogue_effective_phys_resist(int p){ if(p<=0) return 0; if(p>90) p=90; int eff = p - (p*p)/300; if(eff<0) eff=0; if(eff>75) eff=75; return eff; }

int rogue_apply_mitigation_enemy(RogueEnemy* e, int raw, unsigned char dmg_type, int *out_overkill){
    if(!e || !e->alive) return 0;
    int dmg = raw; if(dmg<0) dmg=0;
    if(dmg_type != ROGUE_DMG_TRUE){
        if(dmg_type == ROGUE_DMG_PHYSICAL){
            int armor = e->armor; if(armor>0){ if(armor >= dmg) dmg = (dmg>1?1:dmg); else dmg -= armor; }
            int pr_raw = clampi(e->resist_physical,0,90); int pr = rogue_effective_phys_resist(pr_raw); if(pr>0){ int reduce=(dmg*pr)/100; dmg -= reduce; }
            if(raw >= ROGUE_DEF_SOFTCAP_MIN_RAW){
                float armor_frac = 0.0f; if(armor>0){ armor_frac = (float)armor / (float)(raw + armor); if(armor_frac>0.90f) armor_frac=0.90f; }
                float total_frac = armor_frac + (float)pr / 100.0f; if(total_frac > 0.0f){
                    if(total_frac > ROGUE_DEF_SOFTCAP_REDUCTION_THRESHOLD){
                        float excess = total_frac - ROGUE_DEF_SOFTCAP_REDUCTION_THRESHOLD;
                        float adjusted_excess = excess * ROGUE_DEF_SOFTCAP_SLOPE;
                        float capped_total = ROGUE_DEF_SOFTCAP_REDUCTION_THRESHOLD + adjusted_excess;
                        if(capped_total > ROGUE_DEF_SOFTCAP_MAX_REDUCTION) capped_total = ROGUE_DEF_SOFTCAP_MAX_REDUCTION;
                        int target = (int)floorf((float)raw * (1.0f - capped_total) + 0.5f);
                        if(target < 1) target = 1;
                        if(target > dmg) { }
                        else dmg = target;
                    }
                }
            }
        } else {
            int resist=0; switch(dmg_type){
                case ROGUE_DMG_FIRE: resist = e->resist_fire; break;
                case ROGUE_DMG_FROST: resist = e->resist_frost; break;
                case ROGUE_DMG_ARCANE: resist = e->resist_arcane; break;
                default: break; }
            resist = clampi(resist,0,90); if(resist>0){ int reduce=(dmg*resist)/100; dmg -= reduce; }
        }
    }
    if(dmg < 1) dmg = 1;
    int overkill = 0; if(e->health - dmg < 0){ overkill = dmg - e->health; }
    if(out_overkill) *out_overkill = overkill; return dmg;
}
