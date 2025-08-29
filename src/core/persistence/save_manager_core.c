#include "save_manager_core.h"
#include "../../util/log.h"
#include "save_internal.h"

/* Forward declarations for component registration implemented in components module */
void rogue_register_all_components_internal(void);

/* External API glue */
void rogue_save_manager_init(void)
{
    if (!g_save_initialized)
        g_save_initialized = 1;
    if (!g_save_migrations_registered)
    {
        rogue_register_core_migrations_internal();
        g_save_migrations_registered = 1;
    }
}

void rogue_save_manager_register(const RogueSaveComponent* comp)
{
    if (g_save_component_count >= ROGUE_SAVE_MAX_COMPONENTS || !comp)
        return;
    for (int i = 0; i < g_save_component_count; i++)
        if (g_save_components[i].id == comp->id)
            return;
    g_save_components[g_save_component_count++] = *comp;
}

void rogue_save_register_migration(const RogueSaveMigration* mig)
{
    if (!mig)
        return;
    if (g_save_migration_count < (int) (sizeof g_save_migrations / sizeof g_save_migrations[0]))
        g_save_migrations[g_save_migration_count++] = *mig;
}

/* Expose for existing code */
void rogue_register_core_save_components(void) { rogue_register_all_components_internal(); }
