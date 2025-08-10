/**
 * @file state_machine.cpp
 * @brief Simple Enum-Based State Machine Implementation for ESP32-S3 Hydroponic System
 * @author Arduino Developer
 * @date 2025
 */

#include "state_machine.h"
#include "pump.h"

//=============================================================================
// GLOBAL STATE MANAGER INSTANCE
//=============================================================================

state_manager_t state_manager; // Default constructor handles proper initialization

//=============================================================================
// PRIVATE HELPER FUNCTIONS
//=============================================================================

/**
 * @brief Log state transition for debugging
 */
static void log_transition(const char* subsystem, const char* from_state, const char* to_state) {
    if (state_manager.debug_logging_enabled) {
        Serial.printf("[STATE] %s: %s -> %s\n", subsystem, from_state, to_state);
    }
}

//=============================================================================
// STATE MACHINE INITIALIZATION
//=============================================================================

bool state_machine_init(void) {
    uint32_t now = millis();
    
    // Initialize all timing
    state_manager.system_state_entry_time = now;
    state_manager.sensor_state_entry_time = now;
    state_manager.calibration_state_entry_time = now;
    
    for (int i = 0; i < 4; i++) {
        state_manager.pump_state_entry_times[i] = now;
    }
    
    // Enable debug logging by default during initialization
    state_manager.debug_logging_enabled = true;
    
    Serial.println("[STATE] State machine initialized");
    return true;
}

//=============================================================================
// SYSTEM STATE FUNCTIONS
//=============================================================================

bool system_transition_to(SystemState new_state) {
    SystemState old_state = state_manager.system_state;
    
    // Validate transition
    if (!is_valid_system_transition(old_state, new_state)) {
        if (state_manager.debug_logging_enabled) {
            Serial.printf("[STATE ERROR] Invalid system transition: %s -> %s\n", 
                         system_state_to_string(old_state), system_state_to_string(new_state));
        }
        return false;
    }
    
    // Perform transition
    state_manager.system_state = new_state;
    state_manager.system_state_entry_time = millis();
    
    log_transition("SYSTEM", system_state_to_string(old_state), system_state_to_string(new_state));
    return true;
}

const char* system_state_to_string(SystemState state) {
    switch (state) {
        case SystemState::STARTUP:        return "STARTUP";
        case SystemState::INITIALIZING:   return "INITIALIZING";
        case SystemState::MONITORING:     return "MONITORING";
        case SystemState::DOSING:         return "DOSING";
        case SystemState::CALIBRATING:    return "CALIBRATING";
        case SystemState::ERROR:          return "ERROR";
        case SystemState::MAINTENANCE:    return "MAINTENANCE";
        case SystemState::SHUTDOWN:       return "SHUTDOWN";
        default:                    return "UNKNOWN";
    }
}

uint32_t  system_get_state_duration_ms(void) {
    return millis() - state_manager.system_state_entry_time;
}

//=============================================================================
// PUMP STATE FUNCTIONS
//=============================================================================

bool pump_transition_to(PumpId pump_id, PumpState new_state) {
    int pump_index = static_cast<int>(pump_id);
    if (pump_index >= 4) return false;
    
    PumpState old_state = state_manager.pump_states[pump_index];
    
    // Validate transition
    if (!is_valid_pump_transition(old_state, new_state)) {
        if (state_manager.debug_logging_enabled) {
            Serial.printf("[STATE ERROR] Invalid pump %d transition: %s -> %s\n", 
                         pump_id, pump_state_to_string(old_state), pump_state_to_string(new_state));
        }
        return false;
    }
    
    // Perform transition
    state_manager.pump_states[pump_index] = new_state;
    state_manager.pump_state_entry_times[pump_index] = millis();
    
    char subsystem[16];
    snprintf(subsystem, sizeof(subsystem), "PUMP_%d", pump_index);
    log_transition(subsystem, pump_state_to_string(old_state), pump_state_to_string(new_state));
    
    return true;
}

const char* pump_state_to_string(PumpState state) {
    switch (state) {
        case PumpState::IDLE:         return "IDLE";
        case PumpState::PRIMING:      return "PRIMING";
        case PumpState::DOSING:       return "DOSING";
        case PumpState::COOLING_DOWN: return "COOLING_DOWN";
        case PumpState::ERROR:        return "ERROR";
        case PumpState::MAINTENANCE:  return "MAINTENANCE";
        default:                return "UNKNOWN";
    }
}

uint32_t pump_get_state_duration_ms(PumpId pump_id) {
    int pump_index = static_cast<int>(pump_id);
    if (pump_index >= 4) return 0;
    return millis() - state_manager.pump_state_entry_times[pump_index];
}

//=============================================================================
// SENSOR STATE FUNCTIONS
//=============================================================================

bool sensor_transition_to(SensorState new_state) {
    SensorState old_state = state_manager.sensor_state;
    
    // Validate transition
    if (!is_valid_sensor_transition(old_state, new_state)) {
        if (state_manager.debug_logging_enabled) {
            Serial.printf("[STATE ERROR] Invalid sensor transition: %s -> %s\n", 
                         sensor_state_to_string(old_state), sensor_state_to_string(new_state));
        }
        return false;
    }
    
    // Perform transition
    state_manager.sensor_state = new_state;
    state_manager.sensor_state_entry_time = millis();
    
    log_transition("SENSOR", sensor_state_to_string(old_state), sensor_state_to_string(new_state));
    return true;
}

const char* sensor_state_to_string(SensorState state) {
    switch (state) {
        case SensorState::INITIALIZING:   return "INITIALIZING";
        case SensorState::WARMING_UP:     return "WARMING_UP";
        case SensorState::READING:        return "READING";
        case SensorState::FILTERING:      return "FILTERING";
        case SensorState::READY:          return "READY";
        case SensorState::ERROR:          return "ERROR";
        default:                    return "UNKNOWN";
    }
}

uint32_t sensor_get_state_duration_ms(void) {
    return millis() - state_manager.sensor_state_entry_time;
}

//=============================================================================
// CALIBRATION STATE FUNCTIONS
//=============================================================================

bool calibration_transition_to(CalibrationState new_state) {
    CalibrationState old_state = state_manager.calibration_state;
    
    // Perform transition (calibration states are simple, no validation needed)
    state_manager.calibration_state = new_state;
    state_manager.calibration_state_entry_time = millis();
    
    log_transition("CALIBRATION", calibration_state_to_string(old_state), calibration_state_to_string(new_state));
    return true;
}

const char* calibration_state_to_string(CalibrationState state) {
    switch (state) {
        case CalibrationState::IDLE:      return "IDLE";
        case CalibrationState::ACTIVE:    return "ACTIVE";
        default:            return "UNKNOWN";
    }
}

//=============================================================================
// STATE VALIDATION FUNCTIONS
//=============================================================================

bool is_valid_system_transition(SystemState from, SystemState to) {
    // Emergency transitions (ERROR, MAINTENANCE, SHUTDOWN) allowed from any state
    if (to == SystemState::ERROR || to == SystemState::MAINTENANCE || to == SystemState::SHUTDOWN) {
        return true;
    }
    
    switch (from) {
        case SystemState::STARTUP:
            return (to == SystemState::INITIALIZING);
            
        case SystemState::INITIALIZING:
            return (to == SystemState::MONITORING);
            
        case SystemState::MONITORING:
            return (to == SystemState::DOSING || to == SystemState::CALIBRATING);
            
        case SystemState::DOSING:
            return (to == SystemState::MONITORING);
            
        case SystemState::CALIBRATING:
            return (to == SystemState::MONITORING);
            
        case SystemState::ERROR:
            return (to == SystemState::MONITORING || to == SystemState::INITIALIZING);
            
        case SystemState::MAINTENANCE:
            return (to == SystemState::MONITORING);
            
        case SystemState::SHUTDOWN:
            return (to == SystemState::STARTUP); // Allow restart
            
        default:
            return false;
    }
}

bool is_valid_pump_transition(PumpState from, PumpState to) {
    // Emergency transitions (IDLE, ERROR, MAINTENANCE) allowed from any state
    if (to == PumpState::IDLE || to == PumpState::ERROR || to == PumpState::MAINTENANCE) {
        return true;
    }
    
    switch (from) {
        case PumpState::IDLE:
            return (to == PumpState::PRIMING);
            
        case PumpState::PRIMING:
            return (to == PumpState::DOSING);
            
        case PumpState::DOSING:
            return (to == PumpState::COOLING_DOWN);
            
        case PumpState::COOLING_DOWN:
            return (to == PumpState::IDLE);
            
        case PumpState::ERROR:
            return (to == PumpState::IDLE);
            
        case PumpState::MAINTENANCE:
            return (to == PumpState::IDLE);
            
        default:
            return false;
    }
}

bool is_valid_sensor_transition(SensorState from, SensorState to) {
    // ERROR transition allowed from any state
    if (to == SensorState::ERROR) {
        return true;
    }
    
    switch (from) {
        case SensorState::INITIALIZING:
            return (to == SensorState::READY);
            
        case SensorState::WARMING_UP:
            return (to == SensorState::READING);
            
        case SensorState::READING:
            return (to == SensorState::FILTERING);
            
        case SensorState::FILTERING:
            return (to == SensorState::READY);
            
        case SensorState::READY:
            return (to == SensorState::WARMING_UP);
            
        case SensorState::ERROR:
            return (to == SensorState::INITIALIZING || to == SensorState::READY);
            
        default:
            return false;
    }
}

//=============================================================================
// EMERGENCY AND UTILITY FUNCTIONS
//=============================================================================

void state_machine_emergency_stop(void) {
    // Emergency stop: transition all systems to safe states immediately
    state_manager.system_state = SystemState::ERROR;
    state_manager.system_state_entry_time = millis();
    
    // Stop all pumps immediately
    for (int i = 0; i < 4; i++) {
        state_manager.pump_states[i] = PumpState::IDLE;
        state_manager.pump_state_entry_times[i] = millis();
    }
    
    // Reset sensor to safe state
    state_manager.sensor_state = SensorState::READY;
    state_manager.sensor_state_entry_time = millis();
    
    Serial.println("[STATE EMERGENCY] All systems stopped - EMERGENCY MODE");
}

void state_machine_print_status(void) {
    Serial.println("=== STATE MACHINE STATUS ===");
    Serial.printf("System: %s (%lu ms)\n", 
                  system_state_to_string(state_manager.system_state), 
                  system_get_state_duration_ms());
    
    Serial.printf("Sensor: %s (%lu ms)\n", 
                  sensor_state_to_string(state_manager.sensor_state), 
                  sensor_get_state_duration_ms());
    
    Serial.printf("Calibration: %s\n", 
                  calibration_state_to_string(state_manager.calibration_state));
    
    const char* pump_names[] = {"pH_Up", "pH_Down", "Nut_A", "Nut_B"};
    for (int i = 0; i < 4; i++) {
        Serial.printf("Pump %s: %s (%lu ms)\n", 
                      pump_names[i],
                      pump_state_to_string(state_manager.pump_states[i]), 
                      pump_get_state_duration_ms(static_cast<PumpId>(i)));
    }
    
    Serial.printf("Debug logging: %s\n", state_manager.debug_logging_enabled ? "ON" : "OFF");
    Serial.println("============================");
}

void state_machine_enable_debug(bool enabled) {
    state_manager.debug_logging_enabled = enabled;
    Serial.printf("[STATE] Debug logging %s\n", enabled ? "ENABLED" : "DISABLED");
}

//=============================================================================
// STATE MACHINE UPDATE FUNCTION
//=============================================================================

void state_machine_update(void) {
    uint32_t now = millis();
    
    // Handle ERROR state recovery attempts
    if (state_manager.system_state == SystemState::ERROR) {
        // Allow manual recovery after 5 seconds in ERROR state
        if (system_get_state_duration_ms() > 5000) {
            if (state_manager.debug_logging_enabled) {
                Serial.println("[STATE] ERROR state timeout - attempting recovery to MONITORING");
            }
            system_transition_to(SystemState::MONITORING);
        }
    }
    
    // Handle MAINTENANCE state (pumps disabled)
    if (state_manager.system_state == SystemState::MAINTENANCE) {
        // Ensure all pumps are in MAINTENANCE state
        for (int i = 0; i < 4; i++) {
            if (state_manager.pump_states[i] != PumpState::MAINTENANCE && 
                state_manager.pump_states[i] != PumpState::IDLE) {
                pump_transition_to(static_cast<PumpId>(i), PumpState::MAINTENANCE);
            }
        }
    }
    
    // Automatic pump cooling down timeout handling
    for (int i = 0; i < 4; i++) {
        if (state_manager.pump_states[i] == PumpState::COOLING_DOWN) {
            // 5-minute cooling down period (300000ms)
            if (pump_get_state_duration_ms(static_cast<PumpId>(i)) > 300000) {
                pump_transition_to(static_cast<PumpId>(i), PumpState::IDLE);
                if (state_manager.debug_logging_enabled) {
                    Serial.printf("[STATE] Pump %d cooling down complete - transition to IDLE\n", i);
                }
            }
        }
        
        // Pump dosing timeout safety (10 minutes maximum)
        if (state_manager.pump_states[i] == PumpState::DOSING) {
            if (pump_get_state_duration_ms(static_cast<PumpId>(i)) > 600000) { // 10 minutes
                pump_transition_to(static_cast<PumpId>(i), PumpState::ERROR);
                Serial.printf("[STATE ERROR] Pump %d dosing timeout - forced to ERROR state\n", i);
            }
        }
    }
    
    // Automatic sensor state machine management
    SensorState sensor_current_state = state_manager.sensor_state;
    uint32_t sensor_state_duration = sensor_get_state_duration_ms();
    
    // Handle sensor error recovery
    if (sensor_current_state == SensorState::ERROR) {
        // Auto-recover from sensor error after 10 seconds
        if (sensor_state_duration > 10000) {
            sensor_transition_to(SensorState::READY);
        }
    }
    
    // Handle sensor timeout in WARMING_UP state
    if (sensor_current_state == SensorState::WARMING_UP && sensor_state_duration > 5000) {
        // Force transition if warming up takes too long (safety: max 5 seconds)
        sensor_transition_to(SensorState::ERROR);
        Serial.println("[SENSOR SAFETY] Warmup timeout - forced to ERROR");
    }
    
    // Call pump safety check to monitor timeouts and enforce limits
    pump_safety_check();
    
    // Debug status printing
    if (state_manager.debug_logging_enabled) {
        static uint32_t last_status_print = 0;
        
        // Print status every 30 seconds when debug is enabled
        if (now - last_status_print > 30000) {
            state_machine_print_status();
            last_status_print = now;
        }
    }
}