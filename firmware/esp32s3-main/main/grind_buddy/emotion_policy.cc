#include "emotion_policy.h"

EmotionOutput EmotionOutputForState(InteractionState state) {
    switch (state) {
        case InteractionState::Idle:
            return {
                .emotion = "neutral",
                .motion = "settle",
                .sound = "",
                .status_text = "",
            };
        case InteractionState::Aware:
            return {
                .emotion = "curious",
                .motion = "notice_nod",
                .sound = "soft_ping",
                .status_text = "看到你了",
            };
        case InteractionState::ListeningWindow:
            return {
                .emotion = "listening",
                .motion = "lean_in",
                .sound = "focus_tick",
                .status_text = "我在看",
            };
        case InteractionState::Listening:
            return {
                .emotion = "listening",
                .motion = "hold_focus",
                .sound = "",
                .status_text = "",
            };
        case InteractionState::Thinking:
            return {
                .emotion = "thinking",
                .motion = "thinking_sway",
                .sound = "thinking_tick",
                .status_text = "理解中",
            };
        case InteractionState::Speaking:
            return {
                .emotion = "speaking",
                .motion = "talk_bounce",
                .sound = "",
                .status_text = "",
            };
        case InteractionState::Error:
            return {
                .emotion = "error",
                .motion = "shake",
                .sound = "error_tone",
                .status_text = "需要检查",
            };
    }

    return {
        .emotion = "neutral",
        .motion = "settle",
        .sound = "",
        .status_text = "",
    };
}
