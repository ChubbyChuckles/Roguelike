#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "core/encounter_composer.h"

static const char* kEncFile = "encounters.cfg"; /* tests run in build dir */

static void write_encounters_file(void){
    FILE* f = fopen(kEncFile, "wb"); assert(f);
    fputs("id=0\nname=Swarm\ntype=swarm\nmin=6\nmax=8\nelite_spacing=3\nelite_chance=0.5\n\n", f);
    fputs("id=1\nname=BossRoom\ntype=boss_room\nmin=1\nmax=1\nboss=1\nsupport_min=2\nsupport_max=3\nelite_spacing=4\nelite_chance=0.0\n\n", f);
    fclose(f);
}

static void test_load(){
    write_encounters_file();
    int n = rogue_encounters_load_file(kEncFile);
    assert(n==2);
    assert(rogue_encounter_template_count()==2);
    const RogueEncounterTemplate* t0 = rogue_encounter_template_by_id(0);
    assert(t0 && t0->min_count==6 && t0->max_count==8);
}

static void test_compose_swarm(){
    RogueEncounterComposition comp; memset(&comp,0,sizeof comp);
    for(int i=0;i<10;i++){
        int r = rogue_encounter_compose(0, 10, 10, 0, 1234u+i, &comp); assert(r==0);
        assert(comp.unit_count>=6 && comp.unit_count<=8);
        for(int u=0;u<comp.unit_count;u++){ assert(comp.units[u].level==10); }
    }
}

static void test_compose_boss(){
    RogueEncounterComposition comp; memset(&comp,0,sizeof comp);
    int r = rogue_encounter_compose(1, 20, 20, 0, 999u, &comp); assert(r==0);
    assert(comp.boss_present==1);
    assert(comp.unit_count>=1);
    assert(comp.support_count>=2 && comp.support_count<=3);
}

int main(void){
    test_load();
    test_compose_swarm();
    test_compose_boss();
    printf("OK test_enemy_difficulty_phase3 (%d templates)\n", rogue_encounter_template_count());
    return 0;
}
