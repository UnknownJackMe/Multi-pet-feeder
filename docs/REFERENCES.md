# Open-source Reference Map

This project intentionally combines ideas from several open-source projects rather than depending on one codebase.

## 1. Metropolia-Smart-Pet-Feeder/smart-pet-feeder

Use for:

- ESP32-S3 / ESP-IDF project structure
- component-oriented hardware drivers
- EventBus-style decoupling
- HX711 bowl scale integration
- RFID component boundaries
- motor control integration
- MQTT / provisioning / local UI patterns

Do not copy blindly:

- its RC522/13.56 MHz RFID choice is not ideal for our pet-access geometry
- our access-control safety model is stricter

## 2. Alex-ala/OpenCatFlap

Use for:

- passage-control thinking
- side-specific state machines
- direction-aware sensing
- light-barrier / doorway sensing
- avoiding simplistic `RFID -> open` logic

Adaptation:

- our system uses two physical doors with an authentication chamber
- the inner door is the food-security boundary

## 3. Tech-RW/Catfeeder

Use for:

- multi-pet diet isolation
- pet RFID concepts
- mechanical position verification
- avoiding assumptions that actuator commands equal physical state

## 4. hb9ezs/meowpass

Use for:

- pet RFID hardware implementation
- power-rail separation
- read-range sensitivity to electrical noise
- PCB and antenna design ideas

## 5. s60sc/ESP32_RFID_Reader

Use for:

- 125 kHz / 134.2 kHz RFID implementation
- FDX-B implanted microchip support
- antenna tuning / range considerations

Potential future direction:

- support both collar tags and implanted pet microchips through a backend interface

## 6. Michael-Rolle/Arduino-Automatic-RFID-Cat-Feeder

Use for:

- startup homing
- limit-switch confirmed door states
- power-loss recovery

## 7. Honeysad007/RFID-Cat-Feeder

Use for:

- practical multi-cat RFID feeder behavior
- IR-based presence hold
- duplicate-read filtering
- slow/quiet servo motion
- pausing RFID reads during high-noise actuator movement if required

## 8. Catcierge

Use for:

- testable FSM architecture
- separating recognition from door-control decisions
- simulation/test tooling for passage scenarios

## Design rule

No external repository is treated as the architectural source of truth.

Our source of truth is:

- `docs/ARCHITECTURE.md`
- `docs/STATE_MACHINE.md`
- project tests

Any borrowed implementation must be license-compatible and documented before code is copied or adapted.
