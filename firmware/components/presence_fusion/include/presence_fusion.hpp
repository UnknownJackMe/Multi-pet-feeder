#pragma once

#include <cstdint>

#include "core_types.hpp"

namespace mpf {

struct PresenceFusionConfig {
    std::int64_t max_tof_age_ms{500};
    std::int64_t max_scale_age_ms{500};
    float empty_mass_threshold_kg{0.25F};
    float overweight_margin_kg{0.35F};
    int multi_cluster_threshold{2};
};

struct PresenceFusionInputs {
    IdentitySnapshot identity{};
    ToFOccupancySample tof{};
    ChamberMassSample chamber_mass{};
    BeamState outer_beam{};
    BeamState inner_beam{};
    std::int64_t now_ms{0};
};

class PresenceFusion {
public:
    explicit PresenceFusion(PresenceFusionConfig config = {});

    PresenceVerdict evaluate(const PresenceFusionInputs& inputs) const;

private:
    bool tof_fresh(const PresenceFusionInputs& inputs) const;
    bool scale_fresh(const PresenceFusionInputs& inputs) const;
    bool mass_matches_pet(const PresenceFusionInputs& inputs) const;
    bool mass_exceeds_pet_range(const PresenceFusionInputs& inputs) const;

    PresenceFusionConfig config_{};
};

}  // namespace mpf
