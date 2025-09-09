/**
 * @file input_events.h
 * @brief High-level input event processing system.
 *
 * Provides functions for processing input events and managing skill
 * activation queues. This module sits above the basic input handling
 * and translates input events into game actions and skill activations.
 *
 * @author [Your Name]
 * @date September 2025
 * @version 1.0
 */

#ifndef ROGUE_INPUT_EVENTS_H
#define ROGUE_INPUT_EVENTS_H
#include "../core/app/app_state.h"

/**
 * @brief Process all pending input events.
 *
 * Handles all queued input events, translating them into appropriate
 * game actions. This includes movement commands, skill activations,
 * menu navigation, and other input-driven behaviors.
 */
void rogue_process_events(void);

/**
 * @brief Process queued skill activations.
 *
 * Executes any skill activations that were queued during event polling.
 * This function should be called after movement processing to ensure
 * skills are activated in the correct game state context.
 * 
 * @note This separation allows for proper ordering of game logic where
 *       movement is processed before skill effects are applied.
 */
void rogue_process_pending_skill_activations(void);

#endif /* ROGUE_INPUT_EVENTS_H */
