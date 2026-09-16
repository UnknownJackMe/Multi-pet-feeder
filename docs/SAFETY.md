# Safety and Fault Model

This project controls moving doors around animals. Safety behavior is therefore part of the architecture, not a later polish step.

## 1. Safety priorities

Priority order:

1. avoid trapping, pinching or crushing a pet;
2. prevent unauthorized access to food;
3. avoid duplicate/incorrect portions;
4. preserve availability and remote observability.

When priorities conflict, higher priorities win.

## 2. Hard invariants

The implementation and tests must enforce these invariants.

### S-001 Two-door interlock

The outer and inner doors must never be physically open at the same time.

```text
NOT (outer_open AND inner_open)
```

### S-002 Inner-door authorization

The inner door may only start opening when all critical checks pass:

```text
known authorized pet
AND valid meal/session permission
AND outer door physically closed
AND inner door physically closed before command
AND outer threshold clear
AND inner threshold clear
AND chamber occupancy == exactly one plausible pet
AND chamber mass matches authorized pet range
AND required sensors healthy
```

### S-003 Obstruction response

If a threshold becomes blocked while the corresponding door is closing:

1. stop closing;
2. reverse/open enough to remove pinch force;
3. invalidate the current passage attempt;
4. do not continue automatically from the old authorization token.

### S-004 Physical feedback authority

Door state is derived from limit switches / validated physical feedback, not command history.

### S-005 Contradictory limits are a fault

For one door:

```text
open_limit == true && closed_limit == true
```

is impossible in healthy mechanics and must enter fault handling.

### S-006 No duplicate meal after reboot

A reboot may interrupt a meal, but must not reset the meal ledger such that a second full portion is dispensed automatically.

## 3. Fail-closed vs fail-safe

These terms apply to different hazards.

### Food authorization: fail-closed

If identity or occupancy is ambiguous, keep the inner food-access boundary closed.

### Pet physical safety: fail-safe

If door safety is ambiguous while an animal may be in the doorway, remove closing force and move toward a non-pinching state when mechanically possible.

This means `SAFE_FAULT` is not simply "close every door". The correct action depends on where the pet might be.

## 4. Sensor health

Each critical sensor exposes both value and health/freshness.

A stale value must not be treated as a current valid value.

Suggested V1 freshness limits are configuration values and will be established empirically during bench testing.

Critical sensors:

- RFID during authorization;
- outer threshold beam during outer-door movement;
- inner threshold beam during inner-door movement;
- ToF during chamber verification;
- chamber scale during chamber verification;
- open/closed limit switches during all door movement;
- bowl scale during closed-loop dispensing.

## 5. Tailgating threat model

Expected adversarial/accidental cases:

- authorized pet enters and a second pet follows closely;
- second pet inserts only head/front legs through outer doorway;
- two pets overlap from ToF perspective;
- only one RFID tag is visible while two pets are physically present;
- wrong pet enters after an authorized tag was read nearby;
- authorized pet backs out during closing;
- pet remains in chamber longer than expected;
- pet pushes against a moving door;
- one sensor temporarily drops out.

The architecture assumes pets will exploit mechanical openings accidentally or deliberately. RFID alone is therefore not sufficient occupancy evidence.

## 6. Door motion safety layer

Each door controller should implement local protections independent of high-level state transitions:

- bounded movement timeout;
- open and closed limit validation;
- command rejection if requested motion contradicts an already-active limit;
- immediate stop/reverse input from obstruction logic;
- soft/slow motion near end of travel where actuator permits;
- no automatic retry loop that can repeatedly press against an animal.

Prototype door mechanics should use:

- low-mass panel;
- rounded/soft edge;
- low-force linkage or torque-limited actuation;
- mechanical compliance where practical.

## 7. Power recovery

On boot after uncontrolled power loss:

1. do not trust persisted door position;
2. initialize GPIO to non-driving safe states;
3. sample limits/beams before moving;
4. if a doorway is occupied, avoid homing motion through the occupied region;
5. reconcile meal ledger;
6. establish known door state;
7. enter IDLE only after safety checks pass.

A UPS is useful for availability but is not a substitute for correct recovery behavior.

## 8. RFID electrical robustness

Pet RFID range is sensitive to antenna geometry and supply noise.

Prototype requirements:

- motor/servo current does not share an unfiltered path with RFID analog power;
- local decoupling near reader;
- reader antenna mounted away from stepper/servo wiring where practical;
- validate read volume in 3D with the actual collar/tag orientation;
- measure read reliability while motors are active, not only on a quiet bench.

If the selected reader cannot reliably identify a moving pet through the intended portal geometry, change the reader/antenna architecture rather than compensating with application logic.

## 9. Dispensing hazards

Fault conditions:

- auger rotates but bowl mass does not rise;
- mass rises too quickly/unexpectedly;
- bowl already contains unaccounted food;
- bowl scale is unstable/unhealthy;
- motor driver overheats or stalls;
- wrong hopper is selected.

Response should be bounded; never run an auger indefinitely trying to reach target mass.

## 10. Required pre-animal testing

Do not put animals through the prototype until bench tests cover:

- door homing;
- limit-switch failures;
- beam obstruction during close;
- power cut during every door state;
- simultaneous/contradictory sensor signals;
- tailgating simulation with two weighted objects;
- ToF occupancy ambiguity;
- scale disconnect/frozen data;
- motor jam;
- network loss.

Then use supervised animal trials before any unattended operation.

## 11. Unattended-operation gate

Unattended 1–3 day use is not a V1 assumption. It is a release gate reached only after:

- repeated successful supervised operation;
- quantified door obstruction reliability;
- quantified RFID read reliability;
- quantified portion accuracy;
- long-duration soak testing;
- reboot/power-loss tests;
- jam recovery tests;
- telemetry/log review;
- mechanical inspection for wear and sharp/pinch points.

The repository should track these as measurable acceptance criteria rather than declaring the device safe from design intent alone.
