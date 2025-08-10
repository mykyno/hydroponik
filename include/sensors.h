/**
 * @file sensors.h
 * @brief Professional sensor reading system interface for pH, EC, and ultrasonic sensors
 * @author Arduino Developer
 * @date 2025
 * 
 * This header provides interface for:
 * - Non-blocking sensor reading with configurable intervals
 * - Power management for analog sensors
 * - Multi-sample averaging and low-pass filtering
 * - NVS storage for calibration data persistence (ESP32-S3)
 */

#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>
#include <Preferences.h>
#include "calibration.h"
//=============================================================================
// TEMPERATURE SENSOR (DS18B20) INTEGRATION
//=============================================================================
#include <OneWire.h>
#include <DallasTemperature.h>
constexpr int TEMP_SENSOR_PIN = 10;    // DS18B20 data pin (1-Wire)

//=============================================================================
// HARDWARE CONFIGURATION (ESP32-S3)
//=============================================================================

// Analog sensor pins (ESP32-S3 ADC pins)
constexpr int PH_PIN = 6;           // pH sensor analog input (GPIO5)
constexpr int EC_PIN = 5;           // EC sensor analog input (GPIO6)

// Digital control pins
constexpr int PH_POWER_PIN = 7;      // pH sensor power control
constexpr int EC_POWER_PIN = 4;      // EC sensor power control
constexpr int TRIG_PIN = 8;          // Ultrasonic trigger pin
constexpr int ECHO_PIN = 9;         // Ultrasonic echo pin

//=============================================================================
// TIMING AND SAMPLING CONFIGURATION
//=============================================================================

constexpr uint32_t SENSOR_INTERVAL = 5000;  // Sensor reading interval (ms)
constexpr uint32_t SENSOR_WARMUP = 200;     // Sensor warmup time after power on (ms)
constexpr int FILTER_SAMPLES = 5;                // Number of samples for averaging

//=============================================================================
// NVS CONFIGURATION
//=============================================================================

#define NVS_NAMESPACE "sensor_cal"  // NVS namespace for calibration data

//=============================================================================
// DATA STRUCTURES
//=============================================================================

/**
 * @brief Sensor configuration parameters
 * Contains calibration constants and operational settings
 */
typedef struct {
    float ph_slope;                   // pH sensor slope for calibration
    float ph_offset;                  // pH sensor offset for calibration
    float ec_conversion_factor;       // EC sensor conversion factor
    uint32_t sensor_interval_ms;      // Time between sensor readings
    uint32_t warmup_time_ms;          // Sensor stabilization time
    uint8_t filter_samples;           // Number of samples for averaging
    float ph_alpha;                   // EMA alpha coefficient for pH
    float ec_alpha;                   // EMA alpha coefficient for EC
    float dist_alpha;                 // EMA alpha coefficient for distance/volume
    bool calibration_valid;           // Calibration status flag
} sensor_config_t;

/**
 * @brief Sensor reading data structure
 * Contains all sensor values with timestamp and validity flag
 */
struct sensor_readings_t {
    float ph;                    // pH value (0-14)
    float ec;                    // Electrical conductivity (mS/cm)
    float volume;                // Reservoir volume (liters)
    float temperature;           // Water temperature (°C)
    uint32_t timestamp;     // Reading timestamp (ms)
    bool valid;                  // Data validity flag
    
    // Default constructor - initializes to safe, invalid state
    sensor_readings_t() : ph(0.0f), ec(0.0f), volume(0.0f), temperature(0.0f), timestamp(0), valid(false) {}
    
    // Parameterized constructor with automatic timestamp and validity
    sensor_readings_t(float ph_val, float ec_val, float vol_val, float temp_val) 
        : ph(ph_val), ec(ec_val), volume(vol_val), temperature(temp_val), 
          timestamp(millis()), valid(true) {}
};

/**
 * @brief Sensor system state management
 * Maintains current and filtered readings with timing information
 */
struct sensor_state_t {
    sensor_readings_t current;        // Latest raw readings
    sensor_readings_t filtered;       // Filtered/smoothed readings
    uint32_t last_reading_time;  // Last reading timestamp
    bool initialized;                 // System initialization status
    
    // Default constructor
    sensor_state_t() : current(), filtered(7.0f, 1.0f, 0.0f, 25.0f), 
                       last_reading_time(0), initialized(false) {}
};

//=============================================================================
// GLOBAL VARIABLES (EXTERN DECLARATIONS)
//=============================================================================

// NVS preferences object (defined in main.cpp)
extern Preferences preferences;

//=============================================================================
// FUNCTION DECLARATIONS
//=============================================================================

// System management functions
bool sensor_initialize(void);                    // Initialize sensor hardware and state
bool sensor_update_needed(void);                 // Check if reading update is needed

// Reading functions
sensor_readings_t sensor_read_all(void);         // Read all sensors with filtering
sensor_readings_t sensor_read_raw(void);         // Read all sensors without filtering

// Individual sensor functions
// Temperature-compensated sensor read functions accept calibration parameters
float sensor_read_ph_raw(float temperature, const calibration_t& calib);      // Read pH sensor with temperature compensation
float sensor_read_ec_raw(float temperature, const calibration_t& calib);      // Read EC sensor with temperature compensation
float sensor_read_temperature_raw(void);          // Read water temperature
float sensor_read_distance_raw(void);            // Read ultrasonic distance sensor

// Data processing functions
sensor_readings_t sensor_apply_filter(sensor_readings_t new_reading, sensor_readings_t filtered);

// Output functions
void sensor_print_readings(sensor_readings_t readings);  // Print formatted sensor data

#endif // SENSORS_H
