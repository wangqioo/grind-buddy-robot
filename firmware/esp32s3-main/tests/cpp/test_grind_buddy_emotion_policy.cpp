#include <cassert>
#include <cstring>

#include "../../main/grind_buddy/emotion_policy.h"

int main() {
    const auto idle = EmotionOutputForState(InteractionState::Idle);
    assert(std::strcmp(idle.emotion, "neutral") == 0);
    assert(std::strcmp(idle.motion, "settle") == 0);
    assert(std::strcmp(idle.sound, "") == 0);
    assert(std::strcmp(idle.status_text, "") == 0);

    const auto noticed = EmotionOutputForState(InteractionState::Aware);
    assert(std::strcmp(noticed.emotion, "curious") == 0);
    assert(std::strcmp(noticed.motion, "notice_nod") == 0);
    assert(std::strcmp(noticed.sound, "soft_ping") == 0);
    assert(std::strcmp(noticed.status_text, "看到你了") == 0);

    const auto focused = EmotionOutputForState(InteractionState::ListeningWindow);
    assert(std::strcmp(focused.emotion, "listening") == 0);
    assert(std::strcmp(focused.motion, "lean_in") == 0);
    assert(std::strcmp(focused.sound, "focus_tick") == 0);
    assert(std::strcmp(focused.status_text, "我在看") == 0);

    const auto thinking = EmotionOutputForState(InteractionState::Thinking);
    assert(std::strcmp(thinking.emotion, "thinking") == 0);
    assert(std::strcmp(thinking.motion, "thinking_sway") == 0);
    assert(std::strcmp(thinking.sound, "thinking_tick") == 0);
    assert(std::strcmp(thinking.status_text, "理解中") == 0);

    return 0;
}
