#include "presence_fusion.hpp"

namespace mpf {

PresenceFusion::PresenceFusion(PresenceFusionConfig config) : config_(config) {}

bool PresenceFusion::tof_fresh(const PresenceFusionInputs& inputs) const {
    return inputs.tof.healthy &&
           (inputs.now_ms - inputs.tof.timestamp_ms) <= config_.max_tof_age_ms;
}

bool PresenceFusion::scale_fresh(const PresenceFusionInputs& inputs) const {
    return inputs.chamber_mass.healthy &&
           inputs.chamber_mass.stable &&
           (inputs.now_ms - inputs.chamber_mass.timestamp_ms) <= config_.max_scale_age_ms;
}

bool PresenceFusion::mass_matches_pet(const PresenceFusionInputs& inputs) const {
    if (!inputs.identity.known || inputs.identity.profile.id == PetId::Unknown) {
        return false;
    }

    const float mass = inputs.chamber_mass.mass_kg;
    return mass >= inputs.identity.profile.min_chamber_mass_kg &&
           mass <= inputs.identity.profile.max_chamber_mass_kg;
}

bool PresenceFusion::mass_exceeds_pet_range(const PresenceFusionInputs& inputs) const {
    if (!inputs.identity.known || inputs.identity.profile.id == PetId::Unknown) {
        return false;
    }

    return inputs.chamber_mass.mass_kg >
           (inputs.identity.profile.max_chamber_mass_kg + config_.overweight_margin_kg);
}

PresenceVerdict PresenceFusion::evaluate(const PresenceFusionInputs& inputs) const {
    PresenceVerdict output{};
    output.timestamp_ms = inputs.now_ms;
    output.outer_threshold_clear = inputs.outer_beam.healthy && !inputs.outer_beam.blocked;
    output.inner_threshold_clear = inputs.inner_beam.healthy && !inputs.inner_beam.blocked;

    const bool sensors_healthy = tof_fresh(inputs) &&
                                 scale_fresh(inputs) &&
                                 inputs.outer_beam.healthy &&
                                 inputs.inner_beam.healthy;
    output.sensors_healthy = sensors_healthy;

    if (!sensors_healthy) {
        output.verdict = OccupancyVerdict::SensorFault;
        output.mass_matches_authorized_pet = false;
        return output;
    }

    output.mass_matches_authorized_pet = mass_matches_pet(inputs);

    const bool mass_near_empty =
        inputs.chamber_mass.mass_kg <= config_.empty_mass_threshold_kg;
    const bool tof_empty =
        inputs.tof.cluster_count == 0 && inputs.tof.occupied_zone_count == 0;

    if (mass_near_empty && tof_empty) {
        output.verdict = OccupancyVerdict::Empty;
        return output;
    }

    const bool tof_suggests_multiple =
        inputs.tof.cluster_count >= config_.multi_cluster_threshold;

    if (tof_suggests_multiple || mass_exceeds_pet_range(inputs)) {
        output.verdict = OccupancyVerdict::MultipleOrTailgatingSuspected;
        return output;
    }

    const bool tof_suggests_one = inputs.tof.cluster_count == 1;

    if (tof_suggests_one && output.mass_matches_authorized_pet) {
        output.verdict = OccupancyVerdict::ExactlyOnePlausiblePet;
        return output;
    }

    // Examples that intentionally land here:
    // - ToF sees one cluster but chamber mass does not match the authorized pet.
    // - Weight looks plausible but ToF cannot establish exactly one cluster.
    // - Partial-body / doorway geometry causes inconsistent evidence.
    output.verdict = OccupancyVerdict::Ambiguous;
    return output;
}

}  // namespace mpf
