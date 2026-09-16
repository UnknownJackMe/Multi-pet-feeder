# Component Interfaces

V1 separates hardware drivers from domain policy. Drivers report observations; domain components decide what actions are allowed.

## 1. Core domain types

```cpp
enum class PetId {
    Unknown,
    Sushi,
    Coconut,
    Blueberry,
};

enum class DoorId {
    Outer,
    Inner,
};

enum class DoorPhysicalState {
    Unknown,
    Closed,
    Open,
    Moving,
    Fault,
};

enum class HopperId {
    Sushi,
    Coconut,
    Blueberry,
};
```

The real implementation should use stable numeric/string IDs in persistent storage, while these enum names remain convenient V1 defaults.

## 2. RFID reader

### Responsibility

Convert reader-specific frames into normalized tag observations.

### Input

Reader-specific UART/SPI/raw signal.

### Output

```cpp
struct PetTagObserved {
    uint64_t tag_id;
    int64_t timestamp_ms;
    int signal_quality;       // optional; -1 when unavailable
};
```

The RFID driver does **not** decide whether a tag is authorized.

## 3. Pet identity service

### Responsibility

Map tag IDs to configured pets and feeding permissions.

```cpp
struct PetProfile {
    PetId id;
    uint64_t tag_id;
    float min_chamber_mass_kg;
    float max_chamber_mass_kg;
    HopperId hopper;
};
```

Output:

```cpp
struct PetIdentityResult {
    bool known;
    bool currently_allowed;
    PetProfile profile;
};
```

## 4. ToF occupancy driver

### Responsibility

Expose raw/processed chamber occupancy evidence without making access decisions.

```cpp
struct ToFOccupancySample {
    bool healthy;
    int occupied_zone_count;
    int cluster_count;
    float nearest_range_m;
    int64_t timestamp_ms;
};
```

For VL53L5CX, the first V1 algorithm may be deliberately simple:

- reject invalid zones;
- threshold foreground depth against learned empty-chamber baseline;
- count connected occupied regions;
- expose confidence/health to the fusion layer.

## 5. Beam sensors

Each threshold beam provides a debounced logical state.

```cpp
struct BeamState {
    bool healthy;
    bool blocked;
    int64_t stable_since_ms;
};
```

Required beams:

- `outer_threshold_beam`
- `inner_threshold_beam`

## 6. Chamber scale

```cpp
struct ScaleSample {
    bool healthy;
    float mass_kg;
    bool stable;
    int64_t timestamp_ms;
};
```

The chamber scale is used for plausibility/occupancy evidence, not veterinary-grade weight measurement.

## 7. Bowl scale

Same base interface as chamber scale, but with gram-level presentation:

```cpp
struct BowlMassSample {
    bool healthy;
    float mass_g;
    bool stable;
    int64_t timestamp_ms;
};
```

## 8. Door controller

### Commands

```cpp
enum class DoorCommand {
    Open,
    Close,
    Stop,
    HomeClosed,
};
```

### Feedback

```cpp
struct DoorFeedback {
    DoorId door;
    DoorPhysicalState state;
    bool open_limit;
    bool closed_limit;
    bool actuator_fault;
    int64_t timestamp_ms;
};
```

### Rules

- command completion must be confirmed from physical feedback;
- `open_limit && closed_limit` is contradictory and is a fault;
- movement has a bounded timeout;
- obstruction handling is owned jointly by the access FSM and door safety layer;
- an actuator timeout must never be silently converted to `Closed` or `Open`.

## 9. Presence fusion

Inputs:

- latest authorized identity candidate;
- ToF occupancy;
- chamber mass;
- threshold beams;
- sensor health.

Output:

```cpp
enum class OccupancyVerdict {
    Empty,
    ExactlyOnePlausiblePet,
    MultipleOrTailgatingSuspected,
    Ambiguous,
    SensorFault,
};

struct PresenceVerdict {
    OccupancyVerdict verdict;
    bool mass_matches_authorized_pet;
    bool outer_threshold_clear;
    bool inner_threshold_clear;
};
```

Important: the fusion component provides evidence; only access control authorizes the inner door.

## 10. Access-control FSM

### Snapshot input

```cpp
struct AccessInputs {
    PetIdentityResult identity;
    PresenceVerdict presence;
    DoorFeedback outer_door;
    DoorFeedback inner_door;
    bool feeding_session_available;
    int64_t now_ms;
};
```

### Commands

```cpp
struct AccessActions {
    bool open_outer = false;
    bool close_outer = false;
    bool open_inner = false;
    bool close_inner = false;
    bool invalidate_authorization = false;
    bool enter_fault = false;
};
```

The FSM should be deterministic: the same state + same event/snapshot produces the same transition/action.

## 11. Hopper controller

```cpp
struct DispenseRequest {
    HopperId hopper;
    float target_mass_g;
    float tolerance_g;
    uint32_t max_jam_retries;
};

enum class DispenseResult {
    Completed,
    BowlScaleFault,
    Jammed,
    Timeout,
    Cancelled,
};
```

The controller combines motor motion and bowl weight feedback.

## 12. Scheduler

Scheduler output is a permission/window, not a direct motor command.

```cpp
struct MealWindow {
    uint64_t meal_id;
    PetId pet;
    float target_mass_g;
    int64_t start_epoch_s;
    int64_t end_epoch_s;
};
```

The access/feeding session consumes the meal window only after the correct pet is authenticated.

## 13. Feeding ledger

Persistent record:

```cpp
struct FeedingLedgerEntry {
    uint64_t meal_id;
    PetId pet;
    HopperId hopper;
    float target_mass_g;
    float dispensed_mass_g;
    bool completed;
};
```

Writes must be idempotent enough that a reboot cannot cause a second full portion for the same meal.

## 14. Fault manager

Faults should have stable machine-readable codes.

Initial classes:

```text
DOOR_CONTRADICTORY_LIMITS
DOOR_MOVE_TIMEOUT
DOOR_OBSTRUCTION
RFID_UNHEALTHY
TOF_UNHEALTHY
CHAMBER_SCALE_UNHEALTHY
BOWL_SCALE_UNHEALTHY
TAILGATING_SUSPECTED
DISPENSER_JAM
POWER_RECOVERY_REQUIRED
```

Each fault has:

- severity;
- whether feeding/access can continue;
- local recovery action;
- telemetry payload.

## 15. Event bus policy

Use events for decoupling, but do not make safety behavior implicit.

Recommended event families:

```text
sensor.rfid
sensor.tof
sensor.beam
sensor.scale
actuator.door
actuator.hopper
domain.identity
domain.presence
domain.access
domain.feeding
domain.fault
system.boot
system.network
```

Safety-critical transitions remain explicit inside the access-control core and are covered by deterministic tests.
