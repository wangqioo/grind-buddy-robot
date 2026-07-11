#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "../../main/grind_buddy/k230_binary_protocol.h"

static std::vector<uint8_t> Bytes(const char* hex) {
    std::vector<uint8_t> result;
    while (*hex) {
        char pair[3] = {hex[0], hex[1], 0};
        result.push_back(static_cast<uint8_t>(std::strtoul(pair, nullptr, 16)));
        hex += 2;
    }
    return result;
}

static std::vector<uint8_t> ReadHexFixture(const char* path) {
    std::ifstream input(path);
    assert(input.good());

    std::string hex;
    input >> hex;
    assert(!hex.empty());

    std::vector<uint8_t> result;
    for (std::size_t index = 0; index < hex.size(); index += 2) {
        char pair[3] = {hex[index], hex[index + 1], 0};
        result.push_back(static_cast<uint8_t>(std::strtoul(pair, nullptr, 16)));
    }
    return result;
}

int main() {
    K230BinaryParser parser;
    K230BinaryFrame frame;

    auto heartbeat = Bytes("a55a01010001000003e80000f3c7");
    bool ready = false;
    for (uint8_t byte : heartbeat) {
        ready = parser.Feed(byte, frame);
    }
    assert(ready);
    assert(frame.type == K230BinaryMessageType::Heartbeat);
    assert(frame.sequence == 1);
    assert(frame.timestamp_ms == 1000);
    assert(frame.payload_length == 0);

    auto face = Bytes("a55a01020002000003f2000f0000000001f401f40000041a00004bc36c");
    ready = false;
    for (uint8_t byte : face) {
        ready = parser.Feed(byte, frame);
    }
    assert(ready);
    assert(frame.type == K230BinaryMessageType::Face);
    assert(frame.sequence == 2);

    K230FaceObservation observation;
    assert(DecodeK230Face(frame, observation));
    assert(observation.center_x == 0);
    assert(observation.center_y == 0);
    assert(observation.width == 500);
    assert(observation.height == 500);
    assert(observation.pitch_centidegrees == 0);
    assert(observation.yaw_centidegrees == 1050);
    assert(observation.roll_centidegrees == 0);
    assert(observation.confidence == 75);

    auto error = Bytes("a55a017f0004000004060002002aa7b0");
    ready = false;
    for (uint8_t byte : error) {
        ready = parser.Feed(byte, frame);
    }
    assert(ready);
    assert(frame.type == K230BinaryMessageType::Error);
    uint16_t error_code = 0;
    assert(DecodeK230Error(frame, error_code));
    assert(error_code == 42);

    auto corrupt = heartbeat;
    corrupt.back() ^= 0xff;
    ready = false;
    for (uint8_t byte : corrupt) {
        ready = parser.Feed(byte, frame) || ready;
    }
    assert(!ready);

    for (uint8_t byte : heartbeat) {
        ready = parser.Feed(byte, frame) || ready;
    }
    assert(ready);
    assert(frame.type == K230BinaryMessageType::Heartbeat);

    auto fixture = ReadHexFixture("tests/fixtures/k230_uart_stream.hex");
    parser.Reset();
    std::vector<K230BinaryFrame> frames;
    for (uint8_t byte : fixture) {
        if (parser.Feed(byte, frame)) {
            frames.push_back(frame);
        }
    }

    assert(frames.size() == 3);
    assert(frames[0].type == K230BinaryMessageType::Heartbeat);
    assert(frames[0].sequence == 10);
    assert(frames[0].timestamp_ms == 10000);

    assert(frames[1].type == K230BinaryMessageType::Face);
    assert(frames[1].sequence == 11);
    assert(frames[1].timestamp_ms == 10040);
    assert(DecodeK230Face(frames[1], observation));
    assert(observation.center_x == 120);
    assert(observation.center_y == -80);
    assert(observation.width == 420);
    assert(observation.height == 380);
    assert(observation.pitch_centidegrees == 250);
    assert(observation.yaw_centidegrees == -1800);
    assert(observation.roll_centidegrees == 100);
    assert(observation.confidence == 88);

    assert(frames[2].type == K230BinaryMessageType::FaceLost);
    assert(frames[2].sequence == 13);
    assert(frames[2].timestamp_ms == 10120);
    assert(frames[2].payload_length == 0);

    return 0;
}
