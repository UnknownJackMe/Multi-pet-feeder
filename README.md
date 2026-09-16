# Multi-pet-feeder

Open-source multi-pet automatic feeder for a household with multiple pets and pet-specific diets.

The V1 concept uses three independent food hoppers, a shared weighing bowl, RFID identity, a two-door authentication chamber, anti-tailgating sensor fusion, local/offline scheduling, and fail-safe door control.

## Current design documents

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — system architecture and safety model
- [`docs/HARDWARE.md`](docs/HARDWARE.md) — V1 hardware baseline
- [`docs/STATE_MACHINE.md`](docs/STATE_MACHINE.md) — anti-tailgating access-control FSM
- [`docs/REFERENCES.md`](docs/REFERENCES.md) — open-source reference map
- [`docs/ROADMAP.md`](docs/ROADMAP.md) — staged implementation plan

## Core principles

- ESP32-S3 is the authoritative local controller.
- Feeding must continue without Wi-Fi/cloud/NUC/camera.
- Outer and inner doors are never allowed to be open simultaneously.
- Ambiguous identity or occupancy keeps the inner door closed.
- Door state is confirmed by physical sensors, not command assumptions.
- Pet safety overrides food-access completion.

## Status

Architecture bootstrap in progress. No production hardware or firmware has been validated yet.
