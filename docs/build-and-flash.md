# Build And Flash

## K230 Vision

```bash
cd firmware/k230-vision
python3 tools/build_k230_single.py
python3 -m unittest discover -s tests -p 'test_*.py' -v
```

Copy this file to the K230 board:

```text
firmware/k230-vision/dist/main_vision_uart_single.py
```

Run in CanMV:

```python
exec(open("/sdcard/pet/main_vision_uart_single.py").read())
```

## ESP32-S3 Main Controller

Two board targets are supported. Select the board in `idf.py menuconfig` under
`Xiaozhi Assistant -> Board Type`:

- `SZPI ESP32-S3` (`szpi-s3`): bare ESP32-S3 board, ST7789 320x240 landscape
  SPI display, ES8311 + ES7210 audio.
- `征辰科技MINICAM(Grind Buddy)` (`zhengchen-minicam`): Zhengchen minicam
  dev board, ST7789 240x320 portrait SPI display, ES8388 audio, on-board DVP
  camera, battery and ADC volume keys.

```bash
cd firmware/esp32s3-main
. /Users/wq/esp-idf/export.sh
idf.py -B build-szpi-s3-local build
```

For the minicam board use a separate build directory, e.g.
`idf.py -B build-zhengchen-minicam-local build`, with
`CONFIG_USE_EMOTE_MESSAGE_STYLE=y`, `CONFIG_USE_DEVICE_AEC=y`, 16 MB flash and
the `partitions/v2/16m_emote.csv` partition table.

Before flashing, identify the chip. The main controller bench MAC is:

```text
10:51:db:80:e2:e8
```

## ESP32 FOC Controller

```bash
cd firmware/esp32-foc
pio run
pio run -t upload --upload-port /dev/cu.wchusbserial1110
```

Before flashing, identify the chip. The FOC controller bench MAC is:

```text
1c:c3:ab:27:04:10
```
