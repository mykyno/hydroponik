/**
 * @file calibration.cpp
 * @brief Sensor calibration management implementation
 * @author Arduino Developer
 * @date 2025
 */

#include "calibration.h"
#include "sensors.h"

//=============================================================================
// GLOBAL VARIABLES
//=============================================================================

// Global calibration data (loaded from NVS or defaults)
calibration_t calibration;

//=============================================================================
// NVS CALIBRATION MANAGEMENT FUNCTIONS
//=============================================================================

/**
 * @brief Load calibration parameters from NVS storage
 * Sets default values if no calibration data exists
 */
void calibration_load(void) {
  // Get the size of stored calibration data
  size_t calLen = preferences.getBytesLength(NVS_CALIBRATION_KEY);
  
  if (calLen == sizeof(calibration_t)) {
    // Load calibration from NVS using safe aligned copy
    if (preferences.getBytes(NVS_CALIBRATION_KEY, &calibration, sizeof(calibration_t)) == sizeof(calibration_t)) {
      // Successfully loaded
    } else {
      Serial.println("ERROR: Failed to load calibration data");
      calibration_reset();
      return;
    }
    
    Serial.println("Calibration loaded from NVS:");
    Serial.printf("  pH: slope=%.6f, offset=%.4f\n", calibration.ph_slope, calibration.ph_offset);
    Serial.printf("  EC: slope=%.6f, offset=%.4f\n", calibration.ec_slope, calibration.ec_offset);
  } else {
    // No valid calibration found, use defaults
    calibration.ph_slope = DEFAULT_PH_SLOPE;
    calibration.ph_offset = DEFAULT_PH_OFFSET;
    calibration.ec_slope = DEFAULT_EC_SLOPE;
    calibration.ec_offset = DEFAULT_EC_OFFSET;
    // Set volume calibration to invalid (needs manual calibration)
    calibration.empty_distance = 0.0f;
    calibration.half_distance = 0.0f;
    calibration.full_distance = 0.0f;
    calibration.max_volume = 0.0f;
    
    Serial.println("No calibration found in NVS, using defaults:");
    Serial.printf("  pH: slope=%.6f, offset=%.4f\n", calibration.ph_slope, calibration.ph_offset);
    Serial.printf("  EC: slope=%.6f, offset=%.4f\n", calibration.ec_slope, calibration.ec_offset);
  }
}

/**
 * @brief Save current calibration parameters to NVS storage
 * @return true if save successful, false otherwise
 */
bool calibration_save(void) {
  // Save calibration structure to NVS using putBytes
  size_t written = preferences.putBytes(NVS_CALIBRATION_KEY, &calibration, sizeof(calibration_t));
  
  if (written == sizeof(calibration_t)) {
    Serial.println("Calibration saved to NVS successfully");
    return true;
  } else {
    Serial.println("ERROR: Failed to save calibration to NVS");
    return false;
  }
}

/**
 * @brief Reset calibration to factory defaults
 * Useful for starting calibration process over or troubleshooting
 */
void calibration_reset(void) {
  // Set to factory default values
  calibration.ph_slope = DEFAULT_PH_SLOPE;
  calibration.ph_offset = DEFAULT_PH_OFFSET;
  calibration.ec_slope = DEFAULT_EC_SLOPE;
  calibration.ec_offset = DEFAULT_EC_OFFSET;
  
  // Save defaults to NVS
  calibration_save();
  
  Serial.println("Calibration reset to factory defaults");
  Serial.printf("  pH: slope=%.6f, offset=%.4f\n", DEFAULT_PH_SLOPE, DEFAULT_PH_OFFSET);
  Serial.printf("  EC: slope=%.6f, offset=%.4f\n", DEFAULT_EC_SLOPE, DEFAULT_EC_OFFSET);
}

//=============================================================================
// SENSOR CALIBRATION FUNCTIONS
//=============================================================================

/**
 * @brief Perform 2-point pH calibration with flexible buffer values
 * @param voltage1 Voltage reading for first buffer solution (mV)
 * @param ph_value1 Known pH value of first buffer solution
 * @param voltage2 Voltage reading for second buffer solution (mV)
 * @param ph_value2 Known pH value of second buffer solution
 * @return true if calibration successful, false if invalid parameters
 */
bool calibration_ph_2point(float voltage1, float ph_value1, float voltage2, float ph_value2) {
  // Validate input parameters (voltages should be different and within reasonable range)
  if (abs(voltage1 - voltage2) < 50.0 ||         // Minimum 50mV difference
      voltage1 < 0 || voltage1 > 3300 ||         // Valid voltage range
      voltage2 < 0 || voltage2 > 3300 ||
      ph_value1 < 0.0f || ph_value1 > 14.0f ||  // Valid pH range
      ph_value2 < 0.0f || ph_value2 > 14.0f) {
    Serial.println("ERROR: Invalid pH calibration parameters");
    return false;
  }
  
  // Calculate slope: delta_pH / delta_voltage = (7.0 - 4.01) / (ph7_voltage - ph4_voltage)
  calibration.ph_slope = (ph_value2 - ph_value1) / (voltage2 - voltage1);
  
  // Calculate offset: pH = slope * voltage + offset => offset = pH - slope * voltage
  // Using pH 7.0 point: offset = 7.0 - slope * ph7_voltage
  calibration.ph_offset = ph_value2 - (calibration.ph_slope * voltage2);
  
  Serial.println("pH calibration completed:");
  Serial.printf("  2-point calibration: pH %.2f at %.1fmV, pH %.2f at %.1fmV\n", ph_value1, voltage1, ph_value2, voltage2);
  Serial.printf("  Calculated slope: %.6f pH/mV\n", calibration.ph_slope);
  Serial.printf("  Calculated offset: %.4f pH\n", calibration.ph_offset);
  
  // Save new calibration to NVS
  return calibration_save();
}

/**
 * @brief Perform 2-point EC calibration
 * @param low_voltage Voltage reading in low EC solution (mV)
 * @param low_ec_value Known EC value of low solution (mS/cm)
 * @param high_voltage Voltage reading in high EC solution (mV)
 * @param high_ec_value Known EC value of high solution (mS/cm)
 * @return true if calibration successful, false if invalid parameters
 */
bool calibration_ec_2point(float low_voltage, float low_ec_value, float high_voltage, float high_ec_value) {
  // Validate input parameters
  if (abs(low_voltage - high_voltage) < 50.0 ||    // Minimum 50mV difference
      abs(low_ec_value - high_ec_value) < 0.1 ||   // Minimum 0.1 mS/cm difference
      low_voltage < 0 || low_voltage > 3300 ||     // Valid voltage range
      high_voltage < 0 || high_voltage > 3300 ||
      low_ec_value < 0 || high_ec_value < 0) {     // EC values must be positive
    Serial.println("ERROR: Invalid EC calibration parameters");
    return false;
  }
  
  // Calculate slope: delta_EC / delta_voltage
  calibration.ec_slope = (high_ec_value - low_ec_value) / (high_voltage - low_voltage);
  
  // Calculate offset: EC = slope * voltage + offset => offset = EC - slope * voltage
  // Using low point: offset = low_ec_value - slope * low_voltage
  calibration.ec_offset = low_ec_value - (calibration.ec_slope * low_voltage);
  
  Serial.println("EC calibration completed:");
  Serial.printf("  2-point calibration: %.2f mS/cm at %.1fmV, %.2f mS/cm at %.1fmV\n", 
                low_ec_value, low_voltage, high_ec_value, high_voltage);
  Serial.printf("  Calculated slope: %.6f (mS/cm)/mV\n", calibration.ec_slope);
  Serial.printf("  Calculated offset: %.4f mS/cm\n", calibration.ec_offset);
  
  // Save new calibration to NVS
  return calibration_save();
}

/**
 * @brief Perform 3-point volume calibration using HC-SR04 distance measurements
 * @param empty_dist Distance when reservoir is empty (cm)
 * @param half_dist Distance when reservoir is half full (cm)  
 * @param full_dist Distance when reservoir is full (cm)
 * @param max_vol Maximum volume of reservoir (liters)
 * @return true if calibration successful, false if invalid parameters
 */
bool calibration_volume_3point(float empty_dist, float half_dist, float full_dist, float max_vol) {
  // Validate input parameters - distances should be in logical order
  if (full_dist >= half_dist || half_dist >= empty_dist || 
      empty_dist <= 0 || full_dist <= 0 || max_vol <= 0) {
    Serial.println("ERROR: Invalid volume calibration parameters");
    Serial.println("Expected: full_dist < half_dist < empty_dist (all > 0)");
    return false;
  }
  
  // Store calibration points
  calibration.empty_distance = empty_dist;
  calibration.half_distance = half_dist;
  calibration.full_distance = full_dist;
  calibration.max_volume = max_vol;
  
  Serial.println("Volume calibration completed:");
  Serial.printf("  Empty: %.1f cm = 0.0 L\n", empty_dist);
  Serial.printf("  Half:  %.1f cm = %.1f L\n", half_dist, max_vol / 2.0f);
  Serial.printf("  Full:  %.1f cm = %.1f L\n", full_dist, max_vol);
  
  return calibration_save();
}

/**
 * @brief Convert distance measurement to volume using 3-point calibration
 * @param distance Current distance reading (cm)
 * @return Volume in liters (0.0 if calibration not valid)
 */
float calibration_distance_to_volume(float distance) {
  // Check for sensor error
  if (distance < 0) {
    return -1.0f; // Sensor error - propagate error
  }
  
  // Check if volume calibration is available
  if (calibration.max_volume <= 0 || calibration.empty_distance <= 0) {
    return 0.0f; // No calibration available
  }
  
  // Constrain distance to calibrated range
  if (distance >= calibration.empty_distance) return 0.0f;        // Empty or below
  if (distance <= calibration.full_distance) return calibration.max_volume;  // Full or above
  
  // Linear interpolation between calibration points
  if (distance > calibration.half_distance) {
    // Between empty and half: interpolate from 0 to max_volume/2
    float ratio = (calibration.empty_distance - distance) / 
                  (calibration.empty_distance - calibration.half_distance);
    return ratio * (calibration.max_volume / 2.0f);
  } else {
    // Between half and full: interpolate from max_volume/2 to max_volume
    float ratio = (calibration.half_distance - distance) / 
                  (calibration.half_distance - calibration.full_distance);
    return (calibration.max_volume / 2.0f) + ratio * (calibration.max_volume / 2.0f);
  }
}

//=============================================================================
// UTILITY FUNCTIONS
//=============================================================================

/**
 * @brief Check if calibration data appears valid
 * @return true if calibration values are within reasonable ranges
 */
bool calibration_is_valid(void) {
  // Check pH calibration ranges (reasonable slopes and offsets)
  bool ph_valid = (calibration.ph_slope > -0.1 && calibration.ph_slope < 0.1) &&
                  (calibration.ph_offset > 0.0 && calibration.ph_offset < 14.0);
  
  // Check EC calibration ranges
  bool ec_valid = (calibration.ec_slope > -1.0 && calibration.ec_slope < 1.0) &&
                  (calibration.ec_offset >= 0.0);
  
  return ph_valid && ec_valid;
}

//=============================================================================
// INTERACTIVE CALIBRATION FUNCTIONS
//=============================================================================

/**
 * @brief Interactive pH calibration via serial interface
 * Handles all serial input/output for 2-point pH calibration
 */
void calibration_interactive_ph(void) {
  Serial.println("Enter voltage (mV) and pH value for buffer 1");
  Serial.setTimeout(10000);
  float v1 = Serial.parseFloat(); 
  float p1 = Serial.parseFloat();
  if (v1 == 0.0 && p1 == 0.0) {
    Serial.println("Invalid input - calibration cancelled");
    return;
  }
  Serial.printf("%.1f mV, pH %.2f\n", v1, p1);
  
  Serial.println("Enter voltage (mV) and pH value for buffer 2");
  float v2 = Serial.parseFloat(); 
  float p2 = Serial.parseFloat();
  if (v2 == 0.0 && p2 == 0.0) {
    Serial.println("Invalid input - calibration cancelled");
    return;
  }
  Serial.printf("%.1f mV, pH %.2f\n", v2, p2);
  
  if (calibration_ph_2point(v1, p1, v2, p2)) {
    Serial.println("pH calibration successful and saved");
  } else {
    Serial.println("pH calibration failed");
  }
}

/**
 * @brief Interactive EC calibration via serial interface
 * Handles all serial input/output for 2-point EC calibration
 */
void calibration_interactive_ec(void) {
  Serial.println("Enter low voltage in mV and EC value (mS/cm)");
  Serial.setTimeout(10000);
  float lv = Serial.parseFloat(); 
  float le = Serial.parseFloat();
  if (lv == 0.0 && le == 0.0) {
    Serial.println("Invalid input - calibration cancelled");
    return;
  }
  Serial.println(lv); Serial.println(le);
  
  Serial.println("Enter high voltage in mV and EC value (mS/cm)");
  float hv = Serial.parseFloat(); 
  float he = Serial.parseFloat();
  if (hv == 0.0 && he == 0.0) {
    Serial.println("Invalid input - calibration cancelled");
    return;
  }
  Serial.println(hv); Serial.println(he);
  
  if (calibration_ec_2point(lv, le, hv, he)) {
    Serial.println("EC calibration successful and saved");
  } else {
    Serial.println("EC calibration failed");
  }
}

/**
 * @brief Interactive volume calibration via serial interface
 * Handles all serial input/output for 3-point volume calibration using HC-SR04
 */
void calibration_interactive_volume(void) {
  Serial.println("Volume calibration: Fill reservoir EMPTY, then press any key");
  while (!Serial.available());
  Serial.read();
  float empty_dist = sensor_read_distance_raw();
  Serial.printf("Empty distance: %.1f cm\n", empty_dist);
  
  Serial.println("Fill reservoir HALF FULL, then press any key");
  while (!Serial.available());
  Serial.read(); 
  float half_dist = sensor_read_distance_raw();
  Serial.printf("Half distance: %.1f cm\n", half_dist);
  
  Serial.println("Fill reservoir COMPLETELY FULL, then press any key");
  while (!Serial.available());
  Serial.read();
  float full_dist = sensor_read_distance_raw();
  Serial.printf("Full distance: %.1f cm\n", full_dist);
  
  Serial.println("Enter maximum volume of reservoir (liters):");
  Serial.setTimeout(10000);
  float max_vol = Serial.parseFloat();
  if (max_vol <= 0) {
    Serial.println("Invalid volume - calibration cancelled");
    return;
  }
  
  if (calibration_volume_3point(empty_dist, half_dist, full_dist, max_vol)) {
    Serial.println("Volume calibration successful and saved");
  } else {
    Serial.println("Volume calibration failed");
  }
}

/**
 * @brief Print current calibration status and values
 */
void calibration_print_status(void) {
  Serial.println("=== Calibration Status ===");
  Serial.printf("pH Sensor:\n");
  Serial.printf("  Slope: %.6f pH/mV\n", calibration.ph_slope);
  Serial.printf("  Offset: %.4f pH\n", calibration.ph_offset);
  Serial.printf("EC Sensor:\n");
  Serial.printf("  Slope: %.6f (mS/cm)/mV\n", calibration.ec_slope);
  Serial.printf("  Offset: %.4f mS/cm\n", calibration.ec_offset);
  Serial.printf("Valid: %s\n", calibration_is_valid() ? "YES" : "NO");
  Serial.println("========================");
}
