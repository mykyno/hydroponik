/**
 * @file sensors.cpp
 * @brief Professional sensor reading system implementation for pH, EC, and ultrasonic sensors
 * @author Arduino Developer
 * @date 2025
 */

#include "sensors.h"
#include "state_machine.h"
#include <OneWire.h>
#include <DallasTemperature.h>
// Temperature compensation coefficient for EC (per °C)
#define EC_TEMP_COEFF 0.02f

// OneWire and temperature sensor objects
static OneWire oneWire(TEMP_SENSOR_PIN);
static DallasTemperature tempSensor(&oneWire);
//=============================================================================
// TEMPERATURE READING FUNCTION
//=============================================================================
/**
 * @brief Read water temperature from DS18B20 sensor
 * @return Temperature in degrees Celsius
 */
float sensor_read_temperature_raw(void) {
  tempSensor.requestTemperatures();
  float tempC = tempSensor.getTempCByIndex(0);
  return (tempC == DEVICE_DISCONNECTED_C) ? 25.0f : tempC;
}

//=============================================================================
// GLOBAL VARIABLES 
//=============================================================================

/**
 * @brief Global sensor configuration
 * Configuration loaded from NVS or set to defaults
 */
static sensor_config_t sensor_config = {
    .ph_slope = -59.16f,                   // Standard pH probe slope (mV/pH) at 25°C
    .ph_offset = 0.0f,                     // pH calibration offset
    .ec_conversion_factor = 2.5f,          // EC probe conversion factor
    .sensor_interval_ms = SENSOR_INTERVAL, // Reading interval from define
    .warmup_time_ms = SENSOR_WARMUP,       // Warmup time from define
    .filter_samples = FILTER_SAMPLES,      // Sample count from define
    .ph_alpha = 0.2f,                      // EMA weight for new pH readings
    .ec_alpha = 0.2f,                      // EMA weight for new EC readings
    .dist_alpha = 0.3f,                    // EMA weight for new distance readings
    .calibration_valid = false             // Default to invalid until loaded/calibrated
};

/**Q
 * @brief Global sensor stateQ
 * Maintains current system state and readings
 */
static sensor_state_t sensor_state; // Default constructor handles initialization

//=============================================================================
// SENSOR SYSTEM FUNCTIONS
//=============================================================================

/**
 * @brief Initialize sensor hardware and system state
 * @return true if initialization successful, false otherwise
 */
bool sensor_initialize(void) {
  // Configure analog sensor power control pins
  pinMode(PH_POWER_PIN, OUTPUT);
  pinMode(EC_POWER_PIN, OUTPUT);
  
  // Start with sensors powered off to save energy
  digitalWrite(PH_POWER_PIN, LOW);
  digitalWrite(EC_POWER_PIN, LOW);
  
  // Configure ultrasonic sensor pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  // Initialize temperature sensor
  tempSensor.begin();
  
  // Initialize system state
  sensor_state.initialized = true;
  sensor_state.last_reading_time = 0;
  
  // Initialize sensor state machine to READY
  sensor_transition_to(SensorState::READY);
  
  return true;  // Return success (could add hardware verification)
}

/**
 * @brief Check if sensor reading update is needed and manage state transitions
 * @return true if enough time has passed since last reading
 */
bool sensor_update_needed(void) {
  // Check timing interval
  bool time_for_reading = (millis() - sensor_state.last_reading_time >= sensor_config.sensor_interval_ms);
  
  if (time_for_reading) {
    // Initiate sensor reading cycle by transitioning to WARMING_UP state
    if (state_manager.sensor_state == SensorState::READY || 
        state_manager.sensor_state == SensorState::INITIALIZING) {
      sensor_transition_to(SensorState::WARMING_UP);
    }
  }
  
  return time_for_reading;
}

/**
 * @brief Read all sensors with state machine integration
 * @return Filtered sensor readings structure
 */
sensor_readings_t sensor_read_all(void) {
  sensor_readings_t result; // Default constructor initializes to safe invalid state
  
  SensorState current_state = state_manager.sensor_state;
  uint32_t state_duration = sensor_get_state_duration_ms();
  
  switch (current_state) {
    case SensorState::WARMING_UP:
      // Power on sensors and wait for stabilization
      digitalWrite(PH_POWER_PIN, HIGH);
      digitalWrite(EC_POWER_PIN, HIGH);
      
      if (state_duration >= SENSOR_WARMUP) {
        // Warmup complete, transition to reading
        sensor_transition_to(SensorState::READING);
      }
      break;
      
    case SensorState::READING: {
      // Perform actual sensor readings
      sensor_readings_t raw_readings = sensor_read_raw();
      sensor_transition_to(SensorState::FILTERING);
      
      // Store raw readings for filtering phase
      sensor_state.current = raw_readings;
      break;
    }
      
    case SensorState::FILTERING:
      // Apply filtering and prepare final results
      if (sensor_state.current.valid) {
        sensor_state.filtered = sensor_apply_filter(sensor_state.current, sensor_state.filtered);
        result = sensor_state.filtered;
      }
      
      // Power down sensors and transition to ready
      digitalWrite(PH_POWER_PIN, LOW);
      digitalWrite(EC_POWER_PIN, LOW);
      
      sensor_transition_to(SensorState::READY);
      sensor_state.last_reading_time = millis();
      break;
      
    case SensorState::READY:
      // Return last filtered readings
      result = sensor_state.filtered;
      break;
      
    case SensorState::ERROR:
      // In error state, ensure sensors are powered off
      digitalWrite(PH_POWER_PIN, LOW);
      digitalWrite(EC_POWER_PIN, LOW);
      break;
      
    default:
      break;
  }
  
  return result;
}

/**
 * @brief Read all sensors without filtering
 * @return Raw sensor readings structure
 */
sensor_readings_t sensor_read_raw(void) {
  sensor_readings_t readings; // Default constructor initializes to safe state
  // Read temperature first for compensation
  readings.temperature = sensor_read_temperature_raw();
  
  // Set timestamp
  readings.timestamp = millis();
  
  // Read each sensor individually (sensors should already be powered on)
  readings.ph = sensor_read_ph_raw(readings.temperature, calibration);
  readings.ec = sensor_read_ec_raw(readings.temperature, calibration);
  
  // Read distance and convert to volume
  float distance = sensor_read_distance_raw();
  readings.volume = calibration_distance_to_volume(distance);
  
  // Validate readings with error detection and state management
  bool ph_valid = (readings.ph > 0 && readings.ph < 14);
  bool ec_valid = (readings.ec >= 0);
  bool volume_valid = (readings.volume >= 0); // -1 means error
  
  readings.valid = ph_valid && ec_valid && volume_valid;
  
  // Sensor error handling - transition to error state if readings invalid
  if (!readings.valid) {
    static uint8_t error_count = 0;
    error_count++;
    
    if (error_count > 3) {
      sensor_transition_to(SensorState::ERROR);
      error_count = 0;
    }
  } else {
    // Reset error counter on successful reading
    static uint8_t error_count = 0;
    error_count = 0;
  }
  
  return readings;
}

//=============================================================================
// INDIVIDUAL SENSOR READING FUNCTIONS
//=============================================================================

/**
 * @brief Read pH sensor with power management and multi-sampling
 * @return Average pH value (0-14 scale)
 */
/**
 * @brief Read pH sensor with power management, multi-sampling, and temperature compensation
 * @param temperature Water temperature in degrees Celsius
 * @param calib Calibration parameters for pH sensor
 * @return Compensated average pH value (0-14 scale)
 */
float sensor_read_ph_raw(float temperature, const calibration_t& calib) {
  // Note: Power management is now handled by state machine in sensor_read_all()
  // This function assumes sensors are already powered and warmed up
  
  // Take multiple samples for stability
  float sum = 0;
  for (int i = 0; i < sensor_config.filter_samples; i++) {
    // Read raw ADC value (0-4095 for ESP32-S3 12-bit ADC)
    int rawValue = analogRead(PH_PIN);
    
    // Convert to voltage (0-3.3V for ESP32-S3)
    float voltage = rawValue * (3.3f / 4095.0f);
    
    // Convert voltage to millivolts for pH calculation
    float millivolts = voltage * 1000.0;
    
    // Apply calibration formula to get pH value using NVS calibration parameters
    float phValue = calib.ph_slope * millivolts + calib.ph_offset;
    
    // Constrain to valid pH range and accumulate
    sum += constrain(phValue, 0.0, 14.0);
  }
  
  // Note: Power management is now handled by state machine
  // Sensor will be powered off after all readings are complete
  
  // Compute average raw pH
  float rawPh = sum / sensor_config.filter_samples;
  // Temperature compensation: adjust by 0.03 pH per °C deviation from 25°C
  float compensatedPh = rawPh + ((temperature - 25.0f) * 0.03f);
  return constrain(compensatedPh, 0.0, 14.0);
}

/**
 * @brief Read EC sensor with power management and multi-sampling
 * @return Average EC value in mS/cm
 */
/**
 * @brief Read EC sensor with power management, multi-sampling, and temperature compensation
 * @param temperature Water temperature in degrees Celsius
 * @param calib Calibration parameters for EC sensor
 * @return Compensated average EC value (mS/cm)
 */
float sensor_read_ec_raw(float temperature, const calibration_t& calib) {
  // Note: Power management is now handled by state machine in sensor_read_all()
  // This function assumes sensors are already powered and warmed up
  
  // Take multiple samples for stability
  float sum = 0;
  for (int i = 0; i < sensor_config.filter_samples; i++) {
    // Read raw ADC value (0-4095 for ESP32-S3 12-bit ADC)
    int rawValue = analogRead(EC_PIN);
    
    // Convert to voltage (0-3.3V for ESP32-S3)
    float voltage = rawValue * (3.3f / 4095.0f);
    
    // Convert voltage to millivolts for EC calculation
    float millivolts = voltage * 1000.0;
    
    // Apply calibration formula to get EC value using NVS calibration parameters
    float ecValue = calib.ec_slope * millivolts + calib.ec_offset;
    
    // Ensure non-negative values and accumulate
    sum += max(ecValue, 0.0f);
  }
  
  // Note: Power management is now handled by state machine
  // Sensor will be powered off after all readings are complete
  
  // Compute average raw EC
  float rawEc = sum / sensor_config.filter_samples;
  // Temperature compensation: adjust by coefficient per °C deviation from 25°C
  float compensatedEc = rawEc * (1.0f + EC_TEMP_COEFF * (temperature - 25.0f));
  return compensatedEc;
}

/**
 * @brief Read ultrasonic distance sensor (HC-SR04)
 * @return Distance in centimeters, or previous value if error
 */
float sensor_read_distance_raw(void) {
  // Send trigger pulse (10µs high pulse)
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);           // Clean low state
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);          // Trigger pulse
  digitalWrite(TRIG_PIN, LOW);
  
  // Measure echo pulse duration (with timeout)
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);  // 30ms timeout
  
  // Handle timeout error - return error indicator
  if (duration == 0) {
    Serial.println("Distance sensor timeout");
    return -1.0f; // Clear error indicator
  }
  
  // Convert time to distance: distance = (time * speed_of_sound) / 2
  // Speed of sound ≈ 343 m/s = 0.034 cm/µs
  float distance = (duration * 0.034f) / 2.0f;
  
  // Reject obviously invalid readings (sensor range: 2-400cm)
  if (distance < 2.0f || distance > 400.0f) {
    return 20.0f; // Return reasonable default distance
  }
  
  return distance;
}

//=============================================================================
// DATA PROCESSING FUNCTIONS
//=============================================================================

/**
 * @brief Apply low-pass filtering to sensor readings
 * @param new_reading Latest sensor readings
 * @param filtered Previous filtered readings
 * @return Filtered sensor readings with smoothed values
 */
sensor_readings_t sensor_apply_filter(sensor_readings_t new_reading, sensor_readings_t filtered) {
  sensor_readings_t result = new_reading;  // Copy metadata (timestamp, valid flag)
  
  // Apply low-pass filter to each sensor value
  // Formula: filtered = (old_value * weight) + (new_value * (1-weight))
  
  // pH: EMA using configurable alpha
  result.ph = (filtered.ph * (1.0f - sensor_config.ph_alpha)) + (new_reading.ph * sensor_config.ph_alpha);
  
  // EC: EMA using configurable alpha
  result.ec = (filtered.ec * (1.0f - sensor_config.ec_alpha)) + (new_reading.ec * sensor_config.ec_alpha);
  
  // Volume: EMA using configurable alpha (reuse dist_alpha)
  result.volume = (filtered.volume * (1.0f - sensor_config.dist_alpha)) + (new_reading.volume * sensor_config.dist_alpha);
  
  // Temperature: update without EMA filtering
  result.temperature = new_reading.temperature;
  
  return result;
}

//=============================================================================
// OUTPUT FUNCTIONS
//=============================================================================

/**
 * @brief Print formatted sensor readings to serial port
 * @param readings Sensor readings structure to display
 */
void sensor_print_readings(sensor_readings_t readings) {
  // Print pH value with 2 decimal places
  Serial.print(readings.ph, 2);
  Serial.print(" | ");
  
  // Print EC value with 2 decimal places
  Serial.print(readings.ec, 2);
  Serial.print(" | ");
  
  // Print volume with 1 decimal place
  Serial.print(readings.volume, 1);
  Serial.println(" L");
}
