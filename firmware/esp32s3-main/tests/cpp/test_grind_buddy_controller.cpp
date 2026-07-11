#include <cassert>

#include "../../main/grind_buddy/grind_buddy_controller.h"

namespace {

K230VisionEvent Event(const char* type, int64_t timestamp_ms) {
    K230VisionEvent event;
    event.type = type;
    event.ts = timestamp_ms;
    event.confidence = 1.0f;
    if (event.type == "face.pose") {
        event.center_x = 500;
        event.center_y = -250;
    }
    return event;
}

bool HasAction(
    const std::vector<GrindBuddyAction>& actions,
    GrindBuddyActionType type
) {
    for (const auto& action : actions) {
        if (action.type == type) {
            return true;
        }
    }
    return false;
}

const GrindBuddyAction* FindAction(
    const std::vector<GrindBuddyAction>& actions,
    GrindBuddyActionType type
) {
    for (const auto& action : actions) {
        if (action.type == type) {
            return &action;
        }
    }
    return nullptr;
}

}  // namespace

int main() {
    GrindBuddyController controller(true);

    controller.OnK230Event(Event("presence.enter", 1000));
    assert(controller.state() == InteractionState::Aware);
    controller.ClearActions();

    controller.OnK230Event(Event("gaze.enter", 1100));
    assert(controller.state() == InteractionState::ListeningWindow);
    assert(HasAction(controller.actions(), GrindBuddyActionType::OpenListeningWindow));
    assert(!HasAction(controller.actions(), GrindBuddyActionType::StartVoiceSession));
    controller.ClearActions();

    controller.OnK230Event(Event("face.pose", 1250));
    assert(!HasAction(controller.actions(), GrindBuddyActionType::StartVoiceSession));
    controller.ClearActions();

    controller.OnK230Event(Event("face.pose", 1500));
    assert(controller.state() == InteractionState::Listening);
    assert(HasAction(controller.actions(), GrindBuddyActionType::StartVoiceSession));
    controller.ClearActions();

    controller.OnK230Event(Event("gaze.leave", 1600));
    assert(controller.state() == InteractionState::Idle);
    assert(HasAction(controller.actions(), GrindBuddyActionType::StopVoiceSession));
    controller.ClearActions();

    controller.OnK230Event(Event("presence.enter", 1700));
    controller.ClearActions();
    controller.OnK230Event(Event("gaze.enter", 1800));
    controller.ClearActions();
    controller.OnK230Event(Event("face.pose", 2200));
    assert(!HasAction(controller.actions(), GrindBuddyActionType::StartVoiceSession));
    controller.ClearActions();

    controller.OnK230Event(Event("face.pose", 3800));
    assert(controller.state() == InteractionState::Listening);
    assert(HasAction(controller.actions(), GrindBuddyActionType::StartVoiceSession));

    return 0;
}
