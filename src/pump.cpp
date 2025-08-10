/**
 * @file pump.cpp
 * @brief Peristaltic pump control system implementation
 * @author Arduino Developer
 * @date 2025
 */
#include <Arduino.h>
#include <esp32-hal-ledc.h>  // Using LEDC API for Arduino-ESP32 3.x (ledcAttach/ledcWrite)
#include "pump.h"
#include "state_machine.h"

//=============================================================================
// GLOBAL VARIABLES
//=============================================================================

// Pump hardware and state array
static pump_t pumps[static_cast<int>(PumpId::COUNT)];

// Common constants
static constexpr uint32_t kHourMs = 60u * 60u * 1000u;
static const char* kPumpNames[static_cast<int>(PumpId::COUNT)] = {"pH_Up", "pH_Down", "Nut_A", "Nut_B"};

// Global system state
pump_system_t pump_system = {
    .auto_ph_control = false,
    .auto_ec_control = false,
    .initialized = false
};

//=============================================================================
// PRIVATE HELPER FUNCTIONS
//=============================================================================

/**
 * @brief Calculate PWM duty cycle from flow rate
 * @param flow_rate_ml_per_min Desired flow rate (5.2-90.0 ml/min)
 * @return PWM duty cycle (0-255)
 */
static uint8_t calculate_pwm_duty(float flow_rate_ml_per_min) {
    // Clamp flow rate to valid range
    flow_rate_ml_per_min = constrain(flow_rate_ml_per_min, 5.2f, PUMP_MAX_FLOW_RATE);
    
    // Linear mapping: 5.2ml/min = 10% PWM, 90ml/min = 100% PWM
    float duty_percent = 10.0f + (flow_rate_ml_per_min - 5.2f) * 90.0f / 84.8f;
    
    // Convert to 8-bit PWM value
    return (uint8_t)(duty_percent * 255.0f / 100.0f);
}

/**
 * @brief Check if pump can dose safely (timing, count limits, and state)
 * @param pump_id Pump identifier for state checking
 * @param pump Pointer to pump structure
 * @return true if dosing is allowed, false if blocked by safety limits
 */
static bool can_dose_safely(PumpId pump_id, pump_t* pump) {
    uint32_t  now = millis();
    
    // Check pump state - must be IDLE to start new dose
    PumpState current_state = state_manager.pump_states[static_cast<int>(pump_id)];
    if (current_state != PumpState::IDLE) {
        return false; // Pump not in correct state for dosing
    }
    
    // Check minimum interval between doses (5 minutes)
    if (now - pump->controller.last_dose_time < PUMP_MIN_DOSE_INTERVAL) {
        return false;
    }
    
    // Reset hourly counter if hour has passed
    if (now - pump->controller.hour_start >= kHourMs) {
        pump->controller.doses_this_hour = 0;
        pump->controller.hour_start = now;
    }
    
    // Check maximum doses per hour limit
    if (pump->controller.doses_this_hour >= PUMP_MAX_DOSES_PER_HOUR) {
        return false;
    }
    
    // Additional safety check: ensure system is in correct state for dosing
    if (state_manager.system_state != SystemState::MONITORING && 
        state_manager.system_state != SystemState::DOSING) {
        return false;
    }
    
    return true;
}

/**
 * @brief Calculate PID-based dose amount
 * @param pid Pointer to PID controller
 * @param current_value Current pH value
 * @param volume_liters Reservoir volume in liters
 * @return Dose amount in ml (always positive)
 */
static float calculate_pid_dose(pid_controller_t* pid, float current_value, float volume_liters) {
    float error = pid->target_value - current_value;
    
    // Update integral with windup protection
    pid->integral += error;
    pid->integral = constrain(pid->integral, PID_INTEGRAL_MIN, PID_INTEGRAL_MAX);
    
    // Calculate derivative
    float derivative = error - pid->last_error;
    pid->last_error = error;
    
    // PID calculation
    float output = pid->kp * error + pid->ki * pid->integral + pid->kd * derivative;
    
    // Scale by volume (normalize to 10L baseline)
    float dose_ml = abs(output) * (volume_liters / 10.0f);
    
    // Apply dose limits  
    return constrain(dose_ml, (float)PUMP_MIN_DOSE_VOLUME, PUMP_MAX_DOSE_VOLUME);
}

/**
 * @brief Start pump with specified dose parameters using state machine
 * @param pump_id Pump identifier
 * @param dose_ml Amount to dose in ml
 * @param flow_rate Flow rate in ml/min
 * @return true if pump started successfully
 */
static bool start_pump_dose(PumpId pump_id, float dose_ml, float flow_rate) {
    int pump_index = static_cast<int>(pump_id);
    if (pump_index >= static_cast<int>(PumpId::COUNT)) return false;
    
    pump_t* pump = &pumps[pump_index];
    
    // Check if pump is available for dosing (must be IDLE or COOLING_DOWN -> IDLE)
    PumpState current_state = state_manager.pump_states[pump_index];
    if (current_state != PumpState::IDLE) {
        return false; // Pump not available
    }
    
    // Calculate run duration
    pump->run_duration_ms = (dose_ml / flow_rate) * 60000; // Convert to milliseconds
    
    // Safety timeout check
    if (pump->run_duration_ms > PUMP_TIMEOUT_MS) {
        pump->run_duration_ms = PUMP_TIMEOUT_MS;
    }
    
    // Calculate and store PWM duty for dosing phase
    uint8_t pwm_duty = calculate_pwm_duty(flow_rate);
    
    // Transition to PRIMING state (PWM will be managed by pump_update)
    if (!pump_transition_to(pump_id, PumpState::PRIMING)) {
        return false; // State transition failed
    }
    
    // Store PWM duty for when we transition to DOSING
    pump->target_pwm_duty = pwm_duty;
    
    // Update pump timing
    pump->start_time = millis();
    
    // Update safety tracking
    pump->controller.last_dose_time = millis();
    pump->controller.doses_this_hour++;
    pump->controller.total_ml_dosed += dose_ml;
    
    return true;
}

//=============================================================================
// PUMP SAFETY FUNCTIONS
//=============================================================================

/**
 * @brief Check and enforce pump safety timeouts and limits
 * Called from state machine update to monitor all pumps
 */
void pump_safety_check(void) {
    if (!pump_system.initialized) return;
    
    
    for (int i = 0; i < static_cast<int>(PumpId::COUNT); i++) {
        PumpState current_state = state_manager.pump_states[i];
        uint32_t  state_duration = pump_get_state_duration_ms(static_cast<PumpId>(i));
        
    switch (current_state) {
            case PumpState::PRIMING:
                // Force transition if priming takes too long (safety: max 5 seconds)
                if (state_duration > 5000) {
                    pump_transition_to(static_cast<PumpId>(i), PumpState::ERROR);
            Serial.printf("[SAFETY] Pump %s priming timeout - forced to ERROR\n", kPumpNames[i]);
                }
                break;
                
            case PumpState::DOSING:
                // Force stop if dosing exceeds maximum timeout (10 minutes)
                if (state_duration > PUMP_TIMEOUT_MS) {
                    ledcWrite(pumps[i].pwm_channel, 0);
                    pumps[i].running = false;
                    pump_transition_to(static_cast<PumpId>(i), PumpState::ERROR);
            Serial.printf("[SAFETY] Pump %s dosing timeout (10min) - forced to ERROR\n", kPumpNames[i]);
                }
                break;
                
            case PumpState::COOLING_DOWN:
                // Automatic transition to IDLE after cooling period (handled by state machine)
                // This is just monitoring - state_machine_update handles the transition
                break;
                
            case PumpState::ERROR:
                // Allow manual recovery after 30 seconds in error state
                if (state_duration > 30000) {
                    pump_transition_to(static_cast<PumpId>(i), PumpState::IDLE);
                    Serial.printf("[SAFETY] Pump %s auto-recovery from ERROR to IDLE\n", kPumpNames[i]);
                }
                break;
                
            default:
                break;
        }
    }
}

//=============================================================================
// PUBLIC SYSTEM FUNCTIONS
//=============================================================================

/**
 * @brief Initialize pump system hardware and state
 * @return true if initialization successful
 */
bool pump_init(void) {
    // GPIO pin mapping for all pumps
    uint8_t gpio_pins[static_cast<int>(PumpId::COUNT)] = {
        PUMP_PH_UP_PIN, PUMP_PH_DOWN_PIN, PUMP_NUTRIENT_A_PIN, PUMP_NUTRIENT_B_PIN
    };

    // Initialize pump structures using constructors
    for (int i = 0; i < static_cast<int>(PumpId::COUNT); i++) {
        pumps[i] = pump_t(); // Default constructor handles most initialization
        pumps[i].gpio_pin = gpio_pins[i];
        // Attach LEDC to the pin with configured frequency/resolution and ensure off
        ledcAttach(pumps[i].gpio_pin, PUMP_PWM_FREQ, PUMP_PWM_RESOLUTION);
        ledcWrite(pumps[i].gpio_pin, 0);
    }

    pump_system.initialized = true;
    Serial.println("Pump system initialized successfully");

    return true;
}

/**
 * @brief Non-blocking pump update function with state machine integration
 * Call this regularly from main loop to handle pump state transitions
 */
void pump_update(void) {
    if (!pump_system.initialized) return;
    
    // no need to read now here; we rely on per-state durations from state machine
    
    for (int i = 0; i < static_cast<int>(PumpId::COUNT); i++) {
        pump_t* pump = &pumps[i];
        PumpState current_state = state_manager.pump_states[i];
        uint32_t  state_duration = pump_get_state_duration_ms(static_cast<PumpId>(i));
        
        switch (current_state) {
            case PumpState::IDLE:
                // Ensure PWM is off in idle state
                ledcWrite(pump->gpio_pin, 0);
                pump->running = false;
                break;
                
            case PumpState::PRIMING:
                // Priming phase: 2-3 seconds with lower PWM (20-30%)
                if (state_duration < 2500) { // 2.5 second priming
                    uint8_t priming_pwm = (uint8_t)(255 * 0.25f); // 25% PWM for priming
                    ledcWrite(pump->gpio_pin, priming_pwm);
                    pump->running = true;
                } else {
                    // Transition to dosing
                    pump_transition_to(static_cast<PumpId>(i), PumpState::DOSING);
                }
                break;
                
            case PumpState::DOSING:
                // Active dosing with target PWM
                if (state_duration < pump->run_duration_ms) {
                    // Set target PWM once at the start of DOSING
                    if (state_duration == 0) {
                        ledcWrite(pump->gpio_pin, pump->target_pwm_duty);
                    }
                    pump->running = true;
                } else {
                    // Dosing complete - stop and begin cooling down
                    ledcWrite(pump->gpio_pin, 0);
                    pump->running = false;
                    pump_transition_to(static_cast<PumpId>(i), PumpState::COOLING_DOWN);
                    Serial.printf("Pump %s completed dose after %.1fs\n", 
                                 kPumpNames[i], pump->run_duration_ms / 1000.0f);
                }
                break;
                
            case PumpState::COOLING_DOWN:
                // Ensure pump stays off during cooling down (state machine handles timeout)
                ledcWrite(pump->gpio_pin, 0);
                pump->running = false;
                break;
                
            case PumpState::ERROR:
                // Error state - ensure pump is off
                ledcWrite(pump->gpio_pin, 0);
                pump->running = false;
                break;
                
            case PumpState::MAINTENANCE:
                // Maintenance mode - pump controlled manually, no automatic operation
                // PWM can be controlled externally in this mode
                break;
        }
    }
}

/**
 * @brief Emergency stop all pumps using state machine
 */
void pump_stop_all(void) {
    for (int i = 0; i < static_cast<int>(PumpId::COUNT); i++) {
        // Immediately stop PWM
    ledcWrite(pumps[i].gpio_pin, 0);
        pumps[i].running = false;
        
        // Transition to IDLE state (emergency transitions always allowed)
        pump_transition_to(static_cast<PumpId>(i), PumpState::IDLE);
    }
    Serial.println("All pumps stopped (emergency) - transitioned to IDLE state");
}

//=============================================================================
// PH CONTROL FUNCTIONS
//=============================================================================

/**
 * @brief Perform automatic pH dosing using PID control
 * @param current_ph Current pH reading
 * @param volume_liters Reservoir volume in liters
 * @return true if dosing was performed
 */
bool pump_ph_dose(float current_ph, float volume_liters) {
    if (!pump_system.initialized || !pump_system.auto_ph_control) {
        return false;
    }
    
    // Validate inputs
    if (volume_liters < 5.0f || volume_liters > 200.0f) {
        return false; // Volume out of safe range
    }
    
    if (current_ph < 4.0f || current_ph > 9.0f) {
        return false; // pH reading invalid
    }
    
    // Determine which pump to use
    PumpId pump_id = (current_ph > pumps[static_cast<int>(PumpId::PH_UP)].controller.target_value) ? 
                        PumpId::PH_DOWN : PumpId::PH_UP;
    
    pump_t* pump = &pumps[static_cast<int>(pump_id)];
    
    // Check safety limits
    if (!can_dose_safely(pump_id, pump)) {
        return false;
    }
    
    // Calculate dose using PID
    float dose_ml = calculate_pid_dose(&pump->controller, current_ph, volume_liters);
    
    // Skip if dose is too small (within acceptable range)
    if (dose_ml < (float)PUMP_MIN_DOSE_VOLUME) {
        return false;
    }
    
    // Start dosing
    bool success = start_pump_dose(pump_id, dose_ml, PUMP_DEFAULT_FLOW_RATE);
    
    if (success) {
        Serial.printf("pH dosing: %.1fml %s (pH %.2f → %.2f, Vol: %.1fL)\n",
                     dose_ml, 
                     (pump_id == PumpId::PH_UP) ? "pH_Up" : "pH_Down",
                     current_ph, pump->controller.target_value, volume_liters);
    }
    
    return success;
}

/**
 * @brief Set pH target value
 * @param target_ph Target pH (5.0-8.0)
 */
void pump_set_ph_target(float target_ph) {
    target_ph = constrain(target_ph, 5.0f, 8.0f);
    
    // Set target for both pumps (shared PID config)
    for (int i = 0; i < static_cast<int>(PumpId::COUNT); i++) {
        pumps[i].controller.target_value = target_ph;
        pumps[i].controller.integral = 0.0f; // Reset integral on target change
        pumps[i].controller.last_error = 0.0f;
    }
}

/**
 * @brief Get current pH target
 * @return Current pH target value
 */
float pump_get_ph_target(void) {
    return pumps[static_cast<int>(PumpId::PH_UP)].controller.target_value;
}

/**
 * @brief Manual dose override (bypasses PID, uses safety limits)
 * @param pump Pump identifier
 * @param ml Amount to dose in ml
 * @return true if dosing started
 */
bool pump_manual_dose(PumpId pump, float ml) {
    int pump_index = static_cast<int>(pump);
    if (pump_index >= static_cast<int>(PumpId::COUNT) || !pump_system.initialized) {
        return false;
    }
    
    // Clamp dose to safety limits
    ml = constrain(ml, (float)PUMP_MIN_DOSE_VOLUME, PUMP_MAX_DOSE_VOLUME);
    
    // Check safety limits
    if (!can_dose_safely(pump, &pumps[pump_index])) {
        Serial.println("Manual dose blocked by safety limits");
        return false;
    }
    
    bool success = start_pump_dose(pump, ml, PUMP_DEFAULT_FLOW_RATE);
    
    if (success) {
        Serial.printf("Manual dose: %.1fml %s\n", ml, 
                     (pump == PumpId::PH_UP) ? "pH_Up" : (pump == PumpId::PH_DOWN ? "pH_Down" : "Pump"));
    }
    
    return success;
}

//=============================================================================
// EC CONTROL FUNCTIONS (Phase 2 placeholders)
//=============================================================================

/**
 * @brief EC dosing function (not implemented in Phase 1)
 * @return false (not implemented)
 */
bool pump_ec_dose(float current_ec, float volume_liters) {
    // Phase 2 implementation
    return false;
}

/**
 * @brief Set EC target (no-op in Phase 1)
 */
void pump_set_ec_target(float target_ec) {
    // Phase 2 implementation
}

/**
 * @brief Get EC target (returns 0.0 in Phase 1)
 */
float pump_get_ec_target(void) {
    return 0.0f; // Phase 2 implementation
}

//=============================================================================
// PID TUNING FUNCTIONS
//=============================================================================

/**
 * @brief Set pH PID parameters
 * @param kp Proportional gain
 * @param ki Integral gain
 * @param kd Derivative gain
 */
void pump_set_ph_pid(float kp, float ki, float kd) {
    for (int i = 0; i < static_cast<int>(PumpId::COUNT); i++) {
        pumps[i].controller.kp = constrain(kp, 0.1f, 50.0f);
        pumps[i].controller.ki = constrain(ki, 0.0f, 5.0f);
        pumps[i].controller.kd = constrain(kd, 0.0f, 10.0f);
        pumps[i].controller.integral = 0.0f; // Reset integral on PID change
    }
}

/**
 * @brief Get current pH PID parameters
 * @param kp Pointer to store Kp value
 * @param ki Pointer to store Ki value
 * @param kd Pointer to store Kd value
 */
void pump_get_ph_pid(float* kp, float* ki, float* kd) {
    *kp = pumps[static_cast<int>(PumpId::PH_UP)].controller.kp;
    *ki = pumps[static_cast<int>(PumpId::PH_UP)].controller.ki;
    *kd = pumps[static_cast<int>(PumpId::PH_UP)].controller.kd;
}

//=============================================================================
// STATUS AND UTILITY FUNCTIONS
//=============================================================================

/**
 * @brief Print detailed pump system status
 */
void pump_print_status(void) {
    if (!pump_system.initialized) {
        Serial.println("Pump system not initialized");
        return;
    }
    
    Serial.println("=== PUMP SYSTEM STATUS ===");
    Serial.printf("Auto pH Control: %s\n", pump_system.auto_ph_control ? "ON" : "OFF");
    Serial.printf("pH Target: %.1f\n", pump_get_ph_target());
    
    float kp, ki, kd;
    pump_get_ph_pid(&kp, &ki, &kd);
    Serial.printf("pH PID: Kp=%.1f, Ki=%.2f, Kd=%.1f\n", kp, ki, kd);
    
    for (int i = 0; i < static_cast<int>(PumpId::COUNT); i++) {
        pump_t* pump = &pumps[i];
    const char* name = kPumpNames[i];
        
        Serial.printf("%s: ", name);
        
        // Display pump state and timing information
        PumpState current_state = state_manager.pump_states[i];
        uint32_t  state_duration = pump_get_state_duration_ms(static_cast<PumpId>(i));
        
        Serial.printf("State: %s", pump_state_to_string(current_state));
        
        if (current_state == PumpState::DOSING) {
            uint32_t  remaining = pump->run_duration_ms - state_duration;
            Serial.printf(" (%.1fs remaining)", remaining / 1000.0f);
        } else if (current_state == PumpState::COOLING_DOWN) {
            uint32_t  remaining = 300000 - state_duration; // 5 minutes cooling
            Serial.printf(" (%.1fs remaining)", remaining / 1000.0f);
        } else if (current_state == PumpState::PRIMING) {
            Serial.printf(" (%.1fs)", state_duration / 1000.0f);
        } else if (state_duration > 0) {
            Serial.printf(" (%.1fs in state)", state_duration / 1000.0f);
        }
        
        Serial.printf(" | Doses: %d/3 this hour", pump->controller.doses_this_hour);
        Serial.printf(" | Total: %.1fml", pump->controller.total_ml_dosed);
        
        // Next dose availability
        uint32_t  time_since_last = millis() - pump->controller.last_dose_time;
        if (time_since_last < PUMP_MIN_DOSE_INTERVAL) {
            uint32_t  wait_time = (PUMP_MIN_DOSE_INTERVAL - time_since_last) / 1000;
            Serial.printf(" | Next dose in: %lum %lus", wait_time / 60, wait_time % 60);
        }
        
        Serial.println();
    }
    Serial.println("========================");
}

/**
 * @brief Check if pump is currently running
 * @param pump Pump identifier
 * @return true if pump is running
 */
bool pump_is_running(PumpId pump) {
    int pump_index = static_cast<int>(pump);
    if (pump_index >= static_cast<int>(PumpId::COUNT)) return false;
    return pumps[pump_index].running;
}

/**
 * @brief Reset dose counters (for testing/maintenance)
 */
void pump_reset_counters(void) {
    for (int i = 0; i < static_cast<int>(PumpId::COUNT); i++) {
        pumps[i].controller.doses_this_hour = 0;
        pumps[i].controller.hour_start = millis();
        pumps[i].controller.integral = 0.0f;
        pumps[i].controller.last_error = 0.0f;
    }
    Serial.println("Pump dose counters reset");
}

/**
 * @brief Get total amount dosed by specific pump
 * @param pump Pump identifier
 * @return Total ml dosed (lifetime)
 */
float pump_get_total_dosed(PumpId pump) {
    int pump_index = static_cast<int>(pump);
    if (pump_index >= static_cast<int>(PumpId::COUNT)) return 0.0f;
    return pumps[pump_index].controller.total_ml_dosed;
}

//=============================================================================
// AUTO CONTROL FUNCTIONS
//=============================================================================

/**
 * @brief Enable or disable automatic pH control
 * @param enabled true to enable, false to disable
 */
void pump_enable_auto_ph(bool enabled) {
    pump_system.auto_ph_control = enabled;
    
    // Reset PID state when enabling
    if (enabled) {
        for (int i = 0; i < static_cast<int>(PumpId::COUNT); i++) {
            pumps[i].controller.integral = 0.0f;
            pumps[i].controller.last_error = 0.0f;
        }
    }
}

/**
 * @brief Check if automatic pH control is enabled
 * @return true if auto pH control is enabled
 */
bool pump_is_auto_ph_enabled(void) {
    return pump_system.auto_ph_control;
}

//=============================================================================
// MANUAL CONTROL FUNCTIONS (Phase 2)
//=============================================================================

/**
 * @brief Start pump at specified flow rate (manual control)
 * @param pump Pump identifier
 * @param ml_per_min Flow rate (10.0-90.0 ml/min)
 * @return true if pump started successfully
 */
bool pump_start_manual(PumpId pump, float ml_per_min) {
    int pump_index = static_cast<int>(pump);
    // Validate pump ID
    if (pump_index >= static_cast<int>(PumpId::COUNT) || !pump_system.initialized) {
        return false;
    }
    
    // Check if pump is available (must be IDLE)
    PumpState current_state = state_manager.pump_states[pump_index];
    if (current_state != PumpState::IDLE) {
        return false; // Pump not available
    }
    
    // Clamp flow rate to valid range
    ml_per_min = constrain(ml_per_min, PUMP_MIN_FLOW_RATE, PUMP_MAX_FLOW_RATE);
    
    // Calculate and store PWM duty cycle for manual operation
    uint8_t pwm_duty = calculate_pwm_duty(ml_per_min);
    pumps[pump_index].target_pwm_duty = pwm_duty;
    
    // For manual operation, skip PRIMING and go directly to DOSING
    if (!pump_transition_to(pump, PumpState::DOSING)) {
        return false;
    }
    
    // Set PWM immediately for manual operation
    ledcWrite(pumps[pump_index].gpio_pin, pwm_duty);
    
    // Update pump timing and state
    pumps[pump_index].running = true;
    pumps[pump_index].start_time = millis();
    pumps[pump_index].run_duration_ms = PUMP_TIMEOUT_MS; // 10 minute safety timeout
    
    return true;
}

/**
 * @brief Stop pump (manual control)
 * @param pump Pump identifier
 * @return true if pump stopped successfully
 */
bool pump_stop_manual(PumpId pump) {
    int pump_index = static_cast<int>(pump);
    // Validate pump ID
    if (pump_index >= static_cast<int>(PumpId::COUNT)) {
        return false;
    }
    
    // Stop PWM output immediately
    ledcWrite(pumps[pump_index].gpio_pin, 0);
    
    // Update pump state
    pumps[pump_index].running = false;
    pumps[pump_index].start_time = 0;
    pumps[pump_index].run_duration_ms = 0;
    
    // Transition pump to COOLING_DOWN if it was running, otherwise IDLE
    PumpState current_state = state_manager.pump_states[pump_index];
    if (current_state == PumpState::DOSING || current_state == PumpState::PRIMING) {
        pump_transition_to(pump, PumpState::COOLING_DOWN);
    } else {
        pump_transition_to(pump, PumpState::IDLE);
    }
    
    return true;
}