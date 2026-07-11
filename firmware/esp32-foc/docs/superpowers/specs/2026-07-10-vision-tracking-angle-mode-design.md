# Vision Tracking Angle Mode Design

## Goal

Add the first usable vision-tracking gimbal mode without changing the proven motor feel.

The vision side sends absolute target angles over serial only when it detects a target. The ESP32 keeps driving both motors to the latest valid target angle with DengFOC's built-in position-speed cascade.

## Current Baseline

The current firmware already has a stable serial target path:

- `SERIAL_TARGET_CONTROL` is enabled.
- `T` commands set target angles relative to power-on home.
- The control loop repeatedly calls:
  - `DFOC_M0_set_Velocity_Angle(M0_target_angle)`
  - `DFOC_M1_set_Velocity_Angle(M1_target_angle)`
- The selected motor feel comes from DengFOC's built-in position-speed cascade, not the older custom cascade.

This design keeps that baseline.

## Requirements

- The vision side sends absolute angles, not pixel offsets and not incremental angles.
- Command frequency is not fixed. The vision side sends only when a target is detected.
- No new command means the gimbal holds the last valid target.
- `M0` is the vertical axis.
- `M1` is the horizontal axis.
- Angles are degrees relative to the power-on home position.
- First-version limits:
  - `M0`: `-90` to `+90` degrees.
  - `M1`: `-180` to `+180` degrees.
- A small angle deadband prevents detector jitter from constantly updating motor targets.
- Keep manual debug commands available.

## Serial Protocol

Use the existing `T` command as the official first-version vision angle command.

Examples:

```text
T 10,-5
T10,-5
T 30
M0 20
M1 -45
H
S
?
```

Command meanings:

- `T m0,m1`: set both axes to absolute target angles.
- `T angle`: set both axes to the same absolute target angle.
- `M0 angle`: set only M0.
- `M1 angle`: set only M1.
- `H`: return both axes to home angle.
- `S`: print status.
- `?`: print help.

The implementation should accept optional spaces so the vision side does not need strict formatting.

## Target Handling

When a valid angle command arrives:

1. Parse the requested target angle in degrees.
2. Clamp it to the configured axis limit.
3. Compare it with the current commanded relative target.
4. If the change is smaller than the deadband, ignore the update.
5. Otherwise convert degrees to radians relative to the home angle.
6. Store the result in `M0_target_angle` and/or `M1_target_angle`.

The loop then continuously calls DengFOC's position-speed cascade. There is no S-curve, no internal angle ramp, and no pixel-to-angle conversion in this version.

## Defaults

Recommended first-version constants:

```cpp
#define VISION_M0_MIN_DEG -90.0f
#define VISION_M0_MAX_DEG 90.0f
#define VISION_M1_MIN_DEG -180.0f
#define VISION_M1_MAX_DEG 180.0f
#define VISION_TARGET_DEADBAND_DEG 0.3f
```

`0.3` degree deadband is small enough to keep tracking responsive, but large enough to reduce tiny detector noise.

## Lost Target Behavior

The ESP32 does not need a special lost-target command for the first version.

If the vision side stops sending angles, the ESP32 keeps holding the last valid target. This avoids sudden return-home motion when detection flickers.

A later version can add a timeout behavior if needed:

- hold last target,
- slowly return home,
- or switch to manual mode.

## Error Handling

Invalid or incomplete serial input should be ignored without changing the current target.

Out-of-range angles should be clamped, not rejected. This keeps the gimbal safe if the vision side briefly sends a bad value.

Status output should show:

- current relative M0 target in degrees,
- current relative M1 target in degrees,
- current motor angles,
- whether the latest command was clamped or ignored by deadband if this is easy to expose.

## Out Of Scope

This version does not include:

- pixel-offset tracking,
- camera field-of-view conversion,
- target prediction,
- S-curve motion planning,
- automatic IMU stabilization,
- serial checksum or packet framing,
- camera-side detection code.

## Verification Plan

1. Build the firmware with PlatformIO.
2. Upload to the ESP32 V4 board.
3. Send `H` and confirm both axes hold home.
4. Send `T 10,-5` and confirm both axes move to the requested absolute angles.
5. Send the same command repeatedly and confirm there is no visible jitter from repeated identical targets.
6. Send a tiny change below the deadband and confirm the target does not update.
7. Send `T 120,220` and confirm M0 clamps to `90`, M1 clamps to `180`.
8. Stop sending commands and confirm the gimbal keeps holding the last target.

