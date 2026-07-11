#pragma once

#include "interaction_state.h"

struct EmotionOutput {
    const char* emotion;
    const char* motion;
    const char* sound;
    const char* status_text;
};

EmotionOutput EmotionOutputForState(InteractionState state);
