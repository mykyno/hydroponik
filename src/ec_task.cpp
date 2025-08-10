/**
 * @file ec_task.cpp
 * @brief Optional FreeRTOS task wrapper for EC control
 */
#include <Arduino.h>
#include "tasks/ec_task.h"
#include "pump.h"
#include "sensors.h"
#include "state_machine.h"

#ifndef ENABLE_EC_TASK
#define ENABLE_EC_TASK 0
#endif

#if ENABLE_EC_TASK
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static StaticTask_t ec_task_tcb;
static StackType_t ec_task_stack[2048 / sizeof(StackType_t)];
static TaskHandle_t ec_task_handle = nullptr;
static volatile uint32_t ec_task_period_ms = 10000; // slower cadence for EC

static void ec_task_entry(void* arg) {
  (void)arg;
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    if (state_manager.system_state == SystemState::MONITORING || state_manager.system_state == SystemState::DOSING) {
      if (sensor_update_needed()) {
        sensor_readings_t r = sensor_read_all();
        if (r.valid && pump_system.auto_ec_control) {
          // Placeholder for future EC dosing logic; currently no-op
          (void)r;
          // pump_ec_dose(r.ec, r.volume);
        }
      }
      pump_update();
    }
    vTaskDelayUntil(&last, pdMS_TO_TICKS(ec_task_period_ms));
  }
}
#endif // ENABLE_EC_TASK

void ec_task_init(void) {}

bool ec_task_start(void) {
#if ENABLE_EC_TASK
  if (ec_task_handle) return true;
  ec_task_handle = xTaskCreateStatic(ec_task_entry, "ECTask",
                                     sizeof(ec_task_stack) / sizeof(StackType_t),
                                     nullptr, tskIDLE_PRIORITY + 1,
                                     ec_task_stack, &ec_task_tcb);
  return ec_task_handle != nullptr;
#else
  return false;
#endif
}

void ec_task_stop(void) {
#if ENABLE_EC_TASK
  if (ec_task_handle) {
    vTaskDelete(ec_task_handle);
    ec_task_handle = nullptr;
  }
#endif
}

bool ec_task_is_running(void) {
#if ENABLE_EC_TASK
  return ec_task_handle != nullptr;
#else
  return false;
#endif
}

void ec_task_set_period_ms(uint32_t period_ms) {
#if ENABLE_EC_TASK
  ec_task_period_ms = period_ms;
#else
  (void)period_ms;
#endif
}
