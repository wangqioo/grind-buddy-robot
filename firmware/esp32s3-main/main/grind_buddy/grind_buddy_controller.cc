#include "grind_buddy_controller.h"

#include "grind_buddy_config.h"

GrindBuddyController::GrindBuddyController(bool start_voice_on_gaze)
    : start_voice_on_gaze_(start_voice_on_gaze) {
}

void GrindBuddyController::ClearActions() {
    actions_.clear();
}

void GrindBuddyController::OnK230Event(const K230VisionEvent& event) {
    if (event.type != "face.pose") {
        AddAction(GrindBuddyActionType::ReportVisionEvent, event.type);
    }

    if (event.type == "presence.enter") {
        Apply(InteractionEvent::PresenceEnter);
    } else if (event.type == "presence.leave") {
        gaze_stabilizing_ = false;
        if (voice_session_active_) {
            AddAction(GrindBuddyActionType::StopVoiceSession, event.type);
            voice_session_active_ = false;
            last_voice_session_stopped_at_ms_ = event.ts;
        }
        Apply(InteractionEvent::PresenceLeave);
    } else if (event.type == "gaze.enter") {
        InteractionState previous = state();
        state_machine_.Apply(InteractionEvent::GazeEnter);
        if (state() == InteractionState::ListeningWindow && previous != state()) {
            AddAction(GrindBuddyActionType::OpenListeningWindow, event.type);
        }
        if (start_voice_on_gaze_) {
            gaze_stabilizing_ = true;
            gaze_entered_at_ms_ = event.ts;
            MaybeStartVoiceSession(event.type, event.ts);
        }
        UpdateDisplayIfChanged(previous);
    } else if (event.type == "gaze.leave") {
        gaze_stabilizing_ = false;
        if (voice_session_active_) {
            AddAction(GrindBuddyActionType::StopVoiceSession, event.type);
            voice_session_active_ = false;
            last_voice_session_stopped_at_ms_ = event.ts;
        }
        Apply(InteractionEvent::GazeLeave);
    } else if (event.type == "face.pose") {
        MaybeStartVoiceSession(event.type, event.ts);
    }
}

void GrindBuddyController::OnListeningWindowTimeout() {
    Apply(InteractionEvent::ListeningWindowTimeout);
}

void GrindBuddyController::OnLocalVoiceDetected() {
    InteractionState previous = state();
    state_machine_.Apply(InteractionEvent::LocalVoiceDetected);
    if (state() == InteractionState::Listening && previous != state()) {
        AddAction(GrindBuddyActionType::StartVoiceSession);
    }
    UpdateDisplayIfChanged(previous);
}

void GrindBuddyController::OnVoiceListenStopped() {
    InteractionState previous = state();
    state_machine_.Apply(InteractionEvent::ListenStop);
    if (state() == InteractionState::Thinking && previous != state()) {
        AddAction(GrindBuddyActionType::StopVoiceSession);
        voice_session_active_ = false;
    }
    UpdateDisplayIfChanged(previous);
}

void GrindBuddyController::OnTtsStarted() {
    Apply(InteractionEvent::TtsStart);
}

void GrindBuddyController::OnTtsStopped() {
    Apply(InteractionEvent::TtsStop);
}

void GrindBuddyController::OnNetworkError() {
    Apply(InteractionEvent::NetworkError);
}

void GrindBuddyController::OnRecovered() {
    Apply(InteractionEvent::Recover);
}

void GrindBuddyController::MaybeStartVoiceSession(
    const std::string& event_type,
    int64_t timestamp_ms
) {
    if (!start_voice_on_gaze_ ||
        !gaze_stabilizing_ ||
        voice_session_active_ ||
        state() != InteractionState::ListeningWindow) {
        return;
    }

    if (timestamp_ms - gaze_entered_at_ms_ <
        grind_buddy_config::kGazeStableBeforeWakeMs) {
        return;
    }

    if (timestamp_ms - last_voice_session_stopped_at_ms_ <
        grind_buddy_config::kWakeCooldownMs) {
        return;
    }

    InteractionState previous = state();
    state_machine_.Apply(InteractionEvent::LocalVoiceDetected);
    if (state() == InteractionState::Listening && previous != state()) {
        voice_session_active_ = true;
        gaze_stabilizing_ = false;
        AddAction(GrindBuddyActionType::StartVoiceSession, event_type);
    }
}

void GrindBuddyController::Apply(InteractionEvent event) {
    InteractionState previous = state();
    state_machine_.Apply(event);
    UpdateDisplayIfChanged(previous);
}

void GrindBuddyController::AddAction(GrindBuddyActionType type, const std::string& event_type) {
    actions_.push_back(GrindBuddyAction{
        .type = type,
        .state = state(),
        .event_type = event_type,
    });
}

void GrindBuddyController::UpdateDisplayIfChanged(InteractionState previous) {
    if (previous != state()) {
        AddAction(GrindBuddyActionType::SetDisplayState);
    }
}
