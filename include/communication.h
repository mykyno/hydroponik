/**
 * @file communication.h
 * @brief Hybrid WiFi/Serial communication manager for ESP32-S3 hydroponic system
 * @author Arduino Developer
 * @date 2025
 * 
 * Provides unified communication interface with automatic WiFi/Serial switching.
 * Serial USB always available as backup, WiFi Telnet as primary when connected.
 */

#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>

//=============================================================================
// CONSTANTS
//=============================================================================

#define WIFI_CONNECT_TIMEOUT_MS 10000
#define WIFI_RETRY_INTERVAL_MS 30000
#define TELNET_PORT 23
#define MAX_TELNET_CLIENTS 3
#define COMM_BUFFER_SIZE 256
#define OTA_PORT 3232
#define OTA_HOSTNAME "ESP32-Hydroponic"

//=============================================================================
// ENUMERATIONS
//=============================================================================

/**
 * @brief Communication interface states
 */
enum class CommState {
  SERIAL_ONLY,      // WiFi failed/unavailable, Serial active
  WIFI_CONNECTING,  // Serial active, WiFi connecting non-blocking
  WIFI_PRIMARY,     // WiFi connected, Telnet active, Serial backup
  ERROR             // Communication system error
};

/**
 * @brief Input source priority
 */
enum class InputSource {
  NONE,
  SERIAL_USB,  // Highest priority - emergency access
  TELNET_CLIENT   // Secondary priority
};

//=============================================================================
// COMMUNICATION MANAGER CLASS
//=============================================================================

/**
 * @brief Unified communication manager for dual WiFi/Serial interface
 */
class CommunicationManager {
private:
  // WiFi configuration
  const char* wifi_ssid;
  const char* wifi_password;
  
  // State management
  CommState current_state;
  uint32_t last_wifi_attempt;
  uint32_t state_change_time;
  
  // Telnet server
  WiFiServer* telnet_server;
  WiFiClient telnet_clients[MAX_TELNET_CLIENTS];
  uint8_t active_clients;
  
  // Input handling
  InputSource last_input_source;
  char input_buffer[COMM_BUFFER_SIZE];
  uint16_t buffer_pos;
  
  // OTA management
  bool ota_enabled;
  bool ota_in_progress;
  uint32_t ota_progress_time;
  
  // Internal methods
  void transition_to(CommState new_state);
  void update_wifi_connection();
  void handle_telnet_clients();
  void cleanup_disconnected_clients();
  bool is_wifi_connected();
  void setup_ota();
  void handle_ota();
  
public:
  // Constructor/Destructor
  CommunicationManager(const char* ssid, const char* password);
  ~CommunicationManager();
  
  // Core interface
  void begin();
  void update();
  
  // Output methods (unified Debug interface)
  void println(const char* message);
  void println(const String& message);
  void printf(const char* format, ...);
  void print_status();
  
  // Input methods
  bool available();
  char read();
  InputSource get_input_source();
  void flush();
  
  // State queries
  CommState get_state() { return current_state; }
  bool is_wifi_available() { return WiFi.status() == WL_CONNECTED; }
  uint8_t get_client_count() { return active_clients; }
  const char* get_ip_address();
  
  // OTA management
  bool is_ota_enabled() { return ota_enabled; }
  bool is_ota_in_progress() { return ota_in_progress; }
  void enable_ota();
  void disable_ota();
  
  // Emergency access
  void emergency_serial_only();
};

//=============================================================================
// GLOBAL INSTANCE
//=============================================================================

// Global communication manager instance
extern CommunicationManager* Debug;

//=============================================================================
// INITIALIZATION FUNCTIONS
//=============================================================================

/**
 * @brief Initialize communication system with WiFi credentials
 * @param ssid WiFi network name
 * @param password WiFi password
 */
void communication_init(const char* ssid, const char* password);

/**
 * @brief Get communication status string
 */
String communication_get_status();

#endif // COMMUNICATION_H