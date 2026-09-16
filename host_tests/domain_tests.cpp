#include <cstdlib>
#include <iostream>

#include "access_control.hpp"
#include "presence_fusion.hpp"

using namespace mpf;

namespace {

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

PetProfile sushi_profile() {
    PetProfile profile{};
    profile.id = PetId::Sushi;
    profile.tag_id = 1001;
    profile.min_chamber_mass_kg = 3.2F;
    profile.max_chamber_mass_kg = 4.4F;
    profile.hopper = HopperId::Sushi;
    return profile;
}

IdentitySnapshot authorized_sushi(std::int64_t now_ms) {
    IdentitySnapshot identity{};
    identity.known = true;
    identity.allowed = true;
    identity.fresh = true;
    identity.profile = sushi_profile();
    identity.timestamp_ms = now_ms;
    return identity;
}

DoorFeedback closed_door(DoorId id, std::int64_t now_ms) {
    DoorFeedback door{};
    door.door = id;
    door.state = DoorPhysicalState::Closed;
    door.healthy = true;
    door.closed_limit = true;
    door.timestamp_ms = now_ms;
    return door;
}

DoorFeedback open_door(DoorId id, std::int64_t now_ms) {
    DoorFeedback door{};
    door.door = id;
    door.state = DoorPhysicalState::Open;
    door.healthy = true;
    door.open_limit = true;
    door.timestamp_ms = now_ms;
    return door;
}

PresenceVerdict presence_one(std::int64_t now_ms) {
    PresenceVerdict p{};
    p.verdict = OccupancyVerdict::ExactlyOnePlausiblePet;
    p.mass_matches_authorized_pet = true;
    p.outer_threshold_clear = true;
    p.inner_threshold_clear = true;
    p.sensors_healthy = true;
    p.timestamp_ms = now_ms;
    return p;
}

PresenceVerdict presence_empty(std::int64_t now_ms) {
    PresenceVerdict p{};
    p.verdict = OccupancyVerdict::Empty;
    p.mass_matches_authorized_pet = false;
    p.outer_threshold_clear = true;
    p.inner_threshold_clear = true;
    p.sensors_healthy = true;
    p.timestamp_ms = now_ms;
    return p;
}

void test_presence_fusion() {
    PresenceFusion fusion{};
    PresenceFusionInputs inputs{};
    inputs.identity = authorized_sushi(1000);
    inputs.now_ms = 1000;
    inputs.outer_beam = {true, false, 900};
    inputs.inner_beam = {true, false, 900};
    inputs.tof.healthy = true;
    inputs.tof.timestamp_ms = 1000;
    inputs.chamber_mass.healthy = true;
    inputs.chamber_mass.stable = true;
    inputs.chamber_mass.timestamp_ms = 1000;

    inputs.tof.cluster_count = 0;
    inputs.tof.occupied_zone_count = 0;
    inputs.chamber_mass.mass_kg = 0.05F;
    auto verdict = fusion.evaluate(inputs);
    expect(verdict.verdict == OccupancyVerdict::Empty,
           "empty chamber should be classified empty");

    inputs.tof.cluster_count = 1;
    inputs.tof.occupied_zone_count = 18;
    inputs.chamber_mass.mass_kg = 3.8F;
    verdict = fusion.evaluate(inputs);
    expect(verdict.verdict == OccupancyVerdict::ExactlyOnePlausiblePet,
           "one cluster + matching mass should pass");

    inputs.tof.cluster_count = 2;
    inputs.chamber_mass.mass_kg = 3.8F;
    verdict = fusion.evaluate(inputs);
    expect(verdict.verdict == OccupancyVerdict::MultipleOrTailgatingSuspected,
           "two ToF clusters must be treated as tailgating suspicion");

    inputs.tof.cluster_count = 1;
    inputs.chamber_mass.mass_kg = 6.0F;
    verdict = fusion.evaluate(inputs);
    expect(verdict.verdict == OccupancyVerdict::MultipleOrTailgatingSuspected,
           "overweight chamber must be treated as tailgating suspicion");

    inputs.chamber_mass.mass_kg = 2.0F;
    verdict = fusion.evaluate(inputs);
    expect(verdict.verdict == OccupancyVerdict::Ambiguous,
           "one ToF cluster with wrong mass must fail closed as ambiguous");

    inputs.tof.timestamp_ms = 0;
    verdict = fusion.evaluate(inputs);
    expect(verdict.verdict == OccupancyVerdict::SensorFault,
           "stale ToF must be treated as sensor fault");
}

void test_normal_access_path() {
    AccessController controller{};
    AccessInputs in{};
    in.now_ms = 0;

    auto out = controller.step(in);
    expect(out.state == AccessState::Homing,
           "boot should enter homing");
    expect(out.actions.home_outer && out.actions.home_inner,
           "boot should request both doors home");

    in.now_ms = 100;
    in.outer_door = closed_door(DoorId::Outer, in.now_ms);
    in.inner_door = closed_door(DoorId::Inner, in.now_ms);
    in.presence = presence_empty(in.now_ms);
    out = controller.step(in);
    expect(out.state == AccessState::Idle,
           "homed doors should enter idle");

    in.now_ms = 200;
    in.identity = authorized_sushi(in.now_ms);
    in.feeding_session_available = true;
    out = controller.step(in);
    expect(out.state == AccessState::AuthPending,
           "authorized pet should create pending authorization");

    in.now_ms = 250;
    in.identity = authorized_sushi(in.now_ms);
    out = controller.step(in);
    expect(out.state == AccessState::OuterOpening && out.actions.open_outer,
           "authorized pet should open outer door first");
    expect(!out.actions.open_inner,
           "inner door must not open during outer opening");

    in.now_ms = 350;
    in.outer_door = open_door(DoorId::Outer, in.now_ms);
    out = controller.step(in);
    expect(out.state == AccessState::EntryWait,
           "outer open confirmation should enter entry wait");

    in.now_ms = 500;
    in.presence = presence_one(in.now_ms);
    out = controller.step(in);
    expect(out.state == AccessState::OuterClosing && out.actions.close_outer,
           "one plausible pet fully inside should close outer door");

    in.now_ms = 650;
    in.outer_door = closed_door(DoorId::Outer, in.now_ms);
    out = controller.step(in);
    expect(out.state == AccessState::ChamberVerify,
           "closed outer door should enter chamber verification");

    in.now_ms = 700;
    out = controller.step(in);
    expect(out.state == AccessState::InnerOpening && out.actions.open_inner,
           "verified single authorized pet should open inner door");
    expect(!out.actions.open_outer,
           "outer door must remain closed while inner opens");

    in.now_ms = 800;
    in.inner_door = open_door(DoorId::Inner, in.now_ms);
    out = controller.step(in);
    expect(out.state == AccessState::FeedEntryWait,
           "inner open confirmation should wait for feed-area entry");

    in.now_ms = 900;
    in.presence = presence_empty(in.now_ms);
    out = controller.step(in);
    expect(out.state == AccessState::InnerClosing && out.actions.close_inner,
           "empty authentication chamber should close inner door");

    in.now_ms = 1000;
    in.inner_door = closed_door(DoorId::Inner, in.now_ms);
    out = controller.step(in);
    expect(out.state == AccessState::Feeding,
           "closed inner door should complete ingress and enter feeding");
}

void test_tailgating_is_rejected() {
    AccessController controller{};
    AccessInputs in{};

    controller.step(in);
    in.now_ms = 100;
    in.outer_door = closed_door(DoorId::Outer, in.now_ms);
    in.inner_door = closed_door(DoorId::Inner, in.now_ms);
    in.presence = presence_empty(in.now_ms);
    controller.step(in);

    in.now_ms = 200;
    in.identity = authorized_sushi(in.now_ms);
    in.feeding_session_available = true;
    controller.step(in);

    in.now_ms = 250;
    in.identity = authorized_sushi(in.now_ms);
    controller.step(in);

    in.now_ms = 350;
    in.outer_door = open_door(DoorId::Outer, in.now_ms);
    controller.step(in);

    in.now_ms = 500;
    in.presence = presence_one(in.now_ms);
    in.presence.verdict = OccupancyVerdict::MultipleOrTailgatingSuspected;
    auto out = controller.step(in);

    expect(out.state == AccessState::RecoveryOuterOpening,
           "tailgating must abort to outer-side recovery");
    expect(out.actions.invalidate_authorization,
           "tailgating must invalidate authorization token");
    expect(!out.actions.open_inner,
           "tailgating must never open inner door");
}

void test_outer_obstruction_aborts_close() {
    AccessController controller{};
    AccessInputs in{};

    controller.step(in);
    in.now_ms = 100;
    in.outer_door = closed_door(DoorId::Outer, in.now_ms);
    in.inner_door = closed_door(DoorId::Inner, in.now_ms);
    in.presence = presence_empty(in.now_ms);
    controller.step(in);

    in.now_ms = 200;
    in.identity = authorized_sushi(in.now_ms);
    in.feeding_session_available = true;
    controller.step(in);

    in.now_ms = 250;
    in.identity = authorized_sushi(in.now_ms);
    controller.step(in);

    in.now_ms = 350;
    in.outer_door = open_door(DoorId::Outer, in.now_ms);
    controller.step(in);

    in.now_ms = 500;
    in.presence = presence_one(in.now_ms);
    controller.step(in);

    in.now_ms = 550;
    in.presence.outer_threshold_clear = false;
    auto out = controller.step(in);

    expect(out.state == AccessState::RecoveryOuterOpening,
           "blocked outer threshold while closing must abort");
    expect(out.actions.open_outer,
           "blocked outer threshold should command outer door back open");
    expect(!out.actions.open_inner,
           "obstruction recovery must not expose food side");
}

void test_both_doors_open_faults() {
    AccessController controller{};
    AccessInputs in{};
    controller.step(in);

    in.now_ms = 100;
    in.outer_door = closed_door(DoorId::Outer, in.now_ms);
    in.inner_door = closed_door(DoorId::Inner, in.now_ms);
    in.presence = presence_empty(in.now_ms);
    controller.step(in);

    in.now_ms = 200;
    in.outer_door = open_door(DoorId::Outer, in.now_ms);
    in.inner_door = open_door(DoorId::Inner, in.now_ms);
    auto out = controller.step(in);

    expect(out.state == AccessState::SafeFault,
           "both doors physically open must enter safe fault");
    expect(out.actions.enter_fault,
           "both doors physically open must latch a fault");
}

}  // namespace

int main() {
    test_presence_fusion();
    test_normal_access_path();
    test_tailgating_is_rejected();
    test_outer_obstruction_aborts_close();
    test_both_doors_open_faults();

    std::cout << "All domain tests passed.\n";
    return 0;
}
