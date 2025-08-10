/**
 * @file ec_task.h
 * @brief FreeRTOS-ready scaffolding for EC control task (optional)
 */
#ifndef EC_TASK_H
#define EC_TASK_H

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

void ec_task_init(void);
bool ec_task_start(void);
void ec_task_stop(void);
bool ec_task_is_running(void);
void ec_task_set_period_ms(uint32_t period_ms);

#ifdef __cplusplus
}
#endif

#endif // EC_TASK_H
