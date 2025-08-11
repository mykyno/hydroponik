# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Context

This is an iterative hydroponic system development project for ESP32-S3. We build the system part by part, expanding functionality gradually. The focus is on clear, maintainable code without overengineering.

**Current Status**: Phase 3 hybrid communication system with WiFi/Telnet primary, Serial backup. Manual pump control operational.

## Development Philosophy & Code Style

- **Iterative approach**: Build system component by component, test each part
- **C-style syntax**: Use C syntax patterns, avoid C++ complexity where possible
- **Simple and clear**: No overengineering, prioritize readability and maintainability
- **Future FreeRTOS ready**: Architecture should accommodate task-based design later
- **Hardware-focused**: This is embedded code for real sensor hardware

## Essential Commands

```bash
# Core development cycle
pio run                                    # Build and check compilation
pio run --target upload --target monitor  # Deploy and monitor (115200 baud)

# Utility commands
pio run --target clean                     # Clean build
pio check                                  # Static analysis
pio device monitor --baud 115200          # Monitor only
```

## Code Architecture Rules

### File Organization
- **main.cpp**: Arduino setup()/loop(), system orchestration, CLI handling, WiFi credentials
- **sensors.h/.cpp**: All sensor reading logic, power management, filtering, DS18B20 temperature
- **calibration.h/.cpp**: NVS storage, calibration algorithms with factory defaults
- **pump.cpp**: Peristaltic pump control system with PID and manual control (no header found)
- **state_machine.h/.cpp**: Hardware-level state management for system transitions
- **communication.h/.cpp**: Hybrid WiFi/Serial manager, Telnet server, OTA updates

### Key Design Patterns
1. **State-Based System**: Hardware-level state machine manages system transitions and safety
2. **Hybrid Communication**: WiFi/Telnet primary with Serial backup, automatic failover
3. **Power Management**: Sensors powered only during reading cycles
4. **Non-blocking Operations**: Use timing checks, avoid delay() in main loop  
5. **NVS Persistence**: Store calibration data in ESP32 NVS for reboot survival
6. **Temperature Compensation**: Adjust pH/EC readings based on water temperature via DS18B20
7. **Multi-sample Filtering**: Average multiple readings to reduce noise
8. **Safety-First Pumps**: Multiple safety layers prevent over-dosing
9. **OTA Updates**: Over-the-air firmware updates via ArduinoOTA when WiFi connected

### Hardware Configuration (ESP32-S3)
```
# Sensors (Phase 1)
pH Sensor:     GPIO6 (analog) + GPIO7 (power control)
EC Sensor:     GPIO5 (analog) + GPIO4 (power control) 
Temperature:   GPIO10 (DS18B20 1-Wire)
Ultrasonic:    GPIO8 (trigger) + GPIO9 (echo)

# Pump System (Phase 2)
pH Up Pump:    GPIO11 (PWM Channel 0)
pH Down Pump:  GPIO12 (PWM Channel 1) 
Nutrient A:    GPIO13 (PWM Channel 2)
Nutrient B:    GPIO14 (PWM Channel 3)

# Communication (Phase 3)
Serial USB:    115200 baud (always available as backup)
WiFi:          Primary interface with Telnet server (port 23)
OTA Updates:   Port 3232, hostname "ESP32-Hydroponic"
```

### CLI Interface
Runtime commands via hybrid interface (Serial USB backup + WiFi Telnet primary):

**Calibration Commands:**
- `s` - Show current calibration parameters
- `r` - Reset calibration to factory defaults  
- `p` - Perform pH 2-point calibration routine
- `e` - Perform EC 2-point calibration routine
- `v` - Perform volume calibration routine

**Automatic pH Control (Phase 1):**
- `a` - Toggle automatic pH control ON/OFF
- `t` - Set pH target (5.0-8.0 range)
- `q` - Show pump system status (all 4 pumps)
- `m` - Manual dose (pH Up/Down with volume)

**Manual Pump Control (Phase 2):**
- `1` - Start pH Up pump at specified flow rate (10-90 ml/min)
- `2` - Start pH Down pump at specified flow rate (10-90 ml/min)
- `3` - Start Nutrient A pump at specified flow rate (10-90 ml/min)  
- `4` - Start Nutrient B pump at specified flow rate (10-90 ml/min)
- `x` - Emergency stop all pumps
- `z` - Stop specific pump (prompts for pump selection)

## Important Implementation Guidelines

### When Writing Code:
1. **Follow existing patterns** - Look at current sensor reading structure
2. **Use C-style** - Prefer structs, functions, avoid classes when possible
3. **Handle //CLAUDE: comments** - These mark specific implementation requests
4. **Maintain power efficiency** - Follow existing power on/off patterns
5. **Add proper error handling** - Check sensor validity, handle disconnections
6. **Use consistent naming** - Follow sensor_*, calibration_*, pump_* function naming
7. **Respect safety limits** - Pump system has multiple safety mechanisms

### Dependencies & Build Configuration
```ini
# Platform & Board (platformio.ini)
platform = https://github.com/pioarduino/platform-espressif32/releases/download/51.03.05/platform-espressif32.zip
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200

# Required Libraries
lib_deps =
  adafruit/Adafruit Unified Sensor@^1.1.4  # Base sensor abstraction
  paulstoffregen/OneWire@^2.3.7            # DS18B20 communication protocol  
  milesburton/DallasTemperature@^3.9.0     # DS18B20 temperature sensor
```

### Key Constants & Timing
- `SENSOR_INTERVAL`: 5000ms between sensor readings
- `SENSOR_WARMUP`: 200ms for sensor stabilization after power on
- `FILTER_SAMPLES`: 5 samples for noise reduction averaging
- EC temperature compensation: 0.02 per °C
- `PUMP_MIN_DOSE_INTERVAL`: 300000ms (5 minutes) between automatic doses
- `PUMP_TIMEOUT_MS`: 600000ms (10 minutes) maximum pump runtime
- `PUMP_PWM_FREQ`: 1000Hz PWM frequency for pump control

### Data Structures
- `sensor_readings_t`: Contains pH, EC, temperature, distance with timestamp and validity
- `calibration_t`: Slope/offset parameters for pH and EC sensors  
- `pump_t`: Individual pump state including PID controller and safety tracking
- `pump_system_t`: Global pump system state
- `SystemState` enum: Hardware-level system states (STARTUP, INITIALIZING, OPERATIONAL, etc.)
- `PumpState` enum: Individual pump operation states with safety transitions
- All readings include validity flags for error handling

### Pump System Architecture
- **4-pump system**: pH Up, pH Down, Nutrient A, Nutrient B
- **Dual operation modes**: Automatic PID control (pH only) + Manual control (all pumps)
- **Safety mechanisms**: Dose limits, timing restrictions, flow rate validation, emergency stops
- **PWM control**: 8-bit resolution, 1kHz frequency, flow rates 10-90 ml/min
- **Volume scaling**: All automatic doses proportional to reservoir volume

### Communication System Architecture  
- **Hybrid Interface**: WiFi Telnet primary, Serial USB always available as backup
- **Auto-failover**: Seamless switching between communication methods based on WiFi status
- **Multi-client Telnet**: Up to 3 concurrent Telnet connections supported (port 23)
- **OTA Updates**: Over-the-air firmware updates when WiFi connected (ArduinoOTA)
- **WiFi Credentials**: Hardcoded in main.cpp (WIFI_SSID, WIFI_PASSWORD constants)
- **Connection Management**: Non-blocking WiFi connection with 10s timeout, 30s retry intervals

This system is designed for real-world deployment in hydroponic monitoring, prioritizing reliability and maintainability over complexity.

## Development Best Practices

- **Documentation**: Update documentation files whenever making changes
- **Safety first**: Never bypass pump safety mechanisms
- **Test thoroughly**: Always run `pio run` and `pio check` before deployment
- **Monitor operation**: Use `q` command frequently to check pump status
- **Emergency procedures**: Know how to use `x` and `z` commands to stop pumps
- **WiFi Configuration**: Update WiFi credentials in main.cpp for target deployment network
- **Communication Testing**: Verify both Serial and Telnet interfaces during development

## Development Notes

- **Missing pump.h**: pump.cpp exists but corresponding header file missing from include/ directory
- **GPIO Pin Updates**: Actual GPIO assignments in source code differ from original documentation
- **Test Structure**: test/ directory contains test_communication.cpp for communication system validation
- **WiFi Credentials**: Currently hardcoded - consider environment variables for production deployment 