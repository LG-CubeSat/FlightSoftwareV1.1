#ifndef ADCS_BOARD_H
#define ADCS_BOARD_H
void adcs_application_start(void);
void adcs_watchdog_start(void);
void adcs_watchdog_pet(void);
void adcs_csp_init(void);
void adcs_stdio_lock(void);
void adcs_stdio_unlock(void);
#endif
