#include "k230_uart.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

#ifndef GRIND_BUDDY_HOST_TEST
#include <cJSON.h>
#endif

namespace {

bool IsKnownEventType(const std::string& type) {
    return type == "presence.enter" ||
           type == "presence.leave" ||
           type == "gaze.enter" ||
           type == "gaze.leave" ||
           type == "face.pose";
}

float ClampConfidence(float value) {
    return std::max(0.0f, std::min(1.0f, value));
}

#ifdef GRIND_BUDDY_HOST_TEST
bool LooksLikeJsonObject(const std::string& line) {
    return line.size() >= 2 && line.front() == '{' && line.back() == '}';
}

bool ExtractString(const std::string& line, const char* key, std::string& value) {
    std::string needle = std::string("\"") + key + "\":\"";
    size_t start = line.find(needle);
    if (start == std::string::npos) {
        return false;
    }
    start += needle.size();
    size_t end = line.find('"', start);
    if (end == std::string::npos) {
        return false;
    }
    value = line.substr(start, end - start);
    return true;
}

bool ExtractNumber(const std::string& line, const char* key, double& value) {
    std::string needle = std::string("\"") + key + "\":";
    size_t start = line.find(needle);
    if (start == std::string::npos) {
        return false;
    }
    start += needle.size();

    const char* begin = line.c_str() + start;
    char* end = nullptr;
    value = std::strtod(begin, &end);
    if (end == begin) {
        return false;
    }
    while (*end != '\0' && std::isspace(static_cast<unsigned char>(*end))) {
        ++end;
    }
    return *end == ',' || *end == '}';
}
#endif

}  // namespace

void K230JsonLineParser::Feed(char c) {
    if (c == '\n') {
        if (!dropping_line_) {
            ParseLine(buffer_);
        }
        buffer_.clear();
        dropping_line_ = false;
        return;
    }

    if (c == '\r') {
        return;
    }

    if (dropping_line_) {
        return;
    }

    if (buffer_.size() >= kMaxLineLength) {
        buffer_.clear();
        dropping_line_ = true;
        return;
    }

    buffer_.push_back(c);
}

void K230JsonLineParser::ParseLine(const std::string& line) {
    if (line.empty()) {
        return;
    }

#ifdef GRIND_BUDDY_HOST_TEST
    if (!LooksLikeJsonObject(line)) {
        return;
    }

    K230VisionEvent event;
    double numeric = 0.0;

    if (!ExtractString(line, "type", event.type) ||
        !ExtractNumber(line, "ts", numeric)) {
        return;
    }
    event.ts = static_cast<int64_t>(numeric);

    if (!IsKnownEventType(event.type)) {
        return;
    }

    if (ExtractNumber(line, "confidence", numeric)) {
        event.confidence = ClampConfidence(static_cast<float>(numeric));
    }

    if (event.type == "face.pose") {
        if (!ExtractNumber(line, "yaw", numeric)) {
            return;
        }
        event.yaw = static_cast<float>(numeric);

        if (!ExtractNumber(line, "pitch", numeric)) {
            return;
        }
        event.pitch = static_cast<float>(numeric);

        if (!ExtractNumber(line, "roll", numeric)) {
            return;
        }
        event.roll = static_cast<float>(numeric);

        if (!ExtractNumber(line, "confidence", numeric)) {
            return;
        }
        event.confidence = ClampConfidence(static_cast<float>(numeric));
    }

    callback_(event);
#else
    cJSON* root = cJSON_Parse(line.c_str());
    if (!root) {
        return;
    }

    cJSON* type = cJSON_GetObjectItem(root, "type");
    cJSON* ts = cJSON_GetObjectItem(root, "ts");
    if (!cJSON_IsString(type) || !cJSON_IsNumber(ts)) {
        cJSON_Delete(root);
        return;
    }

    K230VisionEvent event;
    event.type = type->valuestring;
    event.ts = static_cast<int64_t>(ts->valuedouble);

    if (!IsKnownEventType(event.type)) {
        cJSON_Delete(root);
        return;
    }

    cJSON* confidence = cJSON_GetObjectItem(root, "confidence");
    if (cJSON_IsNumber(confidence)) {
        event.confidence = ClampConfidence(static_cast<float>(confidence->valuedouble));
    }

    if (event.type == "face.pose") {
        cJSON* yaw = cJSON_GetObjectItem(root, "yaw");
        cJSON* pitch = cJSON_GetObjectItem(root, "pitch");
        cJSON* roll = cJSON_GetObjectItem(root, "roll");
        if (!cJSON_IsNumber(yaw) ||
            !cJSON_IsNumber(pitch) ||
            !cJSON_IsNumber(roll) ||
            !cJSON_IsNumber(confidence)) {
            cJSON_Delete(root);
            return;
        }
        event.yaw = static_cast<float>(yaw->valuedouble);
        event.pitch = static_cast<float>(pitch->valuedouble);
        event.roll = static_cast<float>(roll->valuedouble);
        event.confidence = ClampConfidence(static_cast<float>(confidence->valuedouble));
    }

    callback_(event);
    cJSON_Delete(root);
#endif
}
