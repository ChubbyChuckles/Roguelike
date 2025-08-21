/* Loot tuning console commands (feature 9.6)
 * Provides a lightweight string-based command interface intended for
 * developer / test harness usage. This avoids needing an in-game UI
 * while still exercising parsing + logic via unit tests.
 *
 * Supported commands (case-insensitive leading token):
 *   weight <rarity_int 0-4> <factor_float>  -- set dynamic rarity factor
 *   reset_dyn                              -- reset dynamic rarity factors to 1.0
 *   reset_stats                            -- clear rolling rarity statistics window
 *   stats                                  -- emit a one-line histogram summary
 *   get <rarity_int 0-4>                   -- query current factor for rarity
 * Returns 0 on success, non-zero on parse / semantic error.
 * If out/out_sz provided, writes a short human readable message.
 */
#ifndef ROGUE_LOOT_COMMANDS_H
#define ROGUE_LOOT_COMMANDS_H

int rogue_loot_run_command(const char* line, char* out, int out_sz);

#endif /* ROGUE_LOOT_COMMANDS_H */
