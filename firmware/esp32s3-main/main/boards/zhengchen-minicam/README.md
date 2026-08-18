# Zhengchen Minicam Board Profile (Grind Buddy)

Board profile for the Zhengchen minicam ESP32-S3 dev board, the second main
controller target of Grind Buddy. Integrated from the standalone minicam
firmware tree and ported to this repo's newer xiaozhi-esp32 base (esp_video
camera API, non-virtual `LcdDisplay::SetupUI`).

## How it differs from the szpi-s3 (立创系裸开发板) version

Both boards run the same Grind Buddy interaction logic and share the same
`grind_buddy` module, emote_gen emotion runtime and K230 vision UART protocol.
They differ only in hardware peripherals:

| Item | szpi-s3 (立创系裸开发板) | zhengchen-minicam (this board) |
|---|---|---|
| Display | ST7789 320x240, landscape via swap_xy | ST7789 240x320 portrait native, invert color + RGB order; runtime portrait/landscape toggle |
| LCD chip select | PCA9557 IO expander bit 0 | Dedicated GPIO40 |
| Audio codec | ES8311 (speaker) + ES7210 (mic array) | ES8388 (single codec) |
| PA enable | PCA9557 bit 1 | Not present |
| Camera | None (K230 owns all vision) | On-board DVP camera header, SCCB shares the audio I2C bus (SDA GPIO1 / SCL GPIO2), 20 MHz XCLK |
| Battery / keys | None | Battery + charge detect on ADC (GPIO3/4), ADC volume keys (GPIO9) |
| BOOT button | GPIO0 | GPIO11 |
| K230 vision UART | RX only, GPIO10 @ 921600 | RX GPIO47 / TX GPIO48 @ 921600 (bidirectional reserved) |
| LED | None | None (`NoLed`; `SingleLed` asserts on `GPIO_NUM_NC`) |
| Settings menu | Not present | Double-click BOOT opens an on-screen settings menu (AEC toggle, orientation, re-provision) in non-emote display mode |

## Camera notes

- The board profile uses the esp_video (`Esp32Camera`) API with
  `init_sccb = false`, reusing the audio codec I2C bus handle for SCCB.
- Camera sensor drivers are disabled by default in sdkconfig. When a camera
  module is attached, enable the matching `CONFIG_CAMERA_*` sensor (all DVP
  sensors were verified to probe correctly; GC2145 additionally needs
  `CONFIG_CAMERA_GC2145_AUTO_DETECT_DVP_INTERFACE_SENSOR=y`).
- Without a camera module attached, boot logs show sensor probe failures and
  `open /dev/video2 failed`; this is expected and harmless. The
  `self.camera.capture` MCP tool stays registered but capture will fail.

## Build

```bash
idf.py set-target esp32s3
idf.py menuconfig   # Board Type -> 征辰科技MINICAM(Grind Buddy)
idf.py build flash
```

Recommended options: `CONFIG_USE_EMOTE_MESSAGE_STYLE=y`,
`CONFIG_USE_DEVICE_AEC=y`, 16 MB flash, partition table
`partitions/v2/16m_emote.csv`, and
`CONFIG_GRIND_BUDDY_EMOTE_GEN_PREVIEW=y` with
`CONFIG_GRIND_BUDDY_EMOTE_GEN_ASSETS_FILE` pointing at the repo's
`assets/emote-assets.bin` so the emote_gen partition is flashed too.
