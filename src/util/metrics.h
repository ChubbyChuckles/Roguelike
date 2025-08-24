/* Frame timing & metrics module */
#ifndef ROGUE_CORE_METRICS_H
#define ROGUE_CORE_METRICS_H

/* Initialize / reset metrics counters. */
void rogue_metrics_reset(void);
/* Begin a frame; returns timestamp token. */
double rogue_metrics_frame_begin(void);
/* End a frame; updates global metrics using token from begin. */
void rogue_metrics_frame_end(double frame_start_seconds);
/* Query metrics (any pointer may be NULL). */
void rogue_metrics_get(double* out_fps, double* out_frame_ms, double* out_avg_frame_ms);
/* Delta time (seconds) of last frame. */
double rogue_metrics_delta_time(void);

/* Session metrics (Phase 9.5) */
double rogue_metrics_session_elapsed(void);
void rogue_metrics_record_drop(int rarity);
void rogue_metrics_record_pickup(int rarity);
void rogue_metrics_rates(double* out_items_per_hour, double out_rarity_per_hour[5]);

#endif
