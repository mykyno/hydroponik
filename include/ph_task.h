/**
 * @file ph_task.h
 * @brief FreeRTOS-ready scaffolding for pH control task (optional)
 *
 * This module provides an optional task wrapper for pH control. By default,
 * it compiles to lightweight stubs unless ENABLE_PH_TASK is defined.
 * No dynamic allocation; uses xTaskCreateStatic when enabled.
 */

#ifndef PH_TASK_H
#define PH_TASK_H

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

// API
void ph_task_init(void);
bool ph_task_start(void);          // returns true if task started (or already running)
void ph_task_stop(void);           // stops and deletes the task if running
bool ph_task_is_running(void);
void ph_task_set_period_ms(uint32_t period_ms); // set loop period when running as a task

#ifdef __cplusplus
}
#endif

#endif // PH_TASK_H
