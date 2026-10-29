#ifndef TEST_FREERTOS_H
#define TEST_FREERTOS_H
/* Unit-test stand-in for the short task-context clock critical sections. */
void test_enter_critical(void);
void test_exit_critical(void);
#define taskENTER_CRITICAL() test_enter_critical()
#define taskEXIT_CRITICAL() test_exit_critical()
#endif
