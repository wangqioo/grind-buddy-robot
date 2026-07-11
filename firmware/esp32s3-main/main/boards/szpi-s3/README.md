# SZPI S3 Xiaozhi Board Profile Draft

This directory is a migration package for the real ESP32-S3 board used by Grind
Buddy. It is intended to be copied into `xiaozhi-esp32/main/boards/szpi-s3`
after the profile is reviewed.

Source evidence:

- `/Users/wq/Desktop/szpi-s3-esp/12-speech_recognition`
- Xiaozhi local reference: `/Users/wq/Workshop/MCU/xiaozhi-project/xiaozhi-esp32/main/boards/lichuang-dev`

The profile keeps the SZPI example pin map:

- ST7789 SPI LCD: MOSI GPIO40, CLK GPIO41, DC GPIO39, backlight GPIO42.
- LCD CS: PCA9557 bit 0.
- FT5x06 touch over I2C.
- I2C: SDA GPIO1, SCL GPIO2.
- ES8311 speaker and ES7210 microphone.
- I2S: MCLK GPIO38, BCLK GPIO14, WS GPIO13, DIN GPIO12, DOUT GPIO45.
- Speaker PA enable: PCA9557 bit 1.
- BOOT button: GPIO0.

The SZPI speech-recognition example records at 16 kHz, but Xiaozhi's ES8311
codec path asserts that input and output sample rates are equal. This profile
therefore uses 24 kHz input and 24 kHz output for Xiaozhi integration while
keeping the SZPI physical pin map unchanged.

V1 deliberately does not initialize the ESP32-S3 camera path. K230 owns vision
and sends JSON Lines vision events to ESP32-S3 over UART.

To migrate into Xiaozhi:

1. Copy this directory to `xiaozhi-esp32/main/boards/szpi-s3`.
2. Add `CONFIG_BOARD_TYPE_SZPI_S3` to `main/Kconfig.projbuild`.
3. Add a `CONFIG_BOARD_TYPE_SZPI_S3` branch in `main/CMakeLists.txt` with
   `set(BOARD_TYPE "szpi-s3")`.
4. Include `CONFIG_BOARD_TYPE_SZPI_S3` in the `USE_DEVICE_AEC` dependency list.
5. Build with the `szpi-s3` config and flash the board before adding K230 UART.
