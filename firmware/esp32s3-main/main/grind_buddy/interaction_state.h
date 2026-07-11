#pragma once

enum class InteractionState {
    Idle,
    Aware,
    ListeningWindow,
    Listening,
    Thinking,
    Speaking,
    Error,
};

enum class InteractionEvent {
    PresenceEnter,
    PresenceLeave,
    GazeEnter,
    GazeLeave,
    ListeningWindowTimeout,
    LocalVoiceDetected,
    ListenStop,
    TtsStart,
    TtsStop,
    NetworkError,
    Recover,
};

class InteractionStateMachine {
public:
    InteractionState state() const { return state_; }
    void Apply(InteractionEvent event);

private:
    InteractionState state_ = InteractionState::Idle;
};
