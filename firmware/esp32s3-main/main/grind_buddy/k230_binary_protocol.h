#pragma once

#include <cstddef>
#include <cstdint>

constexpr uint8_t K230_BINARY_VERSION = 1;
constexpr std::size_t K230_BINARY_HEADER_SIZE = 12;
constexpr std::size_t K230_BINARY_CRC_SIZE = 2;
constexpr std::size_t K230_BINARY_MAX_PAYLOAD_SIZE = 256;
constexpr std::size_t K230_BINARY_MAX_FRAME_SIZE =
    K230_BINARY_HEADER_SIZE + K230_BINARY_MAX_PAYLOAD_SIZE + K230_BINARY_CRC_SIZE;

enum class K230BinaryMessageType : uint8_t {
    Heartbeat = 1,
    Face = 2,
    FaceLost = 3,
    Error = 127,
};

struct K230BinaryFrame {
    K230BinaryMessageType type = K230BinaryMessageType::Heartbeat;
    uint16_t sequence = 0;
    uint32_t timestamp_ms = 0;
    uint16_t payload_length = 0;
    uint8_t payload[K230_BINARY_MAX_PAYLOAD_SIZE] = {};
};

struct K230FaceObservation {
    int16_t center_x = 0;
    int16_t center_y = 0;
    int16_t width = 0;
    int16_t height = 0;
    int16_t pitch_centidegrees = 0;
    int16_t yaw_centidegrees = 0;
    int16_t roll_centidegrees = 0;
    uint8_t confidence = 0;
};

uint16_t K230Crc16Ccitt(const uint8_t* data, std::size_t length);
bool DecodeK230Face(const K230BinaryFrame& frame, K230FaceObservation& observation);
bool DecodeK230Error(const K230BinaryFrame& frame, uint16_t& error_code);

class K230BinaryParser {
public:
    bool Feed(uint8_t byte, K230BinaryFrame& frame);
    void Reset();

private:
    uint8_t buffer_[K230_BINARY_MAX_FRAME_SIZE] = {};
    std::size_t length_ = 0;
    std::size_t expected_length_ = 0;
};
