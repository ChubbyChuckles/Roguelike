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

#endif
