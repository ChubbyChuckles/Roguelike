/**
 * @file metrics.h
 * @brief Frame timing and performance metrics system.
 *
 * Provides frame timing measurement, performance tracking, and session
 * metrics collection. Includes FPS calculation, frame time averaging,
 * delta time tracking, and item drop/pickup rate statistics for
 * gameplay analytics and performance monitoring.
 *
 * @author [Your Name]
 * @date September 2025
 * @version 1.0
 */

#ifndef ROGUE_CORE_METRICS_H
#define ROGUE_CORE_METRICS_H

/**
 * @brief Initialize and reset all metrics counters.
 *
 * Resets all internal metrics tracking including frame timing,
 * session statistics, and performance averages. Should be called
 * at application startup or when beginning a new session.
 */
void rogue_metrics_reset(void);

/**
 * @brief Begin frame timing measurement.
 *
 * Starts timing measurement for the current frame. Returns a
 * timestamp token that must be passed to rogue_metrics_frame_end()
 * to complete the frame timing measurement.
 *
 * @return Timestamp token for this frame (in seconds)
 */
double rogue_metrics_frame_begin(void);

/**
 * @brief End frame timing measurement.
 *
 * Completes the frame timing measurement using the token from
 * rogue_metrics_frame_begin(). Updates global FPS, frame timing
 * averages, and delta time calculations.
 *
 * @param frame_start_seconds Timestamp token from rogue_metrics_frame_begin()
 */
void rogue_metrics_frame_end(double frame_start_seconds);

/**
 * @brief Query current performance metrics.
 *
 * Retrieves the current performance statistics including FPS,
 * current frame time, and averaged frame time. Any parameter
 * may be NULL if that metric is not needed.
 *
 * @param out_fps Pointer to receive current FPS (may be NULL)
 * @param out_frame_ms Pointer to receive current frame time in milliseconds (may be NULL)
 * @param out_avg_frame_ms Pointer to receive averaged frame time in milliseconds (may be NULL)
 */
void rogue_metrics_get(double* out_fps, double* out_frame_ms, double* out_avg_frame_ms);

/**
 * @brief Get delta time of the last frame.
 *
 * Returns the time elapsed during the last completed frame,
 * which is commonly used for time-based game logic updates.
 *
 * @return Delta time in seconds for the last frame
 */
double rogue_metrics_delta_time(void);

/**
 * @brief Get total elapsed session time.
 *
 * Returns the total time elapsed since the current session began
 * or since metrics were last reset.
 *
 * @return Total session time in seconds
 */
double rogue_metrics_session_elapsed(void);

/**
 * @brief Record an item drop event.
 *
 * Logs an item drop event for the specified rarity tier.
 * Used to track item drop rates and gameplay balance metrics.
 *
 * @param rarity Item rarity tier (0-4, where higher numbers are rarer)
 */
void rogue_metrics_record_drop(int rarity);

/**
 * @brief Record an item pickup event.
 *
 * Logs an item pickup event for the specified rarity tier.
 * Used to track player item acquisition rates and engagement metrics.
 *
 * @param rarity Item rarity tier (0-4, where higher numbers are rarer)
 */
void rogue_metrics_record_pickup(int rarity);

/**
 * @brief Calculate item acquisition rates.
 *
 * Computes the hourly rates of item drops and pickups based on
 * session time and recorded events. Useful for balancing and
 * analytics purposes.
 *
 * @param out_items_per_hour Pointer to receive total items per hour rate (may be NULL)
 * @param out_rarity_per_hour Array of 5 doubles to receive per-rarity hourly rates (may be NULL)
 */
void rogue_metrics_rates(double* out_items_per_hour, double out_rarity_per_hour[5]);

#endif
