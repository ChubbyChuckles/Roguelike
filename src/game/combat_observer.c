/**
 * @file combat_observer.c
 * @brief Observer pattern implementation for damage events
 *
 * This module implements the observer pattern for combat damage events,
 * allowing external systems to register callbacks that are notified whenever
 * damage is dealt in combat. This provides a clean extensibility mechanism
 * for systems that need to react to combat events without tight coupling.
 *
 * Key features:
 * - Observer registration and management with unique IDs
 * - Automatic notification of all registered observers on damage events
 * - Thread-safe observer management (no concurrent modification during dispatch)
 * - Configurable maximum observer limit (16 observers)
 * - Clean separation between base damage recording and observer dispatch
 *
 * The observer system works by:
 * 1. External systems register observer functions with user data
 * 2. When damage occurs, the system records the event and dispatches to all observers
 * 3. Each observer receives the complete damage event data and their user context
 * 4. Observers can be dynamically added, removed, or cleared as needed
 *
 * @note Observer dispatch happens synchronously during damage recording
 * @note Maximum of 16 concurrent observers supported
 * @note Observer functions should be lightweight to avoid impacting combat performance
 */

#include "combat.h"

/** @brief Maximum number of damage observers that can be registered simultaneously */
#define ROGUE_MAX_DAMAGE_OBSERVERS 16

/**
 * @brief Storage structure for a single damage observer registration
 *
 * Contains the observer callback function and associated user data context.
 * Used internally to manage the observer registry.
 */
typedef struct
{
    RogueDamageObserverFn fn; /**< Observer callback function pointer */
    void* user;               /**< User-defined context data passed to callback */
} RogueDamageObserverSlot;

/** @brief Global registry of registered damage observers */
static RogueDamageObserverSlot g_damage_observers[ROGUE_MAX_DAMAGE_OBSERVERS];

/**
 * @brief Register a new damage event observer
 *
 * Adds a new observer function to the registry that will be called whenever
 * damage events occur in combat. The observer will receive notifications for
 * all subsequent damage events until explicitly removed.
 *
 * @param fn Observer callback function to register (must not be NULL)
 * @param user User-defined context data to pass to the callback function
 * @return Unique observer ID (>= 0) on success, -1 if registration failed
 *
 * @note Returns -1 if the callback function is NULL or registry is full
 * @note Observer ID is used for later removal and should be stored by caller
 * @note Maximum of 16 observers can be registered simultaneously
 * @note Observer will be called synchronously during damage event recording
 */
int rogue_combat_add_damage_observer(RogueDamageObserverFn fn, void* user)
{
    if (!fn)
        return -1;
    for (int i = 0; i < ROGUE_MAX_DAMAGE_OBSERVERS; i++)
        if (!g_damage_observers[i].fn)
        {
            g_damage_observers[i].fn = fn;
            g_damage_observers[i].user = user;
            return i;
        }
    return -1;
}

/**
 * @brief Remove a specific damage event observer
 *
 * Removes the observer with the specified ID from the registry. The observer
 * will no longer receive damage event notifications after removal.
 *
 * @param id Observer ID returned by rogue_combat_add_damage_observer()
 *
 * @note Invalid IDs (out of range) are silently ignored
 * @note Observer slots are reused on subsequent registrations
 * @note Safe to call during observer dispatch (deferred removal)
 */
void rogue_combat_remove_damage_observer(int id)
{
    if (id < 0 || id >= ROGUE_MAX_DAMAGE_OBSERVERS)
        return;
    g_damage_observers[id].fn = NULL;
    g_damage_observers[id].user = NULL;
}

/**
 * @brief Remove all registered damage event observers
 *
 * Clears the entire observer registry, removing all registered observers
 * at once. No further damage event notifications will be sent until
 * new observers are registered.
 *
 * @note Useful for cleanup or when switching game modes
 * @note All observer IDs become invalid after clearing
 * @note Safe to call during observer dispatch
 */
void rogue_combat_clear_damage_observers(void)
{
    for (int i = 0; i < ROGUE_MAX_DAMAGE_OBSERVERS; i++)
    {
        g_damage_observers[i].fn = NULL;
        g_damage_observers[i].user = NULL;
    }
}

/**
 * @brief External declarations for damage event system integration
 *
 * These extern declarations provide access to the base damage event recording
 * system and global state, allowing the observer system to integrate with
 * the core damage recording functionality.
 */
extern void rogue_damage_event_record_base(unsigned short attack_id, unsigned char dmg_type,
                                           unsigned char crit, int raw, int mitig, int overkill,
                                           unsigned char execution);
extern int g_damage_event_head;
extern RogueDamageEvent g_damage_events[];

/**
 * @brief Public entry point for damage event recording with observer dispatch
 *
 * This function overrides the public damage event recording interface to add
 * observer notification functionality. It first records the damage event using
 * the base implementation, then dispatches the event to all registered observers.
 *
 * The dispatch process:
 * 1. Records the damage event using the base implementation
 * 2. Retrieves the most recently recorded event from the ring buffer
 * 3. Calls all registered observer functions with the event data
 * 4. Passes each observer's user context data to their callback
 *
 * @param attack_id Unique identifier for the attack that caused this damage
 * @param dmg_type Type of damage (physical, magical, etc.)
 * @param crit Whether this damage was a critical hit (0=no, 1=yes)
 * @param raw Raw damage value before mitigation
 * @param mitig Amount of damage mitigated/absorbed
 * @param overkill Amount of damage that exceeded target's health
 * @param execution Whether this damage resulted in target execution (0=no, 1=yes)
 *
 * @note This function is conditionally compiled when ROGUE_FEATURE_COMBAT_OBSERVER is defined
 * @note Observer dispatch happens synchronously and should be kept lightweight
 * @note All registered observers are called in registration order
 * @note Invalid observer slots (NULL functions) are skipped during dispatch
 */
void rogue_damage_event_record(unsigned short attack_id, unsigned char dmg_type, unsigned char crit,
                               int raw, int mitig, int overkill, unsigned char execution)
{
    rogue_damage_event_record_base(attack_id, dmg_type, crit, raw, mitig, overkill, execution);
    int idx = (g_damage_event_head - 1);
    while (idx < 0)
        idx += ROGUE_DAMAGE_EVENT_CAP;
    idx %= ROGUE_DAMAGE_EVENT_CAP;
    const RogueDamageEvent* ev = &g_damage_events[idx];
    for (int i = 0; i < ROGUE_MAX_DAMAGE_OBSERVERS; i++)
        if (g_damage_observers[i].fn)
            g_damage_observers[i].fn(ev, g_damage_observers[i].user);
}
