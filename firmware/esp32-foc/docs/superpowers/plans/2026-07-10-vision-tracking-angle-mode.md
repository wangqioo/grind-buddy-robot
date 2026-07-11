# Vision Tracking Angle Mode Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the existing serial angle control suitable for first-version vision tracking by accepting absolute angle targets, clamping them to safe limits, and ignoring tiny detector jitter.

**Architecture:** Keep the current single-file firmware structure in `src/main.cpp`. Reuse the proven DengFOC built-in position-speed cascade and only adjust the target-command layer that writes `M0_move_goal_angle` and `M1_move_goal_angle`.

**Tech Stack:** PlatformIO, Arduino ESP32, DengFOC, BMI160, AS5600.

---

### Task 1: Add Vision Angle Constants

**Files:**
- Modify: `src/main.cpp`

- [x] **Step 1: Add constants near the existing serial target settings**

Add these definitions after `SERIAL_DEBUG_INTERVAL_MS`:

```cpp
#define VISION_M0_MIN_DEG -90.0f
#define VISION_M0_MAX_DEG 90.0f
#define VISION_M1_MIN_DEG -180.0f
#define VISION_M1_MAX_DEG 180.0f
#define VISION_TARGET_DEADBAND_DEG 0.3f
```

- [x] **Step 2: Build**

Run: `pio run`

Expected: exit code `0`.

### Task 2: Clamp And Deadband Serial Targets

**Files:**
- Modify: `src/main.cpp`

- [x] **Step 1: Add a helper prototype**

Add this function prototype near the other target helpers:

```cpp
bool ApplyAxisTarget(bool set_axis, float requested_relative_deg, float min_deg, float max_deg, float home_angle, float &goal_angle);
```

- [x] **Step 2: Implement the helper before `StartTargetMove()`**

```cpp
bool ApplyAxisTarget(bool set_axis, float requested_relative_deg, float min_deg, float max_deg, float home_angle, float &goal_angle)
{
  if(!set_axis)
  {
    return false;
  }

  float clamped_deg = ClampFloat(requested_relative_deg, min_deg, max_deg);
  float current_goal_deg = (goal_angle - home_angle) * 180.0f / PI;
  if(fabs(clamped_deg - current_goal_deg) < VISION_TARGET_DEADBAND_DEG)
  {
    return false;
  }

  goal_angle = home_angle + clamped_deg * PI / 180.0f;
  return true;
}
```

- [x] **Step 3: Replace target assignment inside `StartTargetMove()`**

Use the helper for M0 and M1. Keep `target_move_active = true` so `UpdateSerialTargetMotion()` publishes the latest goal into the control loop.

```cpp
  bool m0_updated = ApplyAxisTarget(set_m0, m0_relative_deg, VISION_M0_MIN_DEG, VISION_M0_MAX_DEG, M0_home_angle, M0_move_goal_angle);
  bool m1_updated = ApplyAxisTarget(set_m1, m1_relative_deg, VISION_M1_MIN_DEG, VISION_M1_MAX_DEG, M1_home_angle, M1_move_goal_angle);
```

If neither axis updated, print status and return without changing the active target.

- [x] **Step 4: Build**

Run: `pio run`

Expected: exit code `0`.

### Task 3: Update Serial Help And Status

**Files:**
- Modify: `src/main.cpp`

- [x] **Step 1: Update `PrintSerialControlHelp()`**

Make the help text say `T` is absolute angle input and show the limits.

- [x] **Step 2: Update `PrintSerialTargetStatus()`**

Print target degrees and the active limits so the serial monitor confirms clamping behavior.

- [x] **Step 3: Build**

Run: `pio run`

Expected: exit code `0`.

### Task 4: Manual Hardware Verification

**Files:**
- No source changes.

- [ ] **Step 1: Upload when the board is connected**

Run:

```bash
pio device list
pio run -t upload --upload-port /dev/cu.wchusbserial1110
```

Expected: upload succeeds on the ESP32 V4 board.

- [ ] **Step 2: Serial command checks**

Send:

```text
H
T 10,-5
T 10.1,-5.1
T 120,220
S
```

Expected:

- `T 10,-5` moves both axes to absolute targets.
- `T 10.1,-5.1` is below the `0.3` degree deadband and does not visibly change the commanded target.
- `T 120,220` clamps to `M0=90`, `M1=180`.
- `S` reports the last valid target and limits.
