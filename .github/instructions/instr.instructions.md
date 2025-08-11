---
applyTo: '**'
---
# Copilot instructions for this repo

Purpose: Make AI agents productive quickly in this ESP32-S3 Arduino/PlatformIO hydroponics project. 

MUST OBEY THESE RULES:

Prefer concrete patterns from this codebase over generic advice. Use simplest working solutions. If you are unsure, ask for clarification. Use tools and MCP servers extensivel, but do not waste tokens. Do not apply random changes without understanding the context. Get things done.

Quick start (PlatformIO):
- Build: pio run
- Upload + Monitor (115200): pio run --target upload --target monitor
- Monitor only: pio device monitor --baud 115200
- Clean: pio run --target clean • Static analysis: pio check • Tests: pio test
- OTA (when WiFi connected): pio run --target upload --upload-port ESP32-Hydroponic.local

Architecture at a glance:
- src/main.cpp: Orchestrates startup (communication_init → state_machine_init → calibration_load → sensor_initialize → pump_init), then non-blocking loop. Implements the CLI command switch.
- include/communication.h, src/communication.cpp: CommunicationManager exposes global Debug (println/printf/available/read). Primary output is WiFi Telnet; Serial is always-on backup. OTA auto-enabled on WiFi.
- include/state_machine.h, src/state_machine.cpp: Central enum-based FSMs (SystemState, PumpState[4], SensorState, CalibrationState) with validated transitions, timing, and emergency stop.
- include/sensors.h, src/sensors.cpp: Sensor FSM cycles WARMING_UP → READING → FILTERING → READY. Controls power pins, reads DS18B20, ADC for pH/EC, HC-SR04 distance, applies EMA filtering, converts distance→volume via calibration.
- include/calibration.h, src/calibration.cpp: calibration_t persisted in NVS (Preferences). Interactive serial calibration for pH/EC/volume.
- include/pump.h, src/pump.cpp: LEDC PWM peristaltic control for 4 pumps, PID-based auto pH dosing, manual start/stop, safety (timeouts, spacing, per-hour limits). Uses PumpState machine.
- tasks (src/*_task.cpp, include/tasks/*): Optional FreeRTOS wrappers; disabled by default (ENABLE_*_TASK=0). Keep loop() non-blocking regardless.

Conventions and patterns (project-specific):
- C-style code with structs + free functions; minimal C++ features. Single source of truth for pins/timing in headers (include/*.h). If docs disagree, trust constants in headers.
- Logging: Prefer Debug->printf/println for new work (routes to Serial+Telnet). Legacy modules still use Serial directly; don’t refactor broadly unless asked.
- State gating: Only perform dosing/long actions in SystemState::MONITORING or ::DOSING. Use system_transition_to and pump_transition_to and respect is_valid_* checks.
- Non-blocking loops: Avoid delay() in main logic; use millis()-based durations and the existing FSM timing helpers. Sensor power on/off is handled by the sensor FSM; don’t duplicate.
- Safety first: Obey PUMP_MIN_DOSE_INTERVAL, PUMP_TIMEOUT_MS, and max doses/hour. Use pump_safety_check() via state_machine_update()—don’t bypass.

Communication + CLI tips:
- Unified IO: Debug->available()/read() gives highest-priority input (Serial over Telnet). Use this for CLI handlers.
- Typical CLI additions live in main.cpp’s switch(cmd). Keep handlers fast and non-blocking; print status via Debug.

Calibration + persistence:
- Preferences is created in main.cpp then used by calibration.cpp (NVS namespace in include/sensors.h as NVS_NAMESPACE). Use calibration global for pH/EC/volume math.
- Volume mapping uses calibration_distance_to_volume(distance) with 3-point interpolation.

Pump control rules you must follow:
- To start a dose: compute ml and flow, then start via internal helpers (auto pH uses calculate_pid_dose; manual uses pump_manual_dose or pump_start_manual). All starts happen only from PumpState::IDLE.
- Manual run example: pump_start_manual(PumpId::PH_UP, 30.0f) and stop via pump_stop_manual(). Auto pH target via pump_set_ph_target().

Testing & debugging:
- Unit test example lives in test/test_communication.cpp (Unity). On real hardware, WiFi may cycle SERIAL_ONLY/WIFI_CONNECTING—tests allow either.
- For periodic state insight, enable state machine debug and use state_machine_print_status().

Common pitfalls (here):
- Mixed logging (Serial vs Debug). Use Debug in new code; leave existing Serial unless scoped refactor requested.
- LEDC API: This project uses Arduino-ESP32 3.x style ledcAttach(pin,freq,res) and ledcWrite(pin,duty) with 8-bit resolution. Don’t revert to old channel-based API.

Good examples:
- Non-blocking read→dose flow: src/main.cpp (loop()) and src/ph_task.cpp.
- FSM transitions and safety: src/state_machine.cpp and pump_safety_check() in src/pump.cpp.
