#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace emote {

struct EmoteCanvasGeometry {
    uint16_t width = 0;
    uint16_t height = 0;
};

struct EmotePoint {
    int x = 0;
    int y = 0;
};

struct EmoteRect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

std::string EmoteAssetFilenameFromName(const char* name);
bool IsEafAssetFilename(const char* name);
std::string EmoteAssetFilenameForEmotion(const char* emotion);
EmoteCanvasGeometry ReadEafCanvasGeometry(const void* data, size_t size);
EmotePoint CenteredOffset(int container_width, int container_height, int content_width, int content_height);
EmoteRect TopRightOverlayBounds(int container_width, int container_height, int requested_size, int margin);

} // namespace emote
