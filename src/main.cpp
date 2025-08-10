/**
 * @file main.cpp
 * @brief Main program for ESP32-S3 hydroponic sensor system
 * @author Arduino Developer
 * @date 2025
 * 
 * This is the main entry point for the hydroponic sensor monitoring system.
 * All sensor logic is implemented in the sensors module.
 */

#include <Arduino.h>
#include <Preferences.h>
#include "sensors.h"
#include "calibration.h"
#include "pump.h"
#include "state_machine.h"
#include "communication.h"
#include "tasks/ph_task.h"
#include "tasks/ec_task.h"

//=============================================================================
// GLOBAL VARIABLES
//=============================================================================

// NVS preferences object for calibration storage
Preferences preferences;

// WiFi Configuration (modify these for your network)
const char* WIFI_SSID = "OSK_CD81";
const char* WIFI_PASSWORD = "GMQB4R03JK";

//=============================================================================
// ARDUINO CORE FUNCTIONS
//=============================================================================

/**
 * @brief System initialization
 * Configures hardware and initializes sensor system
 */
void setup() {
  // Initialize hybrid communication system (WiFi + Serial)
  communication_init(WIFI_SSID, WIFI_PASSWORD);
  
  Debug->println("ESP32-S3 Sensor System Starting...");
  Debug->println("Hybrid Communication: WiFi Primary, Serial Backup");
  
  // Initialize state machine (starts in SystemState::STARTUP)
  state_machine_init();
  
  // Begin STARTUP→INITIALIZING transition
  system_transition_to(SystemState::INITIALIZING);
  
  // Initialize NVS preferences
  preferences.begin(NVS_NAMESPACE);
  
  // Load calibration from NVS
  calibration_load();
  
  // Initialize sensor system
  if (sensor_initialize()) {
    Debug->println("Sensor system initialized successfully");
    Debug->println("pH | EC (mS/cm) | Volume (L)");
  } else {
    Debug->println("ERROR: Sensor initialization failed");
    system_transition_to(SystemState::ERROR);
    return;
  }
  
  // Initialize pump system
  if (pump_init()) {
    Debug->println("Pump system initialized successfully");
  } else {
    Debug->println("ERROR: Pump initialization failed");
    system_transition_to(SystemState::ERROR);
    return;
  }
  
  // Optional: initialize task wrappers (stubs when disabled)
  ph_task_init();
  ec_task_init();

  Debug->println("CLI Commands:");
  Debug->println("  Calibration: s=show cal, r=reset cal, p=pH cal, e=EC cal, v=volume cal");
  Debug->println("  Auto pH: a=auto pH, t=pH target, q=pump status, m=manual dose");
  Debug->println("  Manual Pumps: 1=pH_Up, 2=pH_Down, 3=Nut_A, 4=Nut_B");
  Debug->println("  State Machine: S=show all states, R=recover from error, M=maintenance mode");
  Debug->println("  Communication: C=comm status");
  Debug->println("  OTA Updates: O=toggle OTA, U=OTA status");
  Debug->println("  Emergency: x=stop all, z=stop specific pump");
  
  // Complete initialization - transition to monitoring
  system_transition_to(SystemState::MONITORING);
}

/**
 * @brief Main program loop
 * Non-blocking sensor reading and data output
 */
void loop() {
  // Update communication manager (handles WiFi state machine and client connections)
  Debug->update();
  
  // Update state machine (handles automatic transitions and timeouts)
  state_machine_update();
  
  // Only perform normal operations if system is in MONITORING or DOSING state
  if (state_manager.system_state == SystemState::MONITORING || state_manager.system_state == SystemState::DOSING) {
    // Check if it's time for a sensor reading
    if (sensor_update_needed()) {
      // Read all sensors with filtering applied
      sensor_readings_t readings = sensor_read_all();
      
      // Output data if reading is valid
      if (readings.valid) {
        sensor_print_readings(readings);
        
        // Automatic pH dosing if enabled (transitions to DOSING state)
        if (pump_is_auto_ph_enabled()) {
          system_transition_to(SystemState::DOSING);
          pump_ph_dose(readings.ph, readings.volume);
          system_transition_to(SystemState::MONITORING);
        }
      } else {
        // Sensor reading failed - increment error counter
        static uint8_t sensor_error_count = 0;
        sensor_error_count++;
        
        if (sensor_error_count > 3) {
          Debug->println("Multiple sensor failures detected - transitioning to ERROR state");
          system_transition_to(SystemState::ERROR);
          sensor_error_count = 0; // Reset counter
        }
      }
    }
    
    // Update pump operations (non-blocking)
    pump_update();
  }
  
  // CLI handling is always available (except during SHUTDOWN)
  if (state_manager.system_state != SystemState::SHUTDOWN && Debug->available()) {
    char cmd = Debug->read();
    switch (cmd) {
      case 's':
        calibration_print_status();
        break;
      case 'S':
        state_machine_print_status();
        break;
      case 'C':
        Debug->print_status();
        break;
      case 'O':
        if (Debug->is_ota_enabled()) {
          Debug->disable_ota();
        } else {
          Debug->enable_ota();
        }
        break;
      case 'U':
        if (Debug->is_ota_enabled()) {
          Debug->printf("OTA Status: %s", Debug->is_ota_in_progress() ? "Update in progress" : "Ready for updates");
          Debug->printf("OTA Hostname: %s | Port: %d", "ESP32-Hydroponic", 3232);
        } else {
          Debug->println("OTA Status: Disabled (WiFi required)");
        }
        break;
      case 'R':
        Debug->println("Manual recovery attempted");
        if (state_manager.system_state == SystemState::ERROR) {
          system_transition_to(SystemState::MONITORING);
          Debug->println("System recovered from ERROR state");
        } else {
          Debug->println("System not in ERROR state - no recovery needed");
        }
        break;
      case 'M':
        if (state_manager.system_state == SystemState::MAINTENANCE) {
          system_transition_to(SystemState::MONITORING);
          Debug->println("Maintenance mode OFF - system operational");
        } else {
          system_transition_to(SystemState::MAINTENANCE);
          Debug->println("Maintenance mode ON - pumps disabled");
        }
        break;
      case 'r':
        calibration_reset();
        calibration_save();
        Debug->println("Calibration reset to defaults and saved");
        break;
      case 'p':
        system_transition_to(SystemState::CALIBRATING);
        calibration_transition_to(CalibrationState::ACTIVE);
        calibration_interactive_ph();
        calibration_transition_to(CalibrationState::IDLE);
        system_transition_to(SystemState::MONITORING);
        break;
      case 'e':
        system_transition_to(SystemState::CALIBRATING);
        calibration_transition_to(CalibrationState::ACTIVE);
        calibration_interactive_ec();
        calibration_transition_to(CalibrationState::IDLE);
        system_transition_to(SystemState::MONITORING);
        break;
      case 'v':
        system_transition_to(SystemState::CALIBRATING);
        calibration_transition_to(CalibrationState::ACTIVE);
        calibration_interactive_volume();
        calibration_transition_to(CalibrationState::IDLE);
        system_transition_to(SystemState::MONITORING);
        break;
      case 'a':
        pump_enable_auto_ph(!pump_is_auto_ph_enabled());
        Debug->printf("Auto pH control: %s", pump_is_auto_ph_enabled() ? "ON" : "OFF");
        break;
      case 't': {
        Debug->println("Enter target pH (5.0-8.0): - Interactive mode simplified for Demo");
        // For now, cycle through common pH targets
        static float targets[] = {5.5, 6.0, 6.5, 7.0};
        static int target_idx = 2;
        target_idx = (target_idx + 1) % 4;
        float target = targets[target_idx];
        pump_set_ph_target(target);
        Debug->printf("pH target set to %.1f", target);
        break;
      }
      case 'q':
        pump_print_status();
        break;
      case 'm': {
        Debug->println("Manual dose: Simplified - 10ml pH_Up for demo");
        // Simplified manual dose for demo - always dose 10ml pH_Up
        if (pump_manual_dose(PumpId::PH_UP, 10.0)) {
          Debug->printf("Manual dose started: 10.0ml pH_Up");
        } else {
          Debug->println("Manual dose failed (safety limits or pump busy)");
        }
        break;
      }
      // Manual pump controls (Phase 2)
      case '1': {
        Debug->println("pH Up pump: Starting at 30 ml/min (demo)");
        float rate = 30.0; // Default rate for demo
        if (pump_start_manual(PumpId::PH_UP, rate)) {
          Debug->printf("pH Up started at %.1f ml/min", rate);
        } else {
          Debug->println("Failed to start pH Up pump (already running or error)");
        }
        break;
      }
      case '2': {
        Debug->println("pH Down pump: Starting at 25 ml/min (demo)");
        float rate = 25.0;
        if (pump_start_manual(PumpId::PH_DOWN, rate)) {
          Debug->printf("pH Down started at %.1f ml/min", rate);
        } else {
          Debug->println("Failed to start pH Down pump (already running or error)");
        }
        break;
      }
      case '3': {
        Debug->println("Nutrient A pump: Starting at 20 ml/min (demo)");
        float rate = 20.0;
        if (pump_start_manual(PumpId::NUTRIENT_A, rate)) {
          Debug->printf("Nutrient A started at %.1f ml/min", rate);
        } else {
          Debug->println("Failed to start Nutrient A pump (already running or error)");
        }
        break;
      }
      case '4': {
        Debug->println("Nutrient B pump: Starting at 20 ml/min (demo)");
        float rate = 20.0;
        if (pump_start_manual(PumpId::NUTRIENT_B, rate)) {
          Debug->printf("Nutrient B started at %.1f ml/min", rate);
        } else {
          Debug->println("Failed to start Nutrient B pump (already running or error)");
        }
        break;
      }
      case 'x':
        state_machine_emergency_stop();
        pump_stop_all();
        Debug->println("EMERGENCY STOP - All pumps stopped, system in ERROR state");
        break;
      case 'z': {
        Debug->println("Stop pump: Stopping all pumps (demo)");
        // For demo, stop all pumps
        pump_stop_all();
        Debug->println("All pumps stopped");
        break;
      }
      default:
        break;
    }
  }
}