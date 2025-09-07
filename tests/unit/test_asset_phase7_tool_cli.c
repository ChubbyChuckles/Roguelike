/* test_asset_phase7_tool_cli.c - Smoke test for asset_tool stats command
   Invokes asset_tool_main directly with the 'stats' command to ensure
   successful execution path and no crashes in a headless environment. */
#include <assert.h>
#include <stdio.h>
int asset_tool_main(int argc, char** argv); /* from asset_tool_cli */

int main(void)
{
    char* argv[] = {"asset_tool", "stats"};
    int rc = asset_tool_main(2, argv);
    assert(rc == 0);
    printf("test_asset_phase7_tool_cli OK\n");
    return 0;
}
