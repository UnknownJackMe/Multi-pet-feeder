#include "access_control.hpp"

namespace mpf {

namespace {

bool both_physically_open(const AccessInputs& inputs) {
    return is_door_confirmed_open(inputs.outer_door) &&
           is_door_confirmed_open(inputs.inner_door);
}

bool critical_presence_fault(const PresenceVerdict& presence) {
    return !presence.sensors_healthy ||
           presence.verdict == OccupancyVerdict::SensorFault;
}

bool tailgating_or_ambiguous(const PresenceVerdict& presence) {
    return presence.verdict == OccupancyVerdict::MultipleOrTailgatingSuspected ||
           presence.verdict == OccupancyVerdict::Ambiguous;
}

}  // namespace

AccessController::AccessController(AccessConfig config) : config_(config) {}

void AccessController::force_reset(std::int64_t now_ms) {
    state_ = AccessState::Boot;
    state_entered_at_ms_ = now_ms;
    authorized_pet_ = PetId::Unknown;
}

bool AccessController::both_doors_confirmed_closed(const AccessInputs& inputs) const {
    return is_door_confirmed_closed(inputs.outer_door) &&
           is_door_confirmed_closed(inputs.inner_door);
}

bool AccessController::any_door_fault(const AccessInputs& inputs) const {
    const auto bad = [](const DoorFeedback& door) {
        return is_door_feedback_contradictory(door) ||
               door.actuator_fault ||
               door.state == DoorPhysicalState::Fault;
    };

    return bad(inputs.outer_door) || bad(inputs.inner_door) ||
           both_physically_open(inputs);
}

bool AccessController::authorization_valid(const AccessInputs& inputs) const {
    return inputs.identity.known &&
           inputs.identity.allowed &&
           inputs.identity.fresh &&
           inputs.identity.profile.id != PetId::Unknown &&
           inputs.feeding_session_available;
}

bool AccessController::inner_open_preconditions_met(const AccessInputs& inputs) const {
    return authorized_pet_ != PetId::Unknown &&
           inputs.feeding_session_available &&
           is_door_confirmed_closed(inputs.outer_door) &&
           is_door_confirmed_closed(inputs.inner_door) &&
           inputs.presence.sensors_healthy &&
           inputs.presence.verdict == OccupancyVerdict::ExactlyOnePlausiblePet &&
           inputs.presence.mass_matches_authorized_pet &&
           inputs.presence.outer_threshold_clear &&
           inputs.presence.inner_threshold_clear;
}

bool AccessController::timed_out(const AccessInputs& inputs,
                                 std::int64_t timeout_ms) const {
    if (timeout_ms <= 0) {
        return false;
    }
    return (inputs.now_ms - state_entered_at_ms_) > timeout_ms;
}

AccessStepResult AccessController::transition_to(AccessState next,
                                                 const AccessInputs& inputs,
                                                 AccessActions actions) {
    state_ = next;
    state_entered_at_ms_ = inputs.now_ms;
    return {state_, actions};
}

AccessStepResult AccessController::stay(AccessActions actions) const {
    return {state_, actions};
}

AccessStepResult AccessController::enter_safe_fault(const AccessInputs& inputs) {
    AccessActions actions{};
    actions.enter_fault = true;
    actions.invalidate_authorization = true;
    authorized_pet_ = PetId::Unknown;
    return transition_to(AccessState::SafeFault, inputs, actions);
}

AccessStepResult AccessController::abort_to_outer_recovery(const AccessInputs& inputs) {
    AccessActions actions{};
    actions.invalidate_authorization = true;
    authorized_pet_ = PetId::Unknown;

    // Recovery is only allowed toward the outer/public side while the inner
    // boundary is physically confirmed closed.
    if (!is_door_confirmed_closed(inputs.inner_door)) {
        return enter_safe_fault(inputs);
    }

    if (!is_door_confirmed_open(inputs.outer_door)) {
        actions.open_outer = true;
    }

    return transition_to(AccessState::RecoveryOuterOpening, inputs, actions);
}

AccessStepResult AccessController::step(const AccessInputs& inputs) {
    if (state_ != AccessState::Boot &&
        state_ != AccessState::Homing &&
        state_ != AccessState::SafeFault &&
        any_door_fault(inputs)) {
        return enter_safe_fault(inputs);
    }

    switch (state_) {
        case AccessState::Boot: {
            AccessActions actions{};
            actions.home_outer = true;
            actions.home_inner = true;
            return transition_to(AccessState::Homing, inputs, actions);
        }

        case AccessState::Homing: {
            if (any_door_fault(inputs)) {
                return enter_safe_fault(inputs);
            }
            if (both_doors_confirmed_closed(inputs)) {
                return transition_to(AccessState::Idle, inputs);
            }
            if (timed_out(inputs, config_.door_move_timeout_ms)) {
                return enter_safe_fault(inputs);
            }
            return stay();
        }

        case AccessState::Idle: {
            if (!both_doors_confirmed_closed(inputs)) {
                return enter_safe_fault(inputs);
            }

            if (authorization_valid(inputs)) {
                authorized_pet_ = inputs.identity.profile.id;
                return transition_to(AccessState::AuthPending, inputs);
            }
            return stay();
        }

        case AccessState::AuthPending: {
            if (!authorization_valid(inputs) ||
                inputs.identity.profile.id != authorized_pet_) {
                AccessActions actions{};
                actions.invalidate_authorization = true;
                authorized_pet_ = PetId::Unknown;
                return transition_to(AccessState::Idle, inputs, actions);
            }

            if (timed_out(inputs, config_.authorization_timeout_ms)) {
                AccessActions actions{};
                actions.invalidate_authorization = true;
                authorized_pet_ = PetId::Unknown;
                return transition_to(AccessState::Idle, inputs, actions);
            }

            if (is_door_confirmed_closed(inputs.inner_door) &&
                is_door_confirmed_closed(inputs.outer_door) &&
                inputs.presence.outer_threshold_clear) {
                AccessActions actions{};
                actions.open_outer = true;
                return transition_to(AccessState::OuterOpening, inputs, actions);
            }
            return stay();
        }

        case AccessState::OuterOpening: {
            if (!is_door_confirmed_closed(inputs.inner_door)) {
                return enter_safe_fault(inputs);
            }
            if (is_door_confirmed_open(inputs.outer_door)) {
                return transition_to(AccessState::EntryWait, inputs);
            }
            if (timed_out(inputs, config_.door_move_timeout_ms)) {
                return enter_safe_fault(inputs);
            }
            return stay();
        }

        case AccessState::EntryWait: {
            if (critical_presence_fault(inputs.presence) ||
                tailgating_or_ambiguous(inputs.presence)) {
                return abort_to_outer_recovery(inputs);
            }

            if (inputs.presence.verdict == OccupancyVerdict::ExactlyOnePlausiblePet &&
                inputs.presence.mass_matches_authorized_pet &&
                inputs.presence.outer_threshold_clear) {
                AccessActions actions{};
                actions.close_outer = true;
                return transition_to(AccessState::OuterClosing, inputs, actions);
            }

            if (timed_out(inputs, config_.entry_timeout_ms)) {
                return abort_to_outer_recovery(inputs);
            }
            return stay();
        }

        case AccessState::OuterClosing: {
            if (!inputs.presence.outer_threshold_clear) {
                // A closing command may already be in flight even if the
                // open-limit switch still reads active for a short time.
                // Therefore explicitly command the door back open instead of
                // relying on stale physical feedback to imply safety.
                if (!is_door_confirmed_closed(inputs.inner_door)) {
                    return enter_safe_fault(inputs);
                }
                AccessActions actions{};
                actions.open_outer = true;
                actions.invalidate_authorization = true;
                authorized_pet_ = PetId::Unknown;
                return transition_to(AccessState::RecoveryOuterOpening,
                                     inputs,
                                     actions);
            }
            if (is_door_confirmed_closed(inputs.outer_door)) {
                return transition_to(AccessState::ChamberVerify, inputs);
            }
            if (timed_out(inputs, config_.door_move_timeout_ms)) {
                return enter_safe_fault(inputs);
            }
            return stay();
        }

        case AccessState::ChamberVerify: {
            if (inner_open_preconditions_met(inputs)) {
                AccessActions actions{};
                actions.open_inner = true;
                return transition_to(AccessState::InnerOpening, inputs, actions);
            }

            if (critical_presence_fault(inputs.presence) ||
                tailgating_or_ambiguous(inputs.presence) ||
                inputs.presence.verdict == OccupancyVerdict::Empty ||
                timed_out(inputs, config_.chamber_verify_timeout_ms)) {
                return abort_to_outer_recovery(inputs);
            }
            return stay();
        }

        case AccessState::InnerOpening: {
            if (!is_door_confirmed_closed(inputs.outer_door)) {
                return enter_safe_fault(inputs);
            }
            if (is_door_confirmed_open(inputs.inner_door)) {
                return transition_to(AccessState::FeedEntryWait, inputs);
            }
            if (timed_out(inputs, config_.door_move_timeout_ms)) {
                return enter_safe_fault(inputs);
            }
            return stay();
        }

        case AccessState::FeedEntryWait: {
            if (!inputs.presence.inner_threshold_clear) {
                return stay();
            }

            // Once the authentication chamber is empty, the pet has crossed
            // the inner threshold into the feeding area.
            if (inputs.presence.verdict == OccupancyVerdict::Empty) {
                AccessActions actions{};
                actions.close_inner = true;
                return transition_to(AccessState::InnerClosing, inputs, actions);
            }

            if (critical_presence_fault(inputs.presence)) {
                return enter_safe_fault(inputs);
            }
            return stay();
        }

        case AccessState::InnerClosing: {
            if (!inputs.presence.inner_threshold_clear) {
                AccessActions actions{};
                actions.open_inner = true;
                return transition_to(AccessState::FeedEntryWait, inputs, actions);
            }
            if (is_door_confirmed_closed(inputs.inner_door)) {
                return transition_to(AccessState::Feeding, inputs);
            }
            if (timed_out(inputs, config_.door_move_timeout_ms)) {
                return enter_safe_fault(inputs);
            }
            return stay();
        }

        case AccessState::Feeding:
            // Feeding completion and the reverse exit sequence will be wired
            // after the feeding-session component is introduced. Until then,
            // this state intentionally emits no door action.
            return stay();

        case AccessState::ExitSequence:
            // Reserved for the reverse two-door sequence.
            return stay();

        case AccessState::RecoveryOuterOpening: {
            if (!is_door_confirmed_closed(inputs.inner_door)) {
                return enter_safe_fault(inputs);
            }

            if (!is_door_confirmed_open(inputs.outer_door) &&
                !is_door_confirmed_closed(inputs.outer_door)) {
                if (timed_out(inputs, config_.door_move_timeout_ms)) {
                    return enter_safe_fault(inputs);
                }
                return stay();
            }

            if (is_door_confirmed_closed(inputs.outer_door)) {
                if (inputs.presence.verdict == OccupancyVerdict::Empty) {
                    return transition_to(AccessState::Idle, inputs);
                }
                AccessActions actions{};
                actions.open_outer = true;
                return stay(actions);
            }

            if (inputs.presence.verdict == OccupancyVerdict::Empty &&
                inputs.presence.outer_threshold_clear) {
                AccessActions actions{};
                actions.close_outer = true;
                return stay(actions);
            }

            return stay();
        }

        case AccessState::SafeFault:
            // Automatic recovery from a safety fault is intentionally not
            // implemented. A future fault manager may allow specific,
            // explicitly classified recoverable faults to re-home.
            return stay();
    }

    return enter_safe_fault(inputs);
}

}  // namespace mpf
