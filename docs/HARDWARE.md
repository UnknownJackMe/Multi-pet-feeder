# Hardware Baseline

## Required controller and power

- ESP32-S3 N16R8 x1
- DS3231 RTC x1
- 12 V 5 A power supply x1
- 12 V -> 5 V DC-DC regulator, >=5 A x1
- Fuse + holder x1
- Main power switch x1

## Three-channel dispensing

- NEMA17 / 42 mm stepper motor x3
- TMC2209 UART stepper driver x3
- Food-safe hopper, 2-4 L x3
- Auger or rotary dispensing mechanism x3
- Motor coupling x3
- Food-safe PP or 304 stainless chute/tube x3

## Bowl weighing

- 5 kg load cell x1
- HX711 x1
- 304 stainless pet bowl x1

## Identity

V1 baseline:

- 125 kHz medium-range RFID reader x1
- EM4100/TK4100 collar tag x3

Future option:

- 134.2 kHz FDX-B reader for implanted pet microchips

RFID power should be isolated/filtered from motor power as much as practical to reduce read-range degradation caused by actuator noise.

## Anti-tailgating chamber

- VL53L5CX 8x8 multi-zone ToF x1
- IR/photoelectric beam sensor x2
- 20 kg load cell x1
- HX711 x1

The chamber load cell is primarily an occupancy consistency signal, not a veterinary-grade body-weight scale.

## Door system

- Door actuator x2
- Open/closed limit switch x4
- Lightweight PC/acrylic door panel x2
- Guide rail or linkage x2
- Soft silicone anti-pinch edge

Prototype actuator candidates:

- MG996R servo for early mechanism testing
- geared motor / linear actuator if the final door requires better controlled force and position

Door design must not rely on actuator torque alone for safety.

## Optional vision

- 1080p UVC USB camera x1
- 5 V fill light x1
- NUC or other Linux host for YOLO / logging

Vision is advisory in V1 and must not be required to feed pets safely.

## Minimum sensor set

```text
RFID reader x1
RFID tag x3
VL53L5CX x1
beam sensor x2
5 kg load cell x1
20 kg load cell x1
HX711 x2
limit switch x4
DS3231 x1
```
