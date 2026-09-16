# Firmware

ESP32-S3 firmware for the multi-pet feeder.

## Current scope

The repository currently contains the hardware-independent safety/domain core first:

- `core_types` — shared domain data types;
- `presence_fusion` — conservative fusion of ToF, chamber mass and threshold beams;
- `access_control` — two-door anti-tailgating state machine;
- `main` — minimal ESP-IDF application bootstrap.

Hardware drivers are intentionally added after interfaces and safety behavior are testable.

## Design rule

```text
hardware driver
    ↓ observation/event
hardware-independent domain core
    ↓ explicit command
door / hopper actuator adapter
```

RFID, ToF and load-cell drivers must not directly open doors or run hopper motors.

## ESP-IDF build

From an ESP-IDF shell:

```bash
cd firmware
idf.py set-target esp32s3
idf.py build
```

Flashing will be documented once the exact ESP32-S3 board/pin map is frozen.

## Native domain tests

No ESP-IDF installation is required for the host tests:

```bash
cmake -S host_tests -B build/host_tests -DCMAKE_BUILD_TYPE=Release
cmake --build build/host_tests --parallel
ctest --test-dir build/host_tests --output-on-failure
```

These tests are also configured in GitHub Actions.

## Planned component order

1. domain core + host tests;
2. door feedback/actuator adapter;
3. beam sensor adapter;
4. chamber HX711 adapter;
5. VL53L5CX adapter;
6. RFID adapter;
7. pet identity/config storage;
8. bowl HX711 adapter;
9. TMC2209 hopper control;
10. scheduler/RTC and feeding ledger;
11. MQTT/Web telemetry;
12. optional camera/NUC integration.

## Reference implementation policy

The design is informed by projects listed in `docs/REFERENCES.md`, but implementation code is written independently to preserve this repository's Apache-2.0 licensing unless a dependency is explicitly reviewed and documented.
