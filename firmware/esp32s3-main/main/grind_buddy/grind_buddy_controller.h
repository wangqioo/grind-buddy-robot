#pragma once

#include "interaction_state.h"
#include "k230_uart.h"

#include <string>
#include <vector>

enum class GrindBuddyActionType {
    ReportVisionEvent,
    OpenListeningWindow,
    StartVoiceSession,
    StopVoiceSession,
    SetDisplayState,
};

struct GrindBuddyAction {
    GrindBuddyActionType type;
    InteractionState state;
    std::string event_type;
};

class GrindBuddyController {
public:
    explicit GrindBuddyController(bool start_voice_on_gaze = false);

    InteractionState state() const { return state_machine_.state(); }
    const std::vector<GrindBuddyAction>& actions() const { return actions_; }

    void ClearActions();
    void OnK230Event(const K230VisionEvent& event);
    void OnListeningWindowTimeout();
    void OnLocalVoiceDetected();
    void OnVoiceListenStopped();
    void OnTtsStarted();
    void OnTtsStopped();
    void OnNetworkError();
    void OnRecovered();

private:
    void MaybeStartVoiceSession(const std::string& event_type, int64_t timestamp_ms);
    void Apply(InteractionEvent event);
    void AddAction(GrindBuddyActionType type, const std::string& event_type = "");
    void UpdateDisplayIfChanged(InteractionState previous);

    InteractionStateMachine state_machine_;
    std::vector<GrindBuddyAction> actions_;
    bool start_voice_on_gaze_ = false;
    bool voice_session_active_ = false;
    bool gaze_stabilizing_ = false;
    int64_t gaze_entered_at_ms_ = 0;
    int64_t last_voice_session_stopped_at_ms_ = -3000;
};
