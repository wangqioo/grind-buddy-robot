#include "k230_binary_protocol.h"

#include <cstring>

namespace {

constexpr uint8_t kMagic0 = 0xA5;
constexpr uint8_t kMagic1 = 0x5A;
constexpr std::size_t kFacePayloadSize = 15;
constexpr std::size_t kErrorPayloadSize = 2;

uint16_t ReadU16(const uint8_t* data) {
    return static_cast<uint16_t>(
        (static_cast<uint16_t>(data[0]) << 8) | data[1]
    );
}

int16_t ReadI16(const uint8_t* data) {
    return static_cast<int16_t>(ReadU16(data));
}

uint32_t ReadU32(const uint8_t* data) {
    return (static_cast<uint32_t>(data[0]) << 24) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) |
           static_cast<uint32_t>(data[3]);
}

}  // namespace

uint16_t K230Crc16Ccitt(const uint8_t* data, std::size_t length) {
    uint16_t crc = 0xFFFF;
    for (std::size_t index = 0; index < length; ++index) {
        crc ^= static_cast<uint16_t>(data[index]) << 8;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000)
                      ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
                      : static_cast<uint16_t>(crc << 1);
        }
    }
    return crc;
}

void K230BinaryParser::Reset() {
    length_ = 0;
    expected_length_ = 0;
}

bool K230BinaryParser::Feed(uint8_t byte, K230BinaryFrame& frame) {
    if (length_ == 0) {
        if (byte == kMagic0) {
            buffer_[length_++] = byte;
        }
        return false;
    }

    if (length_ == 1) {
        if (byte == kMagic1) {
            buffer_[length_++] = byte;
        } else if (byte == kMagic0) {
            buffer_[0] = byte;
        } else {
            Reset();
        }
        return false;
    }

    if (length_ >= K230_BINARY_MAX_FRAME_SIZE) {
        Reset();
        return false;
    }
    buffer_[length_++] = byte;

    if (length_ == K230_BINARY_HEADER_SIZE) {
        if (buffer_[2] != K230_BINARY_VERSION) {
            Reset();
            return false;
        }

        const uint16_t payload_length = ReadU16(&buffer_[10]);
        if (payload_length > K230_BINARY_MAX_PAYLOAD_SIZE) {
            Reset();
            return false;
        }
        expected_length_ =
            K230_BINARY_HEADER_SIZE + payload_length + K230_BINARY_CRC_SIZE;
    }

    if (expected_length_ == 0 || length_ < expected_length_) {
        return false;
    }

    const uint16_t expected_crc =
        ReadU16(&buffer_[expected_length_ - K230_BINARY_CRC_SIZE]);
    const uint16_t actual_crc =
        K230Crc16Ccitt(buffer_, expected_length_ - K230_BINARY_CRC_SIZE);
    if (expected_crc != actual_crc) {
        Reset();
        return false;
    }

    frame.type = static_cast<K230BinaryMessageType>(buffer_[3]);
    frame.sequence = ReadU16(&buffer_[4]);
    frame.timestamp_ms = ReadU32(&buffer_[6]);
    frame.payload_length = ReadU16(&buffer_[10]);
    if (frame.payload_length > 0) {
        std::memcpy(
            frame.payload,
            &buffer_[K230_BINARY_HEADER_SIZE],
            frame.payload_length
        );
    }

    Reset();
    return true;
}

bool DecodeK230Face(
    const K230BinaryFrame& frame,
    K230FaceObservation& observation
) {
    if (frame.type != K230BinaryMessageType::Face ||
        frame.payload_length != kFacePayloadSize) {
        return false;
    }

    observation.center_x = ReadI16(&frame.payload[0]);
    observation.center_y = ReadI16(&frame.payload[2]);
    observation.width = ReadI16(&frame.payload[4]);
    observation.height = ReadI16(&frame.payload[6]);
    observation.pitch_centidegrees = ReadI16(&frame.payload[8]);
    observation.yaw_centidegrees = ReadI16(&frame.payload[10]);
    observation.roll_centidegrees = ReadI16(&frame.payload[12]);
    observation.confidence = frame.payload[14];
    return true;
}

bool DecodeK230Error(const K230BinaryFrame& frame, uint16_t& error_code) {
    if (frame.type != K230BinaryMessageType::Error ||
        frame.payload_length != kErrorPayloadSize) {
        return false;
    }

    error_code = ReadU16(&frame.payload[0]);
    return true;
}
