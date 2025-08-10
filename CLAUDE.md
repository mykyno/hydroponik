# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Context

This is an iterative hydroponic system development project for ESP32-S3. We build the system part by part, expanding functionality gradually. The focus is on clear, maintainable code without overengineering.

**Current Status**: Phase 2 manual pump control system implemented and operational.

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
- **main.cpp**: Arduino setup()/loop(), system orchestration, CLI handling
- **sensors.h/.cpp**: All sensor reading logic, power management, filtering
- **calibration.h/.cpp**: NVS storage, calibration algorithms
- **pump.h/.cpp**: Peristaltic pump control system with PID and manual control
- **state_machine.h/.cpp**: Hardware-level state management for system transitions

### Key Design Patterns
1. **State-Based System**: Hardware-level state machine manages system transitions and safety
2. **Power Management**: Sensors powered only during reading cycles
3. **Non-blocking Operations**: Use timing checks, avoid delay() in main loop  
4. **NVS Persistence**: Store calibration data in ESP32 NVS for reboot survival
5. **Temperature Compensation**: Adjust pH/EC readings based on water temperature
6. **Multi-sample Filtering**: Average multiple readings to reduce noise
7. **Safety-First Pumps**: Multiple safety layers prevent over-dosing

### Hardware Configuration (ESP32-S3)
```
# Sensors
pH Sensor:     GPIO5 (analog) + GPIO7 (power control)
EC Sensor:     GPIO6 (analog) + GPIO8 (power control) 
Temperature:   GPIO4 (DS18B20 1-Wire)
Ultrasonic:    GPIO9 (trigger) + GPIO10 (echo)

# Pump System (Phase 2)
pH Up Pump:    GPIO11 (PWM Channel 0)
pH Down Pump:  GPIO12 (PWM Channel 1) 
Nutrient A:    GPIO13 (PWM Channel 2)
Nutrient B:    GPIO14 (PWM Channel 3)

# Communication
Serial:        115200 baud for CLI and debugging
```

### CLI Interface
Runtime commands via serial (115200 baud):

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

### Dependencies (platformio.ini)
```
adafruit/Adafruit Unified Sensor@^1.1.4
paulstoffregen/OneWire@^2.3.7  
milesburton/DallasTemperature@^3.9.0
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

This system is designed for real-world deployment in hydroponic monitoring, prioritizing reliability and maintainability over complexity.

## Development Best Practices

- **Documentation**: Update documentation files whenever making changes
- **Safety first**: Never bypass pump safety mechanisms
- **Test thoroughly**: Always run `pio run` and `pio check` before deployment
- **Monitor operation**: Use `q` command frequently to check pump status
- **Emergency procedures**: Know how to use `x` and `z` commands to stop pumps

## Memories

- `/clear` command added to project memory tracking
- Use all necessary tools and agents for this task
- Use all neccesary tools and agents, mcp servers 