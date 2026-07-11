# ESP32 Emote Gen Integration Guide

Updated: 2026-07-10

This document records the current bench-proven path for driving exported
Espressif Emote Gen assets on the ESP32-S3 SZPI display. Use it as the quick
reference when continuing expression work.

## Current Status

The ESP32-S3 firmware can now:

- Flash a packer-exported `emote-assets.bin` into a dedicated `emote_gen`
  partition.
- Mount that partition at boot with `esp_mmap_assets`.
- Find all `.eaf` animations in the pack.
- Decode each EAF canvas size from its first frame.
- Center square EAF clips on the `320x240` SZPI screen.
- Select EAF clips from semantic emotion names and device status changes.
- Keep status, notification, and chat text as an overlay above the expression.

The current bench asset pack is:

```text
/Users/wq/grind-buddy-robot/assets/emote-assets.bin
```

The pack contains 11 animations:

```text
music_25s
sigh_20s_40s
angry_20s
asleep_215s
badminton_12
confident_08
cry_10s_20s
investigate_
laugh_05s_10
leisure_05s_
mock_05s
```

`asleep_215s.eaf` is a useful reference case: the exported pack targets the
device, but the EAF clip itself declares a `240x240` canvas. On a `320x240`
screen it must be positioned at `x=40,y=0` to appear centered.

## Key Files

Firmware root:

```text
firmware/esp32s3-main/
```

Important files:

| File | Purpose |
|---|---|
| `main/display/emote_display.cc` | Runtime display implementation, mmap mount, emotion mapping, EAF playback, text overlay. |
| `main/display/emote_display.h` | `EmoteDisplay` state for `emote_gen` assets and emotion runtime. |
| `main/display/emote_pack_utils.*` | Small host-testable helpers for EAF names, EAF geometry, and centering. |
| `main/Kconfig.projbuild` | `GRIND_BUDDY_EMOTE_GEN_*` options and SZPI emote display enablement. |
| `main/CMakeLists.txt` | Flashes the configured `emote_gen` binary into the `emote_gen` partition. |
| `partitions/v2/16m_emote.csv` | 16 MB flash layout with a 5 MB default `assets` partition and 3 MB `emote_gen` partition. |
| `tests/cpp/test_emote_pack_utils.cpp` | Host test for EAF filtering, canvas parsing, and centering. |

Local bench config is in ignored files:

```text
firmware/esp32s3-main/sdkconfig.defaults.local
firmware/esp32s3-main/sdkconfig
```

Do not commit secrets or local `sdkconfig`.

## Required Config

For the SZPI-S3 bench build, the local config currently enables:

```text
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions/v2/16m_emote.csv"
CONFIG_USE_EMOTE_MESSAGE_STYLE=y
CONFIG_GRIND_BUDDY_EMOTE_GEN_PREVIEW=y
CONFIG_GRIND_BUDDY_EMOTE_GEN_ASSETS_FILE="../../../assets/emote-assets.bin"
```

`CONFIG_GRIND_BUDDY_EMOTE_GEN_PREVIEW_NAME` still exists as a fallback for a
single asset, but the current runtime scans all `.eaf` files in the pack and
selects them by emotion name.

## Build And Flash

From:

```bash
cd /Users/wq/grind-buddy-robot/firmware/esp32s3-main
```

Run the host helper test:

```bash
c++ -std=c++20 -Wall -Wextra -Imain \
  tests/cpp/test_emote_pack_utils.cpp \
  main/display/emote_pack_utils.cc \
  -o /tmp/test_emote_pack_utils && /tmp/test_emote_pack_utils
```

Build firmware:

```bash
. /Users/wq/esp-idf/export.sh >/tmp/idf-export.log
idf.py -B build-szpi-s3-local build
```

Flash the connected board:

```bash
idf.py -B build-szpi-s3-local -p /dev/cu.usbmodem21301 flash
```

Monitor:

```bash
idf.py -B build-szpi-s3-local -p /dev/cu.usbmodem21301 monitor
```

Expected boot log markers:

```text
MMAP Assets [emote_gen] mounted successfully
emote_gen emotion runtime loaded 11 animations
Playing emote_gen emotion: leisure_05s_.eaf
```

## How The Playback Works

At boot, `EmoteDisplay::TryStartEmoteGenPreview()`:

1. Mounts partition `emote_gen` with `mmap_assets_new`.
2. Iterates `mmap_assets_get_stored_files`.
3. Keeps only filenames ending in `.eaf` in a filename-to-index map.
4. Starts the default neutral expression.

Runtime expression changes happen through:

- `SetEmotion("neutral")`
- `SetEmotion("listening")`
- `SetEmotion("thinking")`
- `SetEmotion("speaking")`
- `SetStatus(...)`, which also maps core device status to an emotion.

Each animation is played by `PlayStandaloneEmote()`:

1. Reads the EAF canvas size using `ReadEafCanvasGeometry`.
2. Clamps the animation size to the physical display size.
3. Computes centered top-left offset with `CenteredOffset`.
4. Calls `gfx_anim_set_src`, `gfx_anim_set_segment`, and `gfx_anim_start`.

This keeps square clips centered without stretching them.

## Resource Format Notes

`emote-assets.bin` is an `esp_mmap_assets` binary:

- Header magic: `MMAP`
- Contains `index.json` plus EAF payloads.
- The mmap table reports width and height as `0` for `.eaf` files because EAF
  is not a plain image.
- EAF dimensions must be read from the first frame payload.

The current exported `index.json` contains `x:0,y:0` for every clip. If those
coordinates are honored directly, square clips appear left-aligned. The current
runtime deliberately ignores those coordinates and centers by EAF geometry.

## Current Runtime Limitations

The current code is the first emotion runtime, with temporary mapping choices:

- It does not yet honor `index.json` loop ranges.
- It does not yet parse backend-provided emotion signals.
- The status-to-emotion mapping is intentionally provisional.
- Caption positioning is functional, not final UI design.

Current default mapping:

| Emotion / State | Candidate EAF |
|---|---|
| `neutral`, `idle`, standby | `leisure_05s_` |
| `listening`, `curious` | `investigate_` |
| `thinking`, connecting, activating | `confident_08` |
| `speaking`, `happy` | `laugh_05s_10` |
| `sleepy` | `asleep_215s` |
| `sad` | `cry_10s_20s` |
| `angry`, error | `angry_20s` |
| `playful` | `music_25s` |

## Next Step: Emotion State Machine

The next implementation should move the temporary string mapping into a
first-class emotion state machine:

```text
Application / Grind Buddy behavior state
  -> Emotion state
  -> EmoteDisplay expression selection
  -> EAF animation + status/chat overlay
```

The app state machine should not know EAF filenames. It should emit semantic
emotion states, and `EmoteDisplay` should own the mapping from emotion to asset.

## Text And Status Overlay

Text can be layered over expressions. The existing `EmoteDisplay` already has:

- `g_obj_label_toast`
- `g_obj_label_clock`
- `g_obj_img_status`
- `SetStatus`
- `SetChatMessage`
- `ShowNotification`

The preview-mode early returns have been removed. The current runtime allows
these overlay calls to continue while EAF expressions play:

- `SetStatus`
- `SetChatMessage`
- `ShowNotification`

The final UI still needs a deliberate overlay policy:

| Overlay | Suggested placement | Notes |
|---|---|---|
| Status | Top or bottom strip | Short strings only: connecting, listening, thinking, speaking. |
| User transcript | Bottom caption | 1-2 lines, semi-transparent background if readability needs it. |
| Assistant transcript | Bottom caption or speech-bubble strip | Avoid covering the main eye/mouth area. |
| Notifications | Temporary top toast | Auto-hide after a timeout. |

Practical rule: the expression should remain the primary surface. Text should
be compact, transient, and placed where it does not hide the face.

## Troubleshooting

If the screen shows only one or two visible states:

- Confirm the current state is actually changing.
- Check monitor logs for `SetEmotion: ...` and `Playing emote_gen emotion: ...`.
- Remember that one EAF can internally contain many frames but only a few
  visually distinct poses.

If a square expression appears left-aligned:

- Inspect the EAF canvas size.
- A `240x240` clip on a `320x240` display should be centered at `x=40,y=0`.
- Do not blindly trust `index.json` `x/y` when exported as `0,0`.

If `emote_gen` does not mount:

- Confirm `CONFIG_PARTITION_TABLE_CUSTOM_FILENAME` points to
  `partitions/v2/16m_emote.csv`.
- Confirm the flash command includes the `0xd00000 .../emote-assets.bin` write.
- Confirm the asset binary fits inside the 3 MB `emote_gen` partition.

If build works but flash omits the emote pack:

- Check `CONFIG_GRIND_BUDDY_EMOTE_GEN_PREVIEW=y`.
- Check `CONFIG_GRIND_BUDDY_EMOTE_GEN_ASSETS_FILE`.
- Check `main/CMakeLists.txt` still calls `esptool_py_flash_to_partition` for
  partition `emote_gen`.
