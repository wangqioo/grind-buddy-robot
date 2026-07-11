#include <cassert>
#include <cmath>

#include "../src/face_tracking_policy.h"

int main() {
    FaceTrackingTarget target = ApplyFaceTrackingOffset(40.0f, -40.0f, 0.0f, 0.0f);
    assert(!target.updated);
    assert(target.m0_degrees == 0.0f);
    assert(target.m1_degrees == 0.0f);

    target = ApplyFaceTrackingOffset(200.0f, -100.0f, 0.0f, 0.0f);
    assert(target.updated);
    assert(std::fabs(target.m0_degrees - -0.3f) < 0.001f);
    assert(std::fabs(target.m1_degrees - 0.6f) < 0.001f);

    target = ApplyFaceTrackingOffset(100000.0f, -100000.0f, 0.0f, 0.0f);
    assert(target.updated);
    assert(target.m0_degrees == -90.0f);
    assert(target.m1_degrees == 180.0f);

    return 0;
}
