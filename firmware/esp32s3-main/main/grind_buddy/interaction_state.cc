#include "interaction_state.h"

void InteractionStateMachine::Apply(InteractionEvent event) {
    if (event == InteractionEvent::NetworkError) {
        state_ = InteractionState::Error;
        return;
    }

    if (state_ == InteractionState::Error) {
        if (event == InteractionEvent::Recover) {
            state_ = InteractionState::Idle;
        }
        return;
    }

    switch (state_) {
        case InteractionState::Idle:
            if (event == InteractionEvent::PresenceEnter) {
                state_ = InteractionState::Aware;
            } else if (event == InteractionEvent::GazeEnter) {
                state_ = InteractionState::ListeningWindow;
            }
            break;

        case InteractionState::Aware:
            if (event == InteractionEvent::GazeEnter) {
                state_ = InteractionState::ListeningWindow;
            } else if (event == InteractionEvent::PresenceLeave) {
                state_ = InteractionState::Idle;
            }
            break;

        case InteractionState::ListeningWindow:
            if (event == InteractionEvent::LocalVoiceDetected) {
                state_ = InteractionState::Listening;
            } else if (event == InteractionEvent::GazeLeave ||
                       event == InteractionEvent::PresenceLeave) {
                state_ = InteractionState::Idle;
            } else if (event == InteractionEvent::ListeningWindowTimeout) {
                state_ = InteractionState::Aware;
            }
            break;

        case InteractionState::Listening:
            if (event == InteractionEvent::ListenStop) {
                state_ = InteractionState::Thinking;
            } else if (event == InteractionEvent::GazeLeave ||
                       event == InteractionEvent::PresenceLeave) {
                state_ = InteractionState::Idle;
            } else if (event == InteractionEvent::TtsStart) {
                state_ = InteractionState::Speaking;
            }
            break;

        case InteractionState::Thinking:
            if (event == InteractionEvent::TtsStart) {
                state_ = InteractionState::Speaking;
            } else if (event == InteractionEvent::TtsStop ||
                       event == InteractionEvent::PresenceLeave) {
                state_ = InteractionState::Idle;
            }
            break;

        case InteractionState::Speaking:
            if (event == InteractionEvent::TtsStop) {
                state_ = InteractionState::Aware;
            } else if (event == InteractionEvent::PresenceLeave) {
                state_ = InteractionState::Idle;
            }
            break;

        case InteractionState::Error:
            break;
    }
}
