#pragma once

#include <cstdint>

namespace mpf {

enum class PetId : std::uint8_t {
    Unknown = 0,
    Sushi,
    Coconut,
    Blueberry,
};

enum class HopperId : std::uint8_t {
    Sushi = 0,
    Coconut,
    Blueberry,
};

enum class DoorId : std::uint8_t {
    Outer = 0,
    Inner,
};

enum class DoorPhysicalState : std::uint8_t {
    Unknown = 0,
    Closed,
    Open,
    MovingOpening,
    MovingClosing,
    Fault,
};

enum class OccupancyVerdict : std::uint8_t {
    Empty = 0,
    ExactlyOnePlausiblePet,
    MultipleOrTailgatingSuspected,
    Ambiguous,
    SensorFault,
};

struct PetProfile {
    PetId id{PetId::Unknown};
    std::uint64_t tag_id{0};
    float min_chamber_mass_kg{0.0F};
    float max_chamber_mass_kg{0.0F};
    HopperId hopper{HopperId::Sushi};
};

struct IdentitySnapshot {
    bool known{false};
    bool allowed{false};
    bool fresh{false};
    PetProfile profile{};
    std::int64_t timestamp_ms{0};
};

struct DoorFeedback {
    DoorId door{DoorId::Outer};
    DoorPhysicalState state{DoorPhysicalState::Unknown};
    bool healthy{false};
    bool open_limit{false};
    bool closed_limit{false};
    bool actuator_fault{false};
    std::int64_t timestamp_ms{0};
};

struct PresenceVerdict {
    OccupancyVerdict verdict{OccupancyVerdict::SensorFault};
    bool mass_matches_authorized_pet{false};
    bool outer_threshold_clear{false};
    bool inner_threshold_clear{false};
    bool sensors_healthy{false};
    std::int64_t timestamp_ms{0};
};

struct AccessInputs {
    IdentitySnapshot identity{};
    PresenceVerdict presence{};
    DoorFeedback outer_door{DoorId::Outer};
    DoorFeedback inner_door{DoorId::Inner};
    bool feeding_session_available{false};
    std::int64_t now_ms{0};
};

struct BowlMassSample {
    bool healthy{false};
    bool stable{false};
    float mass_g{0.0F};
    std::int64_t timestamp_ms{0};
};

struct ChamberMassSample {
    bool healthy{false};
    bool stable{false};
    float mass_kg{0.0F};
    std::int64_t timestamp_ms{0};
};

struct ToFOccupancySample {
    bool healthy{false};
    int occupied_zone_count{0};
    int cluster_count{0};
    float nearest_range_m{0.0F};
    std::int64_t timestamp_ms{0};
};

struct BeamState {
    bool healthy{false};
    bool blocked{false};
    std::int64_t stable_since_ms{0};
};

constexpr bool is_door_feedback_contradictory(const DoorFeedback& feedback) {
    return feedback.open_limit && feedback.closed_limit;
}

constexpr bool is_door_confirmed_closed(const DoorFeedback& feedback) {
    return feedback.healthy &&
           !is_door_feedback_contradictory(feedback) &&
           feedback.closed_limit &&
           !feedback.open_limit &&
           feedback.state == DoorPhysicalState::Closed;
}

constexpr bool is_door_confirmed_open(const DoorFeedback& feedback) {
    return feedback.healthy &&
           !is_door_feedback_contradictory(feedback) &&
           feedback.open_limit &&
           !feedback.closed_limit &&
           feedback.state == DoorPhysicalState::Open;
}

}  // namespace mpf
