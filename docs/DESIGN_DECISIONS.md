# Design Decisions

This document records the architectural decisions for V1 and how external open-source projects influence the design.

## Licensing rule

This repository is Apache-2.0 licensed.

Several reference projects use GPL-family licenses. Therefore:

- use those projects to understand architecture, state-machine structure, hardware lessons and failure modes;
- do not copy GPL source files or substantial implementation text into this repository;
- reimplement interfaces and algorithms independently;
- record the reference that motivated a design decision where useful;
- before importing any third-party library, verify its license separately.

This is deliberate because Apache-2.0 must not silently become a derivative GPL work through copied implementation code.

## D-001: ESP32-S3 is the authoritative controller

**Decision:** Core feeding and access-control logic runs locally on ESP32-S3 using ESP-IDF/C++.

**Influence:** Metropolia Smart Pet Feeder demonstrates a componentized ESP32-S3 feeder with event-driven integration.

**Reasoning:**

- deterministic local behavior;
- no dependency on Wi-Fi, MQTT, NUC or cloud availability;
- enough GPIO/peripherals for the V1 sensor set;
- native FreeRTOS/ESP event-loop support.

Camera/NUC output is advisory in V1.

## D-002: Event-driven components, explicit safety coordinator

**Decision:** Drivers publish typed events; domain logic owns decisions.

```text
sensor/driver components
        ↓ events
presence fusion / identity / door feedback
        ↓
access-control FSM
        ↓ commands
door controller / feeder controller
```

The event bus is not allowed to hide safety ownership. Only the access-control FSM may authorize opening the inner door.

**Influence:** Metropolia's EventBus/component split.

## D-003: Two-door authentication chamber

**Decision:** Food access uses an airlock-like chamber with an outer door and inner door.

**Hard invariant:** both doors may never be physically open at the same time.

**Influence:** OpenCatFlap's independent passage sensing and directional state-machine approach, adapted to a feeder airlock.

**Reasoning:** A single RFID-controlled lid does not solve tailgating. Physical separation creates a verification interval before food becomes reachable.

## D-004: Fail-closed authorization

**Decision:** Any disagreement among critical identity/occupancy signals blocks the inner door.

V1 critical evidence:

- authorized RFID identity;
- outer door physically closed;
- inner door physically closed before opening;
- outer and inner threshold beams clear;
- chamber occupancy estimate consistent with one pet;
- chamber weight plausible for the authorized pet;
- no critical sensor fault.

No majority vote is used for these critical checks.

## D-005: Door state is measured, not assumed

**Decision:** Servo/actuator commands do not define physical door state. Open/closed limit switches do.

**Influence:** Tech-RW/Catfeeder's mechanical position verification and Michael-Rolle's homing/limit-switch recovery pattern.

**Reasoning:** Actuators can stall, slip or lose position after power failure.

## D-006: Boot performs homing and plausibility checks

**Decision:** On power-up:

1. initialize drivers;
2. validate sensor communications;
3. establish physical door state using limit switches;
4. home each door to a known safe state;
5. tare/validate scales where safe;
6. restore meal ledger;
7. only then enter IDLE.

No access session is resumed from a persisted transient door state.

## D-007: RFID implementation is abstracted

**Decision:** Domain logic consumes normalized `PetTagObserved` events and does not depend on RC522/WL-134/RDM6300 specifics.

Initial hardware preference is a pet-oriented 125/134.2 kHz reader with an antenna geometry validated on the real animals.

**Influence:** MeowPass and s60sc/ESP32_RFID_Reader.

**Reasoning:** Reader range, antenna geometry and supply noise are larger practical risks than application-level parsing.

## D-008: RFID power is isolated from motor noise

**Decision:** The electrical design separates motor/actuator power from low-noise RFID/logic supply paths, with local filtering and star-style power distribution where practical.

**Influence:** MeowPass documents strong RFID range degradation from noisy power and uses a dedicated clean RFID rail.

For prototype V1, exact rail voltages depend on the selected reader, but the isolation principle is fixed.

## D-009: Presence fusion is independent of identity

**Decision:** Identity and occupancy are separate concepts.

```text
RFID -> who might be present
ToF / beams / chamber scale -> how many / where / whether passage is clear
```

A valid RFID never implies `occupancy == 1`.

## D-010: Camera is not a V1 safety dependency

**Decision:** Camera/YOLO may log, count pets and provide diagnostic evidence, but loss of camera must not prevent safe feeder operation.

The V1 authorization boundary uses deterministic local sensors.

## D-011: Three independent food channels, one weighed bowl

**Decision:** V1 has three hoppers with independent motors feeding one shared bowl.

Closed-loop target mass is measured by the bowl load cell.

This does **not** guarantee zero cross-contamination. If prescription-food-grade isolation becomes a requirement, V2 must use physically isolated bowls/chutes or an exchangeable bowl mechanism.

## D-012: Closed-loop dispensing

**Decision:** Do not control portions by motor runtime alone.

Algorithm:

1. tare/stabilize bowl;
2. coarse dispense;
3. slow/pulse near target;
4. stop inside tolerance;
5. detect no-mass-gain jam;
6. reverse and retry a bounded number of times;
7. latch fault on repeated failure.

**Influence:** feeder projects that combine stepper dispensing with measured food mass; Tech-RW also reinforces the rule that actuator steps alone are not trustworthy physical-state evidence.

## D-013: Feeding ledger prevents duplicate portions

Persist at least:

- meal/session ID;
- pet ID;
- food channel;
- target mass;
- mass already dispensed;
- session state;
- completion timestamp.

After reboot, the controller reconciles physical state and ledger before dispensing again.

## D-014: Safety logic must be host-testable

**Decision:** The access-control policy is written as a hardware-independent state machine with plain data inputs and commands as outputs.

ESP-IDF drivers are adapters around this core.

This allows deterministic tests for:

- normal authorized passage;
- wrong pet;
- two-pet tailgating;
- doorway obstruction;
- contradictory limit switches;
- sensor loss;
- reboot/power interruption;
- actuator timeout.

**Influence:** Catcierge's explicit FSM/testing separation and general embedded safety practice.
