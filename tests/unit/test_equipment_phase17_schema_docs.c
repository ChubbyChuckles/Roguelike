/* Phase 17.1: Public schema docs exporter test */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../../src/core/equipment/equipment_schema_docs.h"

static void assert_true(int cond, const char* msg){ if(!cond){ printf("FAIL:%s\n", msg); exit(1);} }

int main(){
    char buf[2048]; int n = rogue_equipment_schema_docs_export(buf,sizeof buf);
    assert_true(n>0 && n < (int)sizeof(buf)-1, "export size bounds");
    assert_true(strstr(buf,"item_def")!=NULL, "has item_def section");
    assert_true(strstr(buf,"set_def")!=NULL, "has set_def section");
    assert_true(strstr(buf,"proc_def")!=NULL, "has proc_def section");
    assert_true(strstr(buf,"runeword_pattern")!=NULL, "has runeword_pattern section");
    assert_true(strstr(buf,"version")!=NULL, "has version field");
    printf("Phase17.1 schema docs exporter OK (%d bytes)\n", n);
    return 0;
}
