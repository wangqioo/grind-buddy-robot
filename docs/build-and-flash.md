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

```bash
cd firmware/esp32s3-main
. /Users/wq/esp-idf/export.sh
idf.py -B build-szpi-s3-local build
```

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
