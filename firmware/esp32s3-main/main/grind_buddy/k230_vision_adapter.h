#pragma once

#include "k230_binary_protocol.h"
#include "k230_uart.h"

#include <cstdint>
#include <vector>

class K230VisionAdapter {
public:
    std::vector<K230VisionEvent> OnFace(
        const K230FaceObservation& observation,
        int64_t timestamp_ms
    );
    std::vector<K230VisionEvent> OnFaceLost(int64_t timestamp_ms);
    std::vector<K230VisionEvent> OnVisionTimeout(int64_t timestamp_ms);

private:
    static bool IsFacing(const K230FaceObservation& observation);
    static bool ShouldConfirmGazeEnter(int64_t first_facing_at_ms, int64_t timestamp_ms);
    static bool ShouldConfirmGazeLeave(int64_t first_left_at_ms, int64_t timestamp_ms);
    static K230VisionEvent MakePoseEvent(
        const K230FaceObservation& observation,
        int64_t timestamp_ms
    );

    bool face_present_ = false;
    bool gaze_present_ = false;
    int64_t gaze_enter_candidate_at_ms_ = 0;
    int64_t gaze_left_candidate_at_ms_ = 0;
};
