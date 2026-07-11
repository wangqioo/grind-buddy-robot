#include "display/emote_pack_utils.h"

#include <cassert>
#include <cstdint>
#include <string>

int main() {
    assert(emote::EmoteAssetFilenameFromName("asleep_215s") == "asleep_215s.eaf");
    assert(emote::EmoteAssetFilenameFromName("investigate_") == "investigate_.eaf");
    assert(emote::EmoteAssetFilenameFromName("mock_05s.eaf") == "mock_05s.eaf");
    assert(emote::EmoteAssetFilenameFromName(nullptr).empty());
    assert(emote::EmoteAssetFilenameFromName("").empty());
    assert(emote::IsEafAssetFilename("asleep_215s.eaf"));
    assert(!emote::IsEafAssetFilename("index.json"));
    assert(!emote::IsEafAssetFilename(nullptr));
    assert(emote::EmoteAssetFilenameForEmotion("neutral") == "leisure_05s_.eaf");
    assert(emote::EmoteAssetFilenameForEmotion("idle") == "leisure_05s_.eaf");
    assert(emote::EmoteAssetFilenameForEmotion("listening") == "investigate_.eaf");
    assert(emote::EmoteAssetFilenameForEmotion("thinking") == "confident_08.eaf");
    assert(emote::EmoteAssetFilenameForEmotion("speaking") == "laugh_05s_10.eaf");
    assert(emote::EmoteAssetFilenameForEmotion("sad") == "cry_10s_20s.eaf");
    assert(emote::EmoteAssetFilenameForEmotion("angry") == "angry_20s.eaf");
    assert(emote::EmoteAssetFilenameForEmotion("unknown") == "leisure_05s_.eaf");

    const uint8_t eaf_with_240_square[] = {
        0x89, 'E',  'A',  'F',
        0x01, 0x00, 0x00, 0x00, // frame count
        0x00, 0x00, 0x00, 0x00,
        0x18, 0x00, 0x00, 0x00, // payload length
        0x18, 0x00, 0x00, 0x00, // frame 0 length
        0x00, 0x00, 0x00, 0x00, // frame 0 payload offset
        0x5a, 0x5a, '_',  'S',
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x01, 0x08,
        0xf0, 0x00, 0xf0, 0x00, // width=240 height=240
    };

    const auto geometry = emote::ReadEafCanvasGeometry(eaf_with_240_square, sizeof(eaf_with_240_square));
    assert(geometry.width == 240);
    assert(geometry.height == 240);

    const auto offset = emote::CenteredOffset(320, 240, geometry.width, geometry.height);
    assert(offset.x == 40);
    assert(offset.y == 0);

    const auto badge = emote::TopRightOverlayBounds(320, 240, 52, 8);
    assert(badge.x == 260);
    assert(badge.y == 8);
    assert(badge.width == 52);
    assert(badge.height == 52);

    const auto clamped = emote::TopRightOverlayBounds(40, 30, 52, 8);
    assert(clamped.x == 8);
    assert(clamped.y == 8);
    assert(clamped.width == 24);
    assert(clamped.height == 14);
    return 0;
}
