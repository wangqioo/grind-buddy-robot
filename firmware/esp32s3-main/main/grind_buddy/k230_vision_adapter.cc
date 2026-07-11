#include "k230_vision_adapter.h"

#include "grind_buddy_config.h"

#include <cmath>

namespace {

float ConfidenceToUnit(uint8_t confidence) {
    return static_cast<float>(confidence) / 100.0f;
}

float CentidegreesToDegrees(int16_t value) {
    return static_cast<float>(value) / 100.0f;
}

}  // namespace

std::vector<K230VisionEvent> K230VisionAdapter::OnFace(
    const K230FaceObservation& observation,
    int64_t timestamp_ms
) {
    std::vector<K230VisionEvent> events;

    if (!face_present_) {
        events.push_back(K230VisionEvent{
            .type = "presence.enter",
            .ts = timestamp_ms,
            .confidence = ConfidenceToUnit(observation.confidence),
        });
        face_present_ = true;
    }

    const bool is_facing = IsFacing(observation);
    if (is_facing) {
        gaze_left_candidate_at_ms_ = 0;
        if (!gaze_present_) {
            if (gaze_enter_candidate_at_ms_ == 0) {
                gaze_enter_candidate_at_ms_ = timestamp_ms;
            }
            if (ShouldConfirmGazeEnter(gaze_enter_candidate_at_ms_, timestamp_ms)) {
                events.push_back(K230VisionEvent{
                    .type = "gaze.enter",
                    .ts = timestamp_ms,
                    .confidence = ConfidenceToUnit(observation.confidence),
                });
                gaze_present_ = true;
                gaze_enter_candidate_at_ms_ = 0;
            }
        }
    } else {
        gaze_enter_candidate_at_ms_ = 0;
        if (gaze_present_) {
            if (gaze_left_candidate_at_ms_ == 0) {
                gaze_left_candidate_at_ms_ = timestamp_ms;
            }
            if (ShouldConfirmGazeLeave(gaze_left_candidate_at_ms_, timestamp_ms)) {
                events.push_back(K230VisionEvent{
                    .type = "gaze.leave",
                    .ts = timestamp_ms,
                    .confidence = ConfidenceToUnit(observation.confidence),
                });
                gaze_present_ = false;
                gaze_left_candidate_at_ms_ = 0;
            }
        }
    }

    events.push_back(MakePoseEvent(observation, timestamp_ms));
    return events;
}

std::vector<K230VisionEvent> K230VisionAdapter::OnFaceLost(int64_t timestamp_ms) {
    if (!face_present_ && !gaze_present_) {
        return {};
    }

    std::vector<K230VisionEvent> events;
    events.push_back(K230VisionEvent{
        .type = "presence.leave",
        .ts = timestamp_ms,
        .confidence = 0.0f,
    });

    face_present_ = false;
    gaze_present_ = false;
    gaze_enter_candidate_at_ms_ = 0;
    gaze_left_candidate_at_ms_ = 0;
    return events;
}

std::vector<K230VisionEvent> K230VisionAdapter::OnVisionTimeout(
    int64_t timestamp_ms
) {
    return OnFaceLost(timestamp_ms);
}

bool K230VisionAdapter::IsFacing(const K230FaceObservation& observation) {
    return std::abs(observation.yaw_centidegrees) <=
               grind_buddy_config::kFacingYawLimitCentidegrees &&
           std::abs(observation.pitch_centidegrees) <=
               grind_buddy_config::kFacingPitchLimitCentidegrees;
}

bool K230VisionAdapter::ShouldConfirmGazeEnter(
    int64_t first_facing_at_ms,
    int64_t timestamp_ms
) {
    return timestamp_ms - first_facing_at_ms >=
           grind_buddy_config::kGazeEnterHysteresisMs;
}

bool K230VisionAdapter::ShouldConfirmGazeLeave(
    int64_t first_left_at_ms,
    int64_t timestamp_ms
) {
    return timestamp_ms - first_left_at_ms >=
           grind_buddy_config::kGazeLeaveHysteresisMs;
}

K230VisionEvent K230VisionAdapter::MakePoseEvent(
    const K230FaceObservation& observation,
    int64_t timestamp_ms
) {
    return K230VisionEvent{
        .type = "face.pose",
        .ts = timestamp_ms,
        .center_x = observation.center_x,
        .center_y = observation.center_y,
        .face_width = observation.width,
        .face_height = observation.height,
        .yaw = CentidegreesToDegrees(observation.yaw_centidegrees),
        .pitch = CentidegreesToDegrees(observation.pitch_centidegrees),
        .roll = CentidegreesToDegrees(observation.roll_centidegrees),
        .confidence = ConfidenceToUnit(observation.confidence),
    };
}
