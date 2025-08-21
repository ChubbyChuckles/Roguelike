#include "core/integration/entity_id.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_generate_and_validate(){
    RogueEntityId ids[1000];
    for(int i=0;i<1000;i++){ ids[i]=rogue_entity_id_generate(ROGUE_ENTITY_ENEMY); assert(rogue_entity_id_validate(ids[i])); }
    for(int i=1;i<1000;i++){ assert(ids[i]!=ids[i-1]); }
}

static void test_type_and_sequence(){
    RogueEntityId a = rogue_entity_id_generate(ROGUE_ENTITY_ITEM);
    RogueEntityType t = rogue_entity_id_type(a); assert(t==ROGUE_ENTITY_ITEM);
    uint64_t seq = rogue_entity_id_sequence(a); assert(seq>0);
}

static void test_checksum_flip(){
    RogueEntityId id = rogue_entity_id_generate(ROGUE_ENTITY_PLAYER);
    assert(rogue_entity_id_validate(id));
    RogueEntityId bad = id ^ 0x1; /* flip a bit in checksum byte */
    assert(!rogue_entity_id_validate(bad));
}

static void test_registry(){
    RogueEntityId id = rogue_entity_id_generate(ROGUE_ENTITY_WORLD);
    int obj = 42; assert(rogue_entity_register(id,&obj)==0); assert(rogue_entity_lookup(id)==&obj); assert(rogue_entity_release(id)==0); assert(rogue_entity_lookup(id)==NULL); }

static void test_serialize_roundtrip(){
    RogueEntityId id = rogue_entity_id_generate(ROGUE_ENTITY_ITEM);
    char buf[64]; int r = rogue_entity_id_serialize(id,buf,sizeof(buf)); assert(r>0); RogueEntityId parsed=0; assert(rogue_entity_id_parse(buf,&parsed)==0); assert(parsed==id);
}

int main(){
    test_generate_and_validate();
    test_type_and_sequence();
    test_checksum_flip();
    test_registry();
    test_serialize_roundtrip();
    printf("entity_id tests passed\n");
    return 0;
}
