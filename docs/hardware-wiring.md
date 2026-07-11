# Hardware Wiring

## K230 To ESP32-S3 Main Controller

Purpose: visual wake, face pose events, presence/gaze state.

```text
K230 GPIO11 UART2_TXD -> ESP32-S3 main RX GPIO10
K230 GPIO12 UART2_RXD <- reserved for future main-controller command channel
GND                   -> GND
Baud                  -> 921600
Protocol              -> binary frames, magic A5 5A, CRC-16/CCITT
```

## K230 To ESP32 FOC Controller

Purpose: body/gimbal face tracking.

Recommended wiring is one-way from K230 to the FOC controller:

```text
K230 GPIO3 UART1_TXD -> FOC ESP32 IO16 RX
GND                  -> GND
Baud                 -> 115200
Protocol             -> text commands
```

Do not connect `FOC ESP32 IO17 TX -> K230 GPIO4 UART1_RXD` in the current
bench setup. The FOC link is command-only, and K230 does not need to receive
data from the FOC controller.

If the FOC controller is powered before K230 while its TX line is connected to
K230 RX, the ESP32 TX idle level can back-power or disturb the K230 IO domain
during boot. The observed symptom is K230 failing camera startup with:

```text
RuntimeError: sensor(0) runerror, vicap init failed(-1)
```

Safe startup options:

- Preferred: leave FOC TX disconnected and use only `K230 TX -> FOC RX` plus common ground.
- If bidirectional FOC telemetry is later required, add proper isolation or buffering before reconnecting FOC TX to K230 RX.
- When debugging with the old bidirectional wiring, power K230 first, wait until the camera pipeline initializes, then power FOC.

Commands sent by K230 to FOC:

```text
F center_x,center_y
H
```

K230 only sends `F x,y` when the primary face is frontal. When the face turns away or disappears, K230 sends `H` once to return the FOC controller to home/default state.

## FOC Controller Serial Ports

```text
USB Serial -> manual debug commands
Serial2 RX IO16 -> K230 tracking commands
Serial2 TX IO17 -> optional debug/telemetry only; leave disconnected from K230 by default
```

The current Grind Buddy FOC board does not use the BMI160 IMU. The firmware
keeps `ENABLE_IMU_CONTROL=0` and runs body tracking from AS5600 encoder
feedback plus serial targets. If BMI160-dependent startup is re-enabled while
the IMU is unplugged, the FOC firmware will continuously report I2C errors and
will not respond reliably to tracking commands.

Supported FOC commands:

```text
F x,y       face-center tracking offset
T m0,m1    absolute target degrees relative to power-on home
M0 angle   set vertical axis only
M1 angle   set horizontal axis only
H          return both axes to home
S          print status
?          print help
```

## Current Device IDs Seen On Bench

```text
ESP32-S3 main controller MAC: 10:51:db:80:e2:e8
FOC ESP32 MAC:                1c:c3:ab:27:04:10
```

Always identify the chip before flashing when multiple boards are connected.
