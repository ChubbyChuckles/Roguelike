#ifndef ROGUE_EQUIPMENT_FUZZ_H
#define ROGUE_EQUIPMENT_FUZZ_H
/* Phase 18.2: Fuzz equip/un-equip sequence tester.
   Returns count of invariant violations detected over N iterations with deterministic seed. */
int rogue_equipment_fuzz_sequences(int iterations, int seed);
#endif
