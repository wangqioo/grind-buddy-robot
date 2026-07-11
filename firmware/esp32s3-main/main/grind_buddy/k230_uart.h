#pragma once

#include <cstdint>
#include <functional>
#include <string>

struct K230VisionEvent {
    std::string type;
    int64_t ts = 0;
    int16_t center_x = 0;
    int16_t center_y = 0;
    int16_t face_width = 0;
    int16_t face_height = 0;
    float yaw = 0.0f;
    float pitch = 0.0f;
    float roll = 0.0f;
    float confidence = 0.0f;
};

class K230JsonLineParser {
public:
    using Callback = std::function<void(const K230VisionEvent&)>;

    explicit K230JsonLineParser(Callback callback) : callback_(callback) {}
    void Feed(char c);

private:
    static constexpr size_t kMaxLineLength = 256;

    void ParseLine(const std::string& line);

    Callback callback_;
    std::string buffer_;
    bool dropping_line_ = false;
};
