#pragma once

struct FaceTrackingTarget {
  bool updated;
  float m0_degrees;
  float m1_degrees;
};

inline float FaceTrackingClamp(float value, float minimum, float maximum)
{
  if(value < minimum)
  {
    return minimum;
  }
  if(value > maximum)
  {
    return maximum;
  }
  return value;
}

inline FaceTrackingTarget ApplyFaceTrackingOffset(
    float center_x,
    float center_y,
    float current_m0_degrees,
    float current_m1_degrees)
{
  const float deadband = 60.0f;
  const float gain_degrees_per_unit = 0.003f;
  const bool horizontal_active = center_x <= -deadband || center_x >= deadband;
  const bool vertical_active = center_y <= -deadband || center_y >= deadband;

  if(!horizontal_active && !vertical_active)
  {
    return {false, current_m0_degrees, current_m1_degrees};
  }

  float next_m0 = current_m0_degrees;
  float next_m1 = current_m1_degrees;
  if(vertical_active)
  {
    next_m0 += center_y * gain_degrees_per_unit;
  }
  if(horizontal_active)
  {
    next_m1 += center_x * gain_degrees_per_unit;
  }

  return {
    true,
    FaceTrackingClamp(next_m0, -90.0f, 90.0f),
    FaceTrackingClamp(next_m1, -180.0f, 180.0f),
  };
}
