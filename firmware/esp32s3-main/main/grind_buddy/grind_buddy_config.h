#pragma once

#include <cstdint>

namespace grind_buddy_config {

inline constexpr int16_t kFacingYawLimitCentidegrees = 3000;
inline constexpr int16_t kFacingPitchLimitCentidegrees = 3000;
inline constexpr int64_t kGazeEnterHysteresisMs = 300;
inline constexpr int64_t kGazeLeaveHysteresisMs = 500;
inline constexpr int64_t kGazeStableBeforeWakeMs = 400;
inline constexpr int64_t kWakeCooldownMs = 2000;
inline constexpr int64_t kListeningWindowTimeoutUs = 5 * 1000 * 1000;
inline constexpr uint32_t kVisionTimeoutMs = 2000;

}  // namespace grind_buddy_config
