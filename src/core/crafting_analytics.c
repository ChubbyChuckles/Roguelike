#include "crafting_analytics.h"
#include <string.h>
#include <stdio.h>

#ifndef ROGUE_MATERIAL_DEF_CAP
#define ROGUE_MATERIAL_DEF_CAP 512
#endif

static unsigned int s_session_start_ms=0;
static unsigned int s_last_event_ms=0;
static unsigned int s_total_nodes=0;
static unsigned int s_rare_nodes=0;

static unsigned int s_craft_events=0;
static unsigned int s_craft_success=0;
static unsigned int s_quality_hist[101]; /* 0..100 inclusive */

static unsigned int s_enh_attempts=0;
static double s_enh_expected_accum=0.0; /* sum of expected risks */
static unsigned int s_enh_failures=0; /* treat failure if success_flag==0 */

static unsigned long long s_material_acquire[ROGUE_MATERIAL_DEF_CAP];
static unsigned long long s_material_consume[ROGUE_MATERIAL_DEF_CAP];

static int s_drift_alert_latched=0;

void rogue_craft_analytics_reset(void){
    s_session_start_ms=0; s_last_event_ms=0; s_total_nodes=0; s_rare_nodes=0;
    s_craft_events=0; s_craft_success=0; memset(s_quality_hist,0,sizeof s_quality_hist);
    s_enh_attempts=0; s_enh_expected_accum=0.0; s_enh_failures=0;
    memset(s_material_acquire,0,sizeof s_material_acquire);
    memset(s_material_consume,0,sizeof s_material_consume);
    s_drift_alert_latched=0;
}

void rogue_craft_analytics_on_node_harvest(int node_def_index,int material_def_index,int qty,int rare_flag,unsigned int now_ms){ (void)node_def_index; (void)material_def_index; (void)qty; if(s_session_start_ms==0) s_session_start_ms=now_ms; s_last_event_ms=now_ms; s_total_nodes++; if(rare_flag) s_rare_nodes++; }

void rogue_craft_analytics_on_craft(int recipe_index,int quality_out,int success_flag){ (void)recipe_index; if(quality_out<0) quality_out=0; if(quality_out>100) quality_out=100; s_craft_events++; if(success_flag) s_craft_success++; s_quality_hist[quality_out]++; }

void rogue_craft_analytics_on_enhancement(float expected_risk,int success_flag){ if(expected_risk<0) expected_risk=0; if(expected_risk>1) expected_risk=1; s_enh_attempts++; s_enh_expected_accum += expected_risk; if(!success_flag) s_enh_failures++; }

void rogue_craft_analytics_material_acquire(int material_def_index,int qty){ if(material_def_index>=0 && material_def_index<ROGUE_MATERIAL_DEF_CAP && qty>0) s_material_acquire[material_def_index]+= (unsigned long long)qty; }
void rogue_craft_analytics_material_consume(int material_def_index,int qty){ if(material_def_index>=0 && material_def_index<ROGUE_MATERIAL_DEF_CAP && qty>0) s_material_consume[material_def_index]+= (unsigned long long)qty; }

float rogue_craft_analytics_nodes_per_hour(unsigned int now_ms){ if(s_session_start_ms==0 || s_total_nodes==0) return 0.0f; unsigned int elapsed = (now_ms - s_session_start_ms); if(elapsed==0) return 0.0f; double hours = (double)elapsed / (1000.0*3600.0); return (float)(s_total_nodes / hours); }
float rogue_craft_analytics_rare_proc_rate(void){ if(s_total_nodes==0) return 0.0f; return (float)s_rare_nodes / (float)s_total_nodes; }
float rogue_craft_analytics_enhance_risk_variance(void){ if(s_enh_attempts==0) return 0.0f; double expected_mean = s_enh_expected_accum / (double)s_enh_attempts; double observed_fail = (double)s_enh_failures / (double)s_enh_attempts; return (float)(observed_fail - expected_mean); }

int rogue_craft_analytics_check_quality_drift(void){ if(s_drift_alert_latched) return 1; if(s_craft_events < 20) return 0; /* need sample size */
    /* Compute average quality */
    unsigned long long total_q=0; for(int i=0;i<=100;i++){ total_q += (unsigned long long)i * (unsigned long long)s_quality_hist[i]; }
    double avg = (double)total_q / (double)s_craft_events;
    /* Expected mid distribution ~50 if uniform; we allow wide band ±25 before alert. */
    if(avg < 25.0 || avg > 75.0){ s_drift_alert_latched=1; return 1; }
    return 0;
}

int rogue_craft_analytics_export_json(char* buf,int cap){ if(!buf||cap<=0) return -1; int written=0; int n;
    double qual_avg = 0.0;
    if(s_craft_events){
        unsigned long long tq=0; for(int i=0;i<=100;i++){ tq += (unsigned long long)i * (unsigned long long)s_quality_hist[i]; }
        qual_avg = (double)tq / (double)s_craft_events;
    }
    n = snprintf(buf+written, cap-written, "{\n  \"nodes_total\":%u,\n  \"nodes_rare\":%u,\n  \"rare_rate\":%.4f,\n  \"craft_events\":%u,\n  \"craft_success\":%u,\n  \"enh_attempts\":%u,\n  \"enh_expected_mean\":%.4f,\n  \"enh_observed_fail\":%.4f,\n  \"enh_risk_variance\":%.4f,\n  \"quality_avg\":%.2f,\n  \"quality_hist\":[",
        s_total_nodes,s_rare_nodes, rogue_craft_analytics_rare_proc_rate(), s_craft_events, s_craft_success, s_enh_attempts,
        s_enh_attempts? (float)(s_enh_expected_accum / (double)s_enh_attempts):0.0f,
        s_enh_attempts? (float)((double)s_enh_failures / (double)s_enh_attempts):0.0f,
        rogue_craft_analytics_enhance_risk_variance(),
        (float)qual_avg
    );
    if(n<0) return -1; written+=n; if(written>=cap) return -1;
    for(int i=0;i<=100;i++){
        n = snprintf(buf+written, cap-written, "%u%s", s_quality_hist[i], (i<100)?",":"" );
        if(n<0) return -1; written+=n; if(written>=cap) return -1;
    }
    n = snprintf(buf+written, cap-written, "],\n  \"drift_alert\":%d,\n  \"materials\":[", s_drift_alert_latched);
    if(n<0) return -1; written+=n; if(written>=cap) return -1;
    int first=1;
    for(int i=0;i<ROGUE_MATERIAL_DEF_CAP;i++){
        unsigned long long acq = s_material_acquire[i]; unsigned long long con = s_material_consume[i];
        if(acq||con){
            n = snprintf(buf+written, cap-written, "%s{\"id\":%d,\"acq\":%llu,\"con\":%llu}", first?"":",", i, acq, con);
            if(n<0) return -1; written+=n; if(written>=cap) return -1; first=0;
        }
    }
    n = snprintf(buf+written, cap-written, "]\n}\n"); if(n<0) return -1; written+=n; if(written>=cap) return -1; return written; }
