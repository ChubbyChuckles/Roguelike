#ifndef ROGUE_CORE_SKILLS_COEFFS_LOAD_H
#define ROGUE_CORE_SKILLS_COEFFS_LOAD_H

#ifdef __cplusplus
extern "C"
{
#endif

    /* Load skill coefficient parameters from a config file. Supports:
     *  - JSON array (.json): [{ "skill_id":int, "base_scalar":num, "per_rank_scalar":num,
     *                           "str_pct_per10":num, "int_pct_per10":num, "dex_pct_per10":num,
     *                           "stat_cap_pct":num, "stat_softness":num }, ...]
     *  - CSV (.cfg): lines as: skill_id,base,per_rank,str,int,dex,cap,softness
     * Returns number of entries registered, or <0 on error.
     */
    int rogue_skill_coeffs_load_from_cfg(const char* path);

    /* Parse a JSON text buffer (same schema as above). Returns number registered or <0 on error. */
    int rogue_skill_coeffs_parse_json_text(const char* json_text);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_CORE_SKILLS_COEFFS_LOAD_H */
