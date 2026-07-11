#include <cassert>
#include <string>
#include <vector>

#include "../../main/grind_buddy/k230_vision_adapter.h"

static bool HasEvent(const std::vector<K230VisionEvent>& events, const char* type) {
    for (const auto& event : events) {
        if (event.type == type) {
            return true;
        }
    }
    return false;
}

static const K230VisionEvent& FindEvent(
    const std::vector<K230VisionEvent>& events,
    const char* type
) {
    for (const auto& event : events) {
        if (event.type == type) {
            return event;
        }
    }
    assert(false);
    return events[0];
}

int main() {
    K230VisionAdapter adapter;

    K230FaceObservation frontal_face = {};
    frontal_face.center_x = 120;
    frontal_face.center_y = -80;
    frontal_face.width = 500;
    frontal_face.height = 500;
    frontal_face.pitch_centidegrees = 200;
    frontal_face.yaw_centidegrees = 1000;
    frontal_face.roll_centidegrees = 0;
    frontal_face.confidence = 75;

    std::vector<K230VisionEvent> events = adapter.OnFace(frontal_face, 1000);
    assert(events.size() == 2);
    assert(HasEvent(events, "presence.enter"));
    assert(!HasEvent(events, "gaze.enter"));
    assert(HasEvent(events, "face.pose"));

    const auto& pose = FindEvent(events, "face.pose");
    assert(pose.ts == 1000);
    assert(pose.yaw == 10.0f);
    assert(pose.pitch == 2.0f);
    assert(pose.roll == 0.0f);
    assert(pose.confidence == 0.75f);
    assert(pose.center_x == 120);
    assert(pose.center_y == -80);
    assert(pose.face_width == 500);
    assert(pose.face_height == 500);

    events = adapter.OnFace(frontal_face, 1010);
    assert(events.size() == 1);
    assert(!HasEvent(events, "gaze.enter"));
    assert(HasEvent(events, "face.pose"));

    events = adapter.OnFace(frontal_face, 1320);
    assert(events.size() == 2);
    assert(HasEvent(events, "gaze.enter"));
    assert(HasEvent(events, "face.pose"));

    K230FaceObservation side_face = frontal_face;
    side_face.yaw_centidegrees = 4500;
    events = adapter.OnFace(side_face, 1340);
    assert(events.size() == 1);
    assert(!HasEvent(events, "gaze.leave"));
    assert(HasEvent(events, "face.pose"));

    events = adapter.OnFace(side_face, 1350);
    assert(events.size() == 1);
    assert(!HasEvent(events, "gaze.leave"));
    assert(HasEvent(events, "face.pose"));

    events = adapter.OnFace(frontal_face, 1360);
    assert(events.size() == 1);
    assert(!HasEvent(events, "gaze.enter"));
    assert(HasEvent(events, "face.pose"));

    events = adapter.OnFace(side_face, 1520);
    assert(events.size() == 1);
    assert(!HasEvent(events, "gaze.leave"));
    assert(HasEvent(events, "face.pose"));

    events = adapter.OnFace(side_face, 1990);
    assert(events.size() == 1);
    assert(!HasEvent(events, "gaze.leave"));
    assert(HasEvent(events, "face.pose"));

    events = adapter.OnFace(side_face, 2030);
    assert(events.size() == 2);
    assert(HasEvent(events, "gaze.leave"));
    assert(HasEvent(events, "face.pose"));

    events = adapter.OnFaceLost(1600);
    assert(events.size() == 1);
    assert(HasEvent(events, "presence.leave"));

    events = adapter.OnFaceLost(1700);
    assert(events.empty());

    events = adapter.OnVisionTimeout(2200);
    assert(events.empty());

    adapter.OnFace(frontal_face, 3000);
    events = adapter.OnVisionTimeout(3600);
    assert(events.size() == 1);
    assert(HasEvent(events, "presence.leave"));

    return 0;
}
