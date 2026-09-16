#pragma once

#include <cstdint>

#include "core_types.hpp"

namespace mpf {

enum class AccessState : std::uint8_t {
    Boot = 0,
    Homing,
    Idle,
    AuthPending,
    OuterOpening,
    EntryWait,
    OuterClosing,
    ChamberVerify,
    InnerOpening,
    FeedEntryWait,
    InnerClosing,
    Feeding,
    ExitSequence,
    RecoveryOuterOpening,
    SafeFault,
};

struct AccessActions {
    bool home_outer{false};
    bool home_inner{false};
    bool open_outer{false};
    bool close_outer{false};
    bool open_inner{false};
    bool close_inner{false};
    bool invalidate_authorization{false};
    bool enter_fault{false};
};

struct AccessStepResult {
    AccessState state{AccessState::Boot};
    AccessActions actions{};
};

struct AccessConfig {
    std::int64_t authorization_timeout_ms{5000};
    std::int64_t entry_timeout_ms{10000};
    std::int64_t door_move_timeout_ms{5000};
    std::int64_t chamber_verify_timeout_ms{3000};
};

class AccessController {
public:
    explicit AccessController(AccessConfig config = {});

    AccessStepResult step(const AccessInputs& inputs);

    AccessState state() const { return state_; }
    std::int64_t state_entered_at_ms() const { return state_entered_at_ms_; }
    PetId authorized_pet() const { return authorized_pet_; }

    void force_reset(std::int64_t now_ms = 0);

private:
    bool both_doors_confirmed_closed(const AccessInputs& inputs) const;
    bool any_door_fault(const AccessInputs& inputs) const;
    bool inner_open_preconditions_met(const AccessInputs& inputs) const;
    bool authorization_valid(const AccessInputs& inputs) const;
    bool timed_out(const AccessInputs& inputs, std::int64_t timeout_ms) const;

    AccessStepResult transition_to(AccessState next,
                                   const AccessInputs& inputs,
                                   AccessActions actions = {});
    AccessStepResult stay(AccessActions actions = {}) const;
    AccessStepResult enter_safe_fault(const AccessInputs& inputs);
    AccessStepResult abort_to_outer_recovery(const AccessInputs& inputs);

    AccessConfig config_{};
    AccessState state_{AccessState::Boot};
    std::int64_t state_entered_at_ms_{0};
    PetId authorized_pet_{PetId::Unknown};
};

}  // namespace mpf
