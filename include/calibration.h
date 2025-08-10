/**
 * @file calibration.h
 * @brief Sensor calibration management interface
 * @author Arduino Developer
 * @date 2025
 * 
 * This header provides interface for:
 * - NVS storage and retrieval of calibration parameters
 * - 2-point calibration for pH and EC sensors
 * - Factory reset and validation of calibration data
 */

#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <Arduino.h>
#include <Preferences.h>

//=============================================================================
// CALIBRATION CONFIGURATION
//=============================================================================

constexpr const char* NVS_CALIBRATION_KEY = "calibration";  // Key for calibration structure

//=============================================================================
// DEFAULT CALIBRATION VALUES
//=============================================================================

constexpr float DEFAULT_PH_SLOPE = -0.0169f;    // Default pH slope (pH/mV) - theoretical value
constexpr float DEFAULT_PH_OFFSET = 7.0f;       // Default pH offset at neutral point
constexpr float DEFAULT_EC_SLOPE = 0.001f;      // Default EC slope (mS/cm per mV)
constexpr float DEFAULT_EC_OFFSET = 0.0f;       // Default EC offset

//=============================================================================
// DATA STRUCTURES
//=============================================================================

/**
 * @brief Calibration parameters for NVS storage
 * Contains slope and offset values for pH and EC sensors
 */
typedef struct {
    float ph_slope;     // pH sensor slope (pH/mV)
    float ph_offset;    // pH sensor offset (pH)
    float ec_slope;     // EC sensor slope (mS/cm per mV)
    float ec_offset;    // EC sensor offset (mS/cm)
    // Volume calibration: 3-point distance-to-volume mapping
    float empty_distance;    // Distance when reservoir is empty (cm)
    float half_distance;     // Distance when reservoir is half full (cm)
    float full_distance;     // Distance when reservoir is full (cm)
    float max_volume;        // Maximum volume of reservoir (liters)
} calibration_t;

//=============================================================================
// GLOBAL VARIABLES (EXTERN DECLARATIONS)
//=============================================================================

// NVS preferences object (defined in main.cpp)
extern Preferences preferences;

// Global calibration data (defined in calibration.cpp)
extern calibration_t calibration;

//=============================================================================
// FUNCTION DECLARATIONS
//=============================================================================

// NVS calibration management
void calibration_load(void);                     // Load calibration from NVS
bool calibration_save(void);                     // Save calibration to NVS
void calibration_reset(void);                    // Reset to factory defaults

// Sensor calibration functions
// Flexible 2-point pH calibration accepting buffer voltages and pH values
bool calibration_ph_2point(float voltage1, float ph_value1, float voltage2, float ph_value2);  // 2-point flexible pH calibration
bool calibration_ec_2point(float low_voltage, float low_ec_value, float high_voltage, float high_ec_value);  // 2-point EC calibration

// Volume calibration functions
bool calibration_volume_3point(float empty_dist, float half_dist, float full_dist, float max_vol);  // 3-point volume calibration
float calibration_distance_to_volume(float distance);  // Convert distance to volume using calibration

// Interactive calibration functions (handle serial input/output)
void calibration_interactive_ph(void);      // Interactive pH calibration via serial
void calibration_interactive_ec(void);      // Interactive EC calibration via serial
void calibration_interactive_volume(void);  // Interactive volume calibration via serial

// Utility functions
bool calibration_is_valid(void);                 // Check if calibration data is valid
void calibration_print_status(void);             // Print current calibration values

#endif // CALIBRATION_H
