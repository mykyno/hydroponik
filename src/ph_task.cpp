/**
 * @file ph_task.cpp
 * @brief Optional FreeRTOS task wrapper for pH control
 */

#include <Arduino.h>
#include "tasks/ph_task.h"
#include "pump.h"
#include "sensors.h"
#include "state_machine.h"

// Define ENABLE_PH_TASK to 1 to enable task creation; otherwise, stubs only
#ifndef ENABLE_PH_TASK
#define ENABLE_PH_TASK 0
#endif

#if ENABLE_PH_TASK
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static StaticTask_t ph_task_tcb;
static StackType_t ph_task_stack[2048 / sizeof(StackType_t)];
static TaskHandle_t ph_task_handle = nullptr;
static volatile uint32_t ph_task_period_ms = 5000; // default: 5s check cadence

static void ph_task_entry(void* arg) {
  (void)arg;
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    // Only operate in monitoring/dosing modes
    if (state_manager.system_state == SystemState::MONITORING || state_manager.system_state == SystemState::DOSING) {
      if (sensor_update_needed()) {
        sensor_readings_t r = sensor_read_all();
        if (r.valid && pump_is_auto_ph_enabled()) {
          system_transition_to(SystemState::DOSING);
          (void)pump_ph_dose(r.ph, r.volume);
          system_transition_to(SystemState::MONITORING);
        }
      }
      pump_update();
    }
    vTaskDelayUntil(&last, pdMS_TO_TICKS(ph_task_period_ms));
  }
}
#endif // ENABLE_PH_TASK

void ph_task_init(void) {
  // nothing needed when disabled
}

bool ph_task_start(void) {
#if ENABLE_PH_TASK
  if (ph_task_handle) return true;
  ph_task_handle = xTaskCreateStatic(ph_task_entry, "PHTask",
                                     sizeof(ph_task_stack) / sizeof(StackType_t),
                                     nullptr, tskIDLE_PRIORITY + 1,
                                     ph_task_stack, &ph_task_tcb);
  return ph_task_handle != nullptr;
#else
  return false;
#endif
}

void ph_task_stop(void) {
#if ENABLE_PH_TASK
  if (ph_task_handle) {
    vTaskDelete(ph_task_handle);
    ph_task_handle = nullptr;
  }
#endif
}

bool ph_task_is_running(void) {
#if ENABLE_PH_TASK
  return ph_task_handle != nullptr;
#else
  return false;
#endif
}

void ph_task_set_period_ms(uint32_t period_ms) {
#if ENABLE_PH_TASK
  ph_task_period_ms = period_ms;
#else
  (void)period_ms;
#endif
}
