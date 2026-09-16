# Roadmap

## Phase 0 — Architecture freeze

Deliverables:

- architecture baseline
- V1 hardware baseline
- anti-tailgating FSM
- open-source reference map
- safety invariants

Exit criteria:

- no unresolved contradiction between door, sensor and feeding logic

## Phase 1 — Bench prototype

Build on the desk before constructing the full enclosure.

Targets:

- read all 3 RFID tags reliably
- read VL53L5CX occupancy data
- calibrate bowl and chamber load cells
- drive both door actuators with limit-switch confirmation
- drive all 3 feeder motors
- implement event logging

Exit criteria:

- all hardware can be exercised independently
- no actuator state is inferred without physical feedback

## Phase 2 — Access chamber prototype

Targets:

- implement outer/inner door interlock
- implement the access FSM
- test authorized entry
- test wrong-pet rejection
- test tailgating rejection
- test blocked-door recovery
- test reboot/homing recovery

Exit criteria:

- 100 consecutive tailgating test attempts produce zero unauthorized inner-door opens
- obstruction tests never continue closing into an occupied doorway

## Phase 3 — Three-channel dispensing

Targets:

- independent hopper control
- weight-closed-loop dispensing
- coarse/fine dispense control
- jam detection and reverse retry
- persistent meal accounting

Exit criteria:

- repeated target portions meet agreed accuracy
- reboot does not duplicate an already dispensed meal

## Phase 4 — Integrated feeder

Targets:

- combine access chamber and dispenser
- per-pet meal policy
- local RTC scheduling
- offline operation
- telemetry and fault reporting

Exit criteria:

- full feeding cycle works with Wi-Fi disconnected
- wrong pet cannot reach another pet's food through normal or tailgating paths

## Phase 5 — Optional vision

Targets:

- USB camera + NUC
- YOLO pet count
- session video/event capture
- optional ReID experiments

Constraint:

- vision remains advisory unless a later architecture review explicitly promotes it into the safety chain

## Phase 6 — Productization

Targets:

- custom PCB
- improved door mechanism
- food-safe mechanical redesign
- quieter actuators
- backup power
- serviceability and cleaning
- enclosure industrial design
