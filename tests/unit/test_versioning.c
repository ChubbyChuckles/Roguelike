#include "../../src/core/integration/versioning.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Simple migrations for a pretend struct evolving:
// v1: struct { int a; }
// v2: struct { int a; int b; } -> add b initialized to a*2
// v3: struct { int a; int b; int c; } -> add c = a + b
// We represent data as raw bytes; migrations reallocate & adjust size.

typedef struct V1
{
    int a;
} V1;
typedef struct V2
{
    int a;
    int b;
} V2;
typedef struct V3
{
    int a;
    int b;
    int c;
} V3;

static int mig_1_2(void** data, size_t* size, void* user)
{
    if (*size != sizeof(V1))
        return -1;
    V1* old = (V1*) (*data);
    V2* n = (V2*) malloc(sizeof(V2));
    if (!n)
        return -1;
    n->a = old->a;
    n->b = old->a * 2;
    *data = n;
    *size = sizeof(V2);
    return 0;
}

static int mig_2_3(void** data, size_t* size, void* user)
{
    if (*size != sizeof(V2))
        return -1;
    V2* old = (V2*) (*data);
    V3* n = (V3*) malloc(sizeof(V3));
    if (!n)
        return -1;
    n->a = old->a;
    n->b = old->b;
    n->c = old->a + old->b;
    *data = n;
    *size = sizeof(V3);
    return 0;
}

static void test_basic_chain()
{
    assert(rogue_version_register_type("TestType", 3) == 0);
    assert(rogue_version_register_migration("TestType", 1, 2, mig_1_2, NULL) == 0);
    assert(rogue_version_register_migration("TestType", 2, 3, mig_2_3, NULL) == 0);
    V1* obj = (V1*) malloc(sizeof(V1));
    obj->a = 7;
    size_t sz = sizeof(V1);
    void* ptr = obj;
    RogueMigrationProgress prog;
    int rc = rogue_version_migrate("TestType", 1, 0, &ptr, &sz, &prog); // target 0 => current (3)
    assert(rc == 0);
    assert(sz == sizeof(V3));
    V3* v3 = (V3*) ptr;
    assert(v3->a == 7 && v3->b == 14 && v3->c == 21);
    assert(prog.steps_total == 2 && prog.steps_completed == 2 && prog.failed == 0);
    free(v3);
}

static int mig_fail(void** data, size_t* size, void* user) { return -1; }

static void test_failure_rollback()
{
    assert(rogue_version_register_type("FailType", 3) == 0);
    // Good first step
    assert(rogue_version_register_migration("FailType", 1, 2, mig_1_2, NULL) == 0);
    // Failing second step
    assert(rogue_version_register_migration("FailType", 2, 3, mig_fail, NULL) == 0);
    V1* obj = (V1*) malloc(sizeof(V1));
    obj->a = 5;
    size_t sz = sizeof(V1);
    void* ptr = obj;
    RogueMigrationProgress prog;
    int rc = rogue_version_migrate("FailType", 1, 0, &ptr, &sz, &prog);
    assert(rc != 0);
    // Rollback ensures original unchanged
    assert(ptr == obj && sz == sizeof(V1));
    assert(prog.failed == 1 && prog.fail_from == 2 && prog.steps_completed == 1);
    free(obj);
}

int main()
{
    test_basic_chain();
    test_failure_rollback();
    RogueVersioningStats st;
    rogue_versioning_get_stats(&st);
    printf("[versioning] types=%u migrations=%u executed=%llu steps=%llu failures=%llu\n",
           st.types_registered, st.migrations_registered,
           (unsigned long long) st.migrations_executed, (unsigned long long) st.migration_steps,
           (unsigned long long) st.migration_failures);
    return 0;
}
