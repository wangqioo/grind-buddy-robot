#include "display/emote_pack_utils.h"

namespace emote {

namespace {

uint16_t ReadLe16(const uint8_t* data)
{
    return static_cast<uint16_t>(data[0]) | static_cast<uint16_t>(data[1] << 8);
}

uint32_t ReadLe32(const uint8_t* data)
{
    return static_cast<uint32_t>(data[0]) |
        (static_cast<uint32_t>(data[1]) << 8) |
        (static_cast<uint32_t>(data[2]) << 16) |
        (static_cast<uint32_t>(data[3]) << 24);
}

} // namespace

std::string EmoteAssetFilenameFromName(const char* name)
{
    if (name == nullptr || name[0] == '\0') {
        return {};
    }

    std::string filename(name);
    if (filename.size() >= 4 && filename.compare(filename.size() - 4, 4, ".eaf") == 0) {
        return filename;
    }
    filename += ".eaf";
    return filename;
}

bool IsEafAssetFilename(const char* name)
{
    if (name == nullptr) {
        return false;
    }

    std::string filename(name);
    return filename.size() >= 4 && filename.compare(filename.size() - 4, 4, ".eaf") == 0;
}

std::string EmoteAssetFilenameForEmotion(const char* emotion)
{
    if (emotion == nullptr || emotion[0] == '\0') {
        return "leisure_05s_.eaf";
    }

    const std::string name(emotion);
    if (name == "neutral" || name == "idle") {
        return "leisure_05s_.eaf";
    }
    if (name == "listening" || name == "curious") {
        return "investigate_.eaf";
    }
    if (name == "thinking" || name == "connecting" || name == "activating") {
        return "confident_08.eaf";
    }
    if (name == "speaking" || name == "happy") {
        return "laugh_05s_10.eaf";
    }
    if (name == "sleepy") {
        return "asleep_215s.eaf";
    }
    if (name == "sad") {
        return "cry_10s_20s.eaf";
    }
    if (name == "angry" || name == "error") {
        return "angry_20s.eaf";
    }
    if (name == "playful") {
        return "music_25s.eaf";
    }

    return "leisure_05s_.eaf";
}

EmoteCanvasGeometry ReadEafCanvasGeometry(const void* data, size_t size)
{
    const auto* bytes = static_cast<const uint8_t*>(data);
    if (bytes == nullptr || size < 32) {
        return {};
    }

    if (bytes[0] != 0x89 || bytes[1] != 'E' || bytes[2] != 'A' || bytes[3] != 'F') {
        return {};
    }

    const uint32_t frame_count = ReadLe32(bytes + 4);
    constexpr uint32_t kMaxSafeFrameCount = (UINT32_MAX - 16) / 8;
    if (frame_count == 0 || frame_count > kMaxSafeFrameCount) {
        return {};
    }

    const size_t frame_table_end = 16 + static_cast<size_t>(frame_count) * 8;
    if (size < frame_table_end) {
        return {};
    }

    const uint32_t first_frame_offset = ReadLe32(bytes + 20);
    if (first_frame_offset > SIZE_MAX - frame_table_end) {
        return {};
    }

    const size_t first_frame = frame_table_end + static_cast<size_t>(first_frame_offset);
    if (size < first_frame + 16) {
        return {};
    }

    const uint16_t width = ReadLe16(bytes + first_frame + 12);
    const uint16_t height = ReadLe16(bytes + first_frame + 14);
    if (width == 0 || height == 0) {
        return {};
    }

    return {width, height};
}

EmotePoint CenteredOffset(int container_width, int container_height, int content_width, int content_height)
{
    const int x = (container_width > content_width) ? (container_width - content_width) / 2 : 0;
    const int y = (container_height > content_height) ? (container_height - content_height) / 2 : 0;
    return {x, y};
}

EmoteRect TopRightOverlayBounds(int container_width, int container_height, int requested_size, int margin)
{
    const int safe_margin = margin > 0 ? margin : 0;
    const int max_width = container_width > safe_margin * 2 ? container_width - safe_margin * 2 : 0;
    const int max_height = container_height > safe_margin * 2 ? container_height - safe_margin * 2 : 0;

    int width = requested_size > 0 ? requested_size : 0;
    int height = requested_size > 0 ? requested_size : 0;
    if (width > max_width) {
        width = max_width;
    }
    if (height > max_height) {
        height = max_height;
    }

    return {container_width - safe_margin - width, safe_margin, width, height};
}

} // namespace emote
