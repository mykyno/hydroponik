/**
 * @file communication.cpp
 * @brief Implementation of hybrid WiFi/Serial communication manager
 */

#include "communication.h"
#include <stdarg.h>

//=============================================================================
// GLOBAL INSTANCE
//=============================================================================

CommunicationManager* Debug = nullptr;

//=============================================================================
// CONSTRUCTOR/DESTRUCTOR
//=============================================================================

CommunicationManager::CommunicationManager(const char* ssid, const char* password) 
  : wifi_ssid(ssid), wifi_password(password), current_state(CommState::SERIAL_ONLY),
    last_wifi_attempt(0), state_change_time(0), telnet_server(nullptr),
    active_clients(0), last_input_source(InputSource::NONE), buffer_pos(0),
    ota_enabled(false), ota_in_progress(false), ota_progress_time(0) {
  
  // Clear client array
  for (int i = 0; i < MAX_TELNET_CLIENTS; i++) {
    telnet_clients[i] = WiFiClient();
  }
  
  // Clear input buffer
  memset(input_buffer, 0, COMM_BUFFER_SIZE);
}

CommunicationManager::~CommunicationManager() {
  if (telnet_server) {
    delete telnet_server;
    telnet_server = nullptr;
  }
}

//=============================================================================
// CORE INTERFACE
//=============================================================================

void CommunicationManager::begin() {
  // Serial is always initialized first and remains as backup
  if (!Serial) {
    Serial.begin(115200);
    delay(100);
  }
  
  transition_to(CommState::SERIAL_ONLY);
  
  // Initialize WiFi in station mode
  WiFi.mode(WIFI_STA);
  
  Serial.println("Communication Manager initialized - Serial active");
  
  // Start first WiFi connection attempt
  last_wifi_attempt = millis();
  WiFi.begin(wifi_ssid, wifi_password);
  transition_to(CommState::WIFI_CONNECTING);
}

void CommunicationManager::update() {
  uint32_t now = millis();
  
  // Update WiFi connection status
  update_wifi_connection();
  
  // Handle telnet clients if WiFi is active
  if (current_state == CommState::WIFI_PRIMARY) {
    handle_telnet_clients();
  }
  
  // Handle OTA if enabled and WiFi connected
  if (ota_enabled && is_wifi_connected()) {
    handle_ota();
  }
  
  // State-specific updates
  switch (current_state) {
    case CommState::WIFI_CONNECTING:
      // Check for timeout or success
      if (WiFi.status() == WL_CONNECTED) {
        // Create telnet server
        if (!telnet_server) {
          telnet_server = new WiFiServer(TELNET_PORT);
          telnet_server->begin();
        }
        
        // Enable OTA automatically when WiFi connects
        if (!ota_enabled) {
          setup_ota();
          ota_enabled = true;
        }
        
        transition_to(CommState::WIFI_PRIMARY);
      } else if (now - last_wifi_attempt > WIFI_CONNECT_TIMEOUT_MS) {
        // Connection timeout - fall back to serial only
        WiFi.disconnect();
        transition_to(CommState::SERIAL_ONLY);
      }
      break;
      
    case CommState::SERIAL_ONLY:
      // Periodically retry WiFi connection
      if (now - last_wifi_attempt > WIFI_RETRY_INTERVAL_MS) {
        last_wifi_attempt = now;
        WiFi.begin(wifi_ssid, wifi_password);
        transition_to(CommState::WIFI_CONNECTING);
      }
      break;
      
    case CommState::WIFI_PRIMARY:
      // Check for WiFi disconnection
      if (!is_wifi_connected()) {
        transition_to(CommState::SERIAL_ONLY);
      }
      break;
      
    default:
      break;
  }
}

//=============================================================================
// OUTPUT METHODS (UNIFIED DEBUG INTERFACE)
//=============================================================================

void CommunicationManager::println(const char* message) {
  uint32_t now = millis();
  
  // Always output to Serial (backup/emergency access)
  if (current_state == CommState::SERIAL_ONLY) {
    Serial.printf("[%lu] %s [Serial]\n", now, message);
  } else {
    Serial.printf("[%lu] %s [WiFi]\n", now, message);
  }
  
  // Output to telnet clients if WiFi is available
  if (current_state == CommState::WIFI_PRIMARY) {
    String telnet_msg = String("[") + String(now) + String("] ") + String(message) + String("\r\n");
    
    for (int i = 0; i < MAX_TELNET_CLIENTS; i++) {
      if (telnet_clients[i] && telnet_clients[i].connected()) {
        telnet_clients[i].print(telnet_msg);
      }
    }
  }
}

void CommunicationManager::println(const String& message) {
  println(message.c_str());
}

void CommunicationManager::printf(const char* format, ...) {
  char buffer[512];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  
  println(buffer);
}

void CommunicationManager::print_status() {
  String status = communication_get_status();
  println(status.c_str());
}

//=============================================================================
// INPUT METHODS
//=============================================================================

bool CommunicationManager::available() {
  // Serial input has highest priority (emergency access)
  if (Serial.available()) {
    last_input_source = InputSource::SERIAL_USB;
    return true;
  }
  
  // Check telnet clients for input
  if (current_state == CommState::WIFI_PRIMARY) {
    for (int i = 0; i < MAX_TELNET_CLIENTS; i++) {
      if (telnet_clients[i] && telnet_clients[i].connected() && telnet_clients[i].available()) {
        last_input_source = InputSource::TELNET_CLIENT;
        return true;
      }
    }
  }
  
  last_input_source = InputSource::NONE;
  return false;
}

char CommunicationManager::read() {
  // Priority: Serial first, then Telnet
  if (last_input_source == InputSource::SERIAL_USB && Serial.available()) {
    return Serial.read();
  }
  
  if (last_input_source == InputSource::TELNET_CLIENT) {
    for (int i = 0; i < MAX_TELNET_CLIENTS; i++) {
      if (telnet_clients[i] && telnet_clients[i].connected() && telnet_clients[i].available()) {
        return telnet_clients[i].read();
      }
    }
  }
  
  return 0;
}

InputSource CommunicationManager::get_input_source() {
  return last_input_source;
}

void CommunicationManager::flush() {
  Serial.flush();
  
  if (current_state == CommState::WIFI_PRIMARY) {
    for (int i = 0; i < MAX_TELNET_CLIENTS; i++) {
      if (telnet_clients[i] && telnet_clients[i].connected()) {
        // clear() is the modern equivalent to flush for network clients
        telnet_clients[i].clear();
      }
    }
  }
}

//=============================================================================
// STATE MANAGEMENT
//=============================================================================

void CommunicationManager::transition_to(CommState new_state) {
  if (current_state != new_state) {
    current_state = new_state;
    state_change_time = millis();
    
    // State entry actions
    switch (new_state) {
      case CommState::SERIAL_ONLY:
        Serial.println("Communication: Serial Only mode");
        break;
      case CommState::WIFI_CONNECTING:
        Serial.println("Communication: Connecting to WiFi...");
        break;
      case CommState::WIFI_PRIMARY:
        Serial.printf("Communication: WiFi Connected (%s) - Telnet active on port %d\n", 
                     WiFi.localIP().toString().c_str(), TELNET_PORT);
        break;
      
      case CommState::ERROR:
        Serial.println("Communication: Error state");
        break;
    }
  }
}

void CommunicationManager::update_wifi_connection() {
  // Monitor WiFi status changes
  static wl_status_t last_wifi_status = WL_IDLE_STATUS;
  wl_status_t current_wifi_status = WiFi.status();
  
  if (current_wifi_status != last_wifi_status) {
    switch (current_wifi_status) {
      case WL_CONNECTED:
        if (current_state == CommState::WIFI_CONNECTING) {
          // Will transition to WIFI_PRIMARY in main update loop
        }
        break;
      case WL_DISCONNECTED:
      case WL_CONNECTION_LOST:
  if (current_state == CommState::WIFI_PRIMARY) {
          transition_to(CommState::SERIAL_ONLY);
        }
        break;
      default:
        break;
    }
    last_wifi_status = current_wifi_status;
  }
}

void CommunicationManager::handle_telnet_clients() {
  if (!telnet_server) return;
  
  // Check for new client connections (use accept to avoid deprecation)
  WiFiClient new_client = telnet_server->accept();
  if (new_client) {
    // Find empty slot for new client
    bool client_added = false;
    for (int i = 0; i < MAX_TELNET_CLIENTS; i++) {
      if (!telnet_clients[i] || !telnet_clients[i].connected()) {
        telnet_clients[i] = new_client;
        telnet_clients[i].println("ESP32-S3 Hydroponic System - Telnet Interface");
        telnet_clients[i].println("Type 'q' for pump status, 'x' for emergency stop");
        client_added = true;
        break;
      }
    }
    
    if (!client_added) {
      // No available slots
      new_client.println("Server full - try again later");
      new_client.stop();
    }
  }
  
  // Clean up disconnected clients
  cleanup_disconnected_clients();
}

void CommunicationManager::cleanup_disconnected_clients() {
  active_clients = 0;
  for (int i = 0; i < MAX_TELNET_CLIENTS; i++) {
    if (telnet_clients[i]) {
      if (telnet_clients[i].connected()) {
        active_clients++;
      } else {
        telnet_clients[i].stop();
        telnet_clients[i] = WiFiClient();
      }
    }
  }
}

bool CommunicationManager::is_wifi_connected() {
  return WiFi.status() == WL_CONNECTED;
}

const char* CommunicationManager::get_ip_address() {
  static String ip_str;
  if (is_wifi_connected()) {
    ip_str = WiFi.localIP().toString();
    return ip_str.c_str();
  }
  return "Not connected";
}

void CommunicationManager::emergency_serial_only() {
  // Force serial-only mode for emergency access
  WiFi.disconnect();
  ota_enabled = false;
  ArduinoOTA.end();
  transition_to(CommState::SERIAL_ONLY);
  Serial.println("EMERGENCY: Forced to Serial-only mode");
}

//=============================================================================
// OTA IMPLEMENTATION
//=============================================================================

void CommunicationManager::setup_ota() {
  // Configure OTA hostname and authentication
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPort(OTA_PORT);
  
  // OTA event callbacks with dual output
  ArduinoOTA.onStart([this]() {
    ota_in_progress = true;
    ota_progress_time = millis();
    
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
    println(("OTA Update Started: " + type).c_str());
    println("Serial interface remains active during update");
  });
  
  ArduinoOTA.onEnd([this]() {
    ota_in_progress = false;
    println("OTA Update Complete - Rebooting...");
  });
  
  ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
    uint32_t now = millis();
    // Report progress every 2 seconds to avoid spam
    if (now - ota_progress_time > 2000) {
      ota_progress_time = now;
      int percent = (progress / (total / 100));
      printf("OTA Progress: %d%% (%u/%u bytes)", percent, progress, total);
    }
  });
  
  ArduinoOTA.onError([this](ota_error_t error) {
    ota_in_progress = false;
    String error_msg = "OTA Error: ";
    
    switch (error) {
      case OTA_AUTH_ERROR:
        error_msg += "Authentication Failed";
        break;
      case OTA_BEGIN_ERROR:
        error_msg += "Begin Failed";
        break;
      case OTA_CONNECT_ERROR:
        error_msg += "Connect Failed";
        break;
      case OTA_RECEIVE_ERROR:
        error_msg += "Receive Failed";
        break;
      case OTA_END_ERROR:
        error_msg += "End Failed";
        break;
      default:
        error_msg += "Unknown Error";
        break;
    }
    
    println(error_msg.c_str());
    println("OTA Update Failed - System continues normally");
    println("Serial interface operational");
  });
  
  ArduinoOTA.begin();
  println("OTA Update service started");
  printf("OTA Hostname: %s | Port: %d", OTA_HOSTNAME, OTA_PORT);
}

void CommunicationManager::handle_ota() {
  // Process OTA operations (non-blocking)
  ArduinoOTA.handle();
}

void CommunicationManager::enable_ota() {
  if (is_wifi_connected() && !ota_enabled) {
    setup_ota();
    ota_enabled = true;
    println("OTA manually enabled");
  } else if (!is_wifi_connected()) {
    println("OTA requires WiFi connection");
  } else {
    println("OTA already enabled");
  }
}

void CommunicationManager::disable_ota() {
  if (ota_enabled) {
    ArduinoOTA.end();
    ota_enabled = false;
    ota_in_progress = false;
    println("OTA disabled");
  } else {
    println("OTA already disabled");
  }
}

//=============================================================================
// GLOBAL FUNCTIONS
//=============================================================================

void communication_init(const char* ssid, const char* password) {
  if (Debug) {
    delete Debug;
  }
  
  Debug = new CommunicationManager(ssid, password);
  Debug->begin();
}

String communication_get_status() {
  if (!Debug) return "Communication not initialized";
  
  String status = "Communication Status: ";
  
  switch (Debug->get_state()) {
    case CommState::SERIAL_ONLY:
      status += "Serial Only";
      break;
    case CommState::WIFI_CONNECTING:
      status += "WiFi Connecting...";
      break;
    case CommState::WIFI_PRIMARY:
      status += "WiFi Primary (";
      status += Debug->get_ip_address();
      status += ") | Telnet: ";
      status += Debug->get_client_count();
      status += " clients | Serial: Backup";
      if (Debug->is_ota_enabled()) {
        status += " | OTA: ";
        status += Debug->is_ota_in_progress() ? "Updating" : "Ready";
      }
      break;
    case CommState::ERROR:
      status += "Error";
      break;
  }
  
  return status;
}