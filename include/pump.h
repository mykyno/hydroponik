/**
 * @file pump.h
 * @brief Peristaltic pump control system for hydroponic pH and EC adjustment
 * @author Arduino Developer
 * @date 2025
 * 
 * This header provides interface for:
 * - PID-controlled pH adjustment using peristaltic pumps
 * - Volume-proportional dosing based on reservoir size
 * - Safety mechanisms (dose limits, timing restrictions)
 * - Foundation for future EC control expansion
 */

#ifndef PUMP_H
#define PUMP_H

#include <Arduino.h>

//=============================================================================
// HARDWARE CONFIGURATION
//=============================================================================

// GPIO pin assignments for pump control
constexpr int PUMP_PH_UP_PIN = 11;       // pH Up pump (PWM Channel 0)
constexpr int PUMP_PH_DOWN_PIN = 12;     // pH Down pump (PWM Channel 1)
//CLAUDE: implement phase 2 but without automation, just using serial run command to run 
constexpr int PUMP_NUTRIENT_A_PIN = 13;  // Nutrient A pump (PWM Channel 2)
constexpr int PUMP_NUTRIENT_B_PIN = 14;  // Nutrient B pump (PWM Channel 3)

// PWM configuration
constexpr int PUMP_PWM_FREQ = 1000;      // PWM frequency (Hz)
constexpr int PUMP_PWM_RESOLUTION = 8;   // PWM resolution (bits)

// Pump specifications
constexpr float PUMP_MIN_FLOW_RATE = 10.0f;   // Minimum practical flow rate (ml/min)
constexpr float PUMP_MAX_FLOW_RATE = 90.0f;   // Maximum flow rate (ml/min)
constexpr float PUMP_DEFAULT_FLOW_RATE = 30.0f; // Standard dosing rate (ml/min)

//=============================================================================
// SAFETY CONFIGURATION
//=============================================================================

constexpr int PUMP_MAX_DOSES_PER_HOUR = 3;       // Maximum doses per hour per pump
constexpr uint32_t PUMP_MIN_DOSE_INTERVAL = 300000;   // 5 minutes between doses (ms)
constexpr float PUMP_MIN_DOSE_VOLUME = 5.0f;       // Minimum dose volume (ml)
constexpr float PUMP_MAX_DOSE_VOLUME = 25.0f;      // Maximum single dose volume (ml)
constexpr uint32_t PUMP_TIMEOUT_MS = 600000;          // 10 minute maximum run time (safety)

//=============================================================================
// PID CONFIGURATION
//=============================================================================

// Default PID parameters for pH control
constexpr float DEFAULT_PH_KP = 8.0f;      // Proportional gain (ml per pH unit error)
constexpr float DEFAULT_PH_KI = 0.5f;      // Integral gain
constexpr float DEFAULT_PH_KD = 2.0f;      // Derivative gain
constexpr float DEFAULT_PH_TARGET = 6.0f;  // Default target pH

// PID limits
constexpr float PID_INTEGRAL_MAX = 50.0f;  // Maximum integral accumulation
constexpr float PID_INTEGRAL_MIN = -50.0f; // Minimum integral accumulation

//=============================================================================
// DATA STRUC// DATA STRUC researcTURES

//=============================================================================

/**
 * @brief Pump identification enumeration
 */
enum class PumpId {
    PH_UP = 0,         // pH Up pump (raises pH)
    PH_DOWN,           // pH Down pump (lowers pH)
    NUTRIENT_A,        // Nutrient A pump (EC control)
    NUTRIENT_B,        // Nutrient B pump (EC control)
    COUNT = 4          // Total pump count for Phase 2
};

/**
 * @brief PID controller state
 * Maintains PID calculation state and safety tracking
 */
struct pid_controller_t {
    float kp, ki, kd;               // PID coefficients
    float target_value;             // Target pH value
    float integral;                 // Accumulated error (integral term)
    float last_error;               // Previous error (for derivative)
    uint32_t last_dose_time;   // Timestamp of last dose (safety)
    uint8_t doses_this_hour;        // Doses in current hour
    uint32_t hour_start;       // Start of current hour tracking
    float total_ml_dosed;           // Lifetime total ml dosed
    
    // Default constructor with pH defaults
    pid_controller_t() : kp(DEFAULT_PH_KP), ki(DEFAULT_PH_KI), kd(DEFAULT_PH_KD), 
                        target_value(DEFAULT_PH_TARGET), integral(0.0f), last_error(0.0f),
                        last_dose_time(0), doses_this_hour(0), hour_start(millis()), 
                        total_ml_dosed(0.0f) {}
};

/**
 * @brief Pump hardware and control state
 * Contains all pump-specific information and state
 */
struct pump_t {
    uint8_t gpio_pin;               // GPIO pin number
    uint8_t pwm_channel;            // PWM channel number
    pid_controller_t controller;    // PID controller state
    bool running;                   // Pump currently running
    uint32_t start_time;           // Pump start timestamp
    uint32_t run_duration_ms;      // Planned run duration
    uint8_t target_pwm_duty;        // Target PWM duty for dosing phase
    
    // Default constructor
    pump_t() : gpio_pin(0), pwm_channel(0), controller(), running(false), 
               start_time(0), run_duration_ms(0), target_pwm_duty(0) {}
};

/**
 * @brief System state structure
 */
typedef struct {
    bool auto_ph_control;           // Automatic pH control enabled
    bool auto_ec_control;           // Automatic EC control enabled (Phase 2)
    bool initialized;               // System initialization status
} pump_system_t;

//=============================================================================
// GLOBAL VARIABLES (EXTERN DECLARATIONS)
//=============================================================================

// Global system state (defined in pump.cpp)
extern pump_system_t pump_system;

//=============================================================================
// FUNCTION DECLARATIONS
//=============================================================================

// System management functions
bool pump_init(void);                           // Initialize pump system
void pump_update(void);                         // Non-blocking pump update
void pump_stop_all(void);                       // Emergency stop all pumps
void pump_safety_check(void);                   // Check and enforce safety timeouts

// Manual control functions (Phase 2)
bool pump_start_manual(PumpId pump, float ml_per_min); // Start pump at flow rate
bool pump_stop_manual(PumpId pump);                    // Stop specific pump

// pH control functions (Phase 1 implementation)
bool pump_ph_dose(float current_ph, float volume_liters);    // Automatic pH dosing
void pump_set_ph_target(float target_ph);                    // Set pH target
float pump_get_ph_target(void);                              // Get pH target
bool pump_manual_dose(PumpId pump, float ml);             // Manual dose override

// EC control functions (Phase 2 foundation - not yet implemented)
bool pump_ec_dose(float current_ec, float volume_liters);    // Returns false in Phase 1
void pump_set_ec_target(float target_ec);                    // No-op in Phase 1
float pump_get_ec_target(void);                              // Returns 0.0 in Phase 1

// PID tuning functions
void pump_set_ph_pid(float kp, float ki, float kd);         // Set pH PID parameters
void pump_get_ph_pid(float* kp, float* ki, float* kd);      // Get pH PID parameters

// Status and utility functions
void pump_print_status(void);                               // Print pump statistics
bool pump_is_running(PumpId pump);                       // Check if pump running
void pump_reset_counters(void);                             // Reset dose counters
float pump_get_total_dosed(PumpId pump);                 // Get total ml dosed

// Auto control functions
void pump_enable_auto_ph(bool enabled);                     // Enable/disable auto pH
bool pump_is_auto_ph_enabled(void);                         // Check auto pH status

#endif // PUMP_H