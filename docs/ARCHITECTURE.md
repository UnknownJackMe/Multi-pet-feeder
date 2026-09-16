# Architecture

## Goal

Build a three-channel automatic feeder for a multi-pet household (2 cats + 1 dog) with pet-specific food access, anti-tailgating protection, local/offline operation, and measurable feeding outcomes.

## Core design

```text
Three independent food hoppers
  ├─ Sushi
  ├─ Coconut
  └─ Blueberry
        ↓
Independent dispenser motors
        ↓
Shared weighing bowl
        ↓
Feeding chamber
        ↑
Inner door
        ↑
Authentication / anti-tailgating chamber
  ├─ RFID identity
  ├─ multi-zone ToF occupancy
  ├─ floor load cell
  ├─ outer/inner beam sensors
  └─ optional camera
        ↑
Outer door
```

## Safety model

The system is fail-closed for food access and fail-safe for pet safety.

- Identity uncertainty -> inner door remains closed.
- Multiple-pet suspicion -> inner door remains closed.
- Doorway obstruction -> door opening/closing motion is inhibited or reversed.
- Door physical state must be confirmed by limit switches; actuator commands are never treated as state confirmation.
- Network loss must not stop scheduled feeding.
- Reboot must not cause duplicate feeding.

## Control architecture

ESP32-S3 is the authoritative local controller.

```text
ESP32-S3
  ├─ EventBus
  ├─ Pet identity service
  ├─ Presence fusion
  ├─ Access chamber FSM
  ├─ Door controller
  ├─ Hopper controller x3
  ├─ Bowl scale
  ├─ Chamber scale
  ├─ Scheduler / RTC
  ├─ Fault manager
  └─ MQTT / Web telemetry (non-critical)
```

The NUC/camera path is optional and advisory only. Core feeding and safety behavior must remain functional with Wi-Fi, MQTT, NUC and camera unavailable.

## Anti-tailgating logic

The chamber acts as an airlock. Outer and inner doors must never be open at the same time.

Nominal sequence:

1. Detect pet near outer door.
2. Read authorized RFID.
3. Open outer door.
4. Confirm pet entered chamber.
5. Confirm outer threshold is clear.
6. Close outer door and verify closed limit switch.
7. Re-evaluate chamber occupancy:
   - authorized identity only;
   - exactly one pet estimated;
   - chamber weight consistent with that pet;
   - no threshold obstruction.
8. Open inner door.
9. Confirm pet entered feeding chamber.
10. Close inner door.

Any ambiguous condition aborts access and returns to a safe recovery state.

## Sensor fusion

Primary signals:

- RFID tag identity.
- VL53L5CX 8x8 multi-zone ToF occupancy.
- Chamber floor load cell + HX711.
- Outer threshold beam.
- Inner threshold beam.
- Open/closed limit switches for both doors.

Optional signals:

- Camera pet detector / counter.
- NUC-side pet ReID.

Camera output must not be required for safety decisions in V1.

## Dispenser

Each hopper is independent:

```text
hopper -> auger/rotary mechanism -> chute -> weighing bowl
```

Closed-loop dispensing uses bowl mass, not motor time alone.

Suggested algorithm:

1. Fast dispense until target - coarse margin.
2. Slow or pulse dispense near target.
3. Stop at target tolerance.
4. Detect jam if motor motion occurs without expected mass increase.
5. Reverse briefly and retry.
6. Latch a fault after repeated unsuccessful retries.

## V1 hardware baseline

- ESP32-S3 N16R8
- DS3231 RTC
- NEMA17 x3
- TMC2209 x3
- 5 kg load cell + HX711 (bowl)
- 20 kg load cell + HX711 (authentication chamber)
- VL53L5CX x1
- IR/beam sensor x2
- RFID reader + 3 pet tags
- Servo/door actuator x2
- Door limit switches x4
- 12 V supply + 5 V regulator

## Open-source references

We will borrow concepts rather than copy a single project wholesale:

- Metropolia-Smart-Pet-Feeder/smart-pet-feeder: ESP32-S3 component architecture, EventBus, scale, RFID, motor, MQTT.
- Alex-ala/OpenCatFlap: passage sensing, direction-aware access logic, independent FSM concepts.
- Tech-RW/Catfeeder: multi-cat food separation, pet RFID, mechanical position verification.
- hb9ezs/meowpass: pet RFID hardware and power-noise considerations.
- s60sc/ESP32_RFID_Reader: 125/134.2 kHz RFID implementation and antenna considerations.
- Michael-Rolle/Arduino-Automatic-RFID-Cat-Feeder: homing, limit-switch based door state and power-loss recovery.
- Honeysad007/RFID-Cat-Feeder: multi-cat RFID lid logic, IR presence hold, quiet servo motion.

## V1 non-goals

- Native mobile app.
- Cloud-dependent control.
- Camera-required authorization.
- Automatic bowl washing.
- Fully productized enclosure.
- Prescription-food-grade zero cross-contamination guarantee.
