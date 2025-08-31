#ifndef ROGUE_VALIDATION_WIRING_H
#define ROGUE_VALIDATION_WIRING_H

#ifdef __cplusplus
extern "C"
{
#endif

    /* Register default content and cross-rule validators with the validation manager. */
    void rogue_validation_register_default_checks(void);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_VALIDATION_WIRING_H */
