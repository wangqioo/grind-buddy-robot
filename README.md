# Grind Buddy Robot 上班搭子机器人

Grind Buddy Robot 是一个面向黑客松演示的注视感知桌面陪伴机器人。它不是传统意义上的语音助手，也不是简单把大模型装进音箱里。我们的核心想法是：一个小机器人如果能理解用户有没有在看它，并且优先用表情、动作和小声音回应，而不是一直说话，它会更像一个真正有存在感的“上班搭子”。

当前版本已经跑通四个核心运行部分：

- K230 视觉协处理器：本地完成摄像头、人脸、注视方向和追踪信号处理。
- ESP32-S3 主控：负责屏幕表情、麦克风、扬声器、Wi-Fi 和自研后端语音链路。
- ESP32 FOC 控制板：负责无刷电机云台/身体动作，根据人脸偏移做追踪。
- Mac 本地后台：负责 ASR、LLM/VLM、TTS、OTA 和 WebSocket 服务。

## 一句话 Demo

当用户转头看向机器人时，K230 在本地判断正脸注视，ESP32-S3 唤醒交互并切换屏幕表情，FOC 控制板驱动身体跟随用户脸部方向，本地后台可以用短语音或小声音进行回应；当用户转开视线时，机器人停止追踪和听取，回到安静的默认状态。

## 项目亮点

这个项目想探索的不是“又一个会聊天的设备”，而是一个更像有情绪、有身体反应的桌面伙伴。

1. **注视触发交互**
   机器人不是一直主动打扰用户，而是通过 K230 本地视觉判断用户是否看向它。只有用户看它时，它才进入活跃状态；用户看走或人脸消失时，它会停止追踪、停止听取，并回到默认状态。

2. **用情绪做状态机**
   设备状态不只用文字或语音表达，而是映射成表情动画、屏幕状态文字、小声音和身体动作。比如监听、思考、开心、困倦、委屈、错误等状态，都可以对应不同表情和动作。

3. **端云协同，大小模型分工**
   K230 在端侧做快速、低延迟的视觉判断；本地后台和云端大模型负责语言理解、视觉模型理解和更复杂的场景推理。这种架构既保留实时反应，又能接入更强的 AI 能力。

4. **表情和动作耦合**
   屏幕表情不是孤立播放，FOC 身体动作也不是单独测试。项目目标是让不同情绪同时驱动屏幕表情和身体动作，让机器人拥有比语音更丰富的表达通道。

5. **三板解耦，方便迭代**
   K230、ESP32-S3 主控、ESP32 FOC 控制板和本地后台可以独立开发、独立烧录和独立调试。视觉、交互、运动和智能后台职责清晰。

## 当前已验证状态

当前三套固件和本地后台已经完成实机联调：

- K230 摄像头可以识别人脸并输出视觉数据。
- ESP32-S3 可以接收 K230 视觉事件，并在稳定正脸注视后触发视觉唤醒。
- ESP32-S3 屏幕可以播放导出的 `emote-assets.bin` 表情动画包。
- 表情动画可以在 320x240 屏幕居中显示，并叠加状态/对话文字。
- ESP32-S3 可以通过 OTA 和 WebSocket 连接我们自己的本地 AI 后台。
- 本地后台的 ASR、LLM、TTS 链路已分别验证可用。
- K230 可以向 ESP32 FOC 控制板发送人脸中心偏移量。
- ESP32 FOC 控制板可以响应 `F x,y` 追踪命令，并通过 `H` 回到默认位置。
- 当前 FOC 固件已经去掉对 BMI160 IMU 的依赖，适配已拔掉 IMU 的硬件状态。

## 系统架构

```text
                    端侧视觉感知
                +----------------------+
                | K230 CanMV 摄像头    |
                | 人脸 / 注视 / 姿态   |
                +----------+-----------+
                           |
              +------------+-------------+
              |                          |
  二进制 UART |                          | 文本 UART
  921600 baud |                          | 115200 baud
              v                          v
+----------------------------+     +--------------------------+
| ESP32-S3 主控              |     | ESP32 FOC 控制板         |
| 屏幕 / 麦克风 / 扬声器     |     | 云台 / 身体动作          |
| Wi-Fi / 表情状态机         |     | 人脸中心追踪             |
+-------------+--------------+     +--------------------------+
              |
              | OTA + WebSocket
              v
+----------------------------+
| Mac 本地后台               |
| ASR / LLM / VLM / TTS      |
| 自研本地 AI 运行时         |
+----------------------------+
```

职责划分：

| 层级 | 负责模块 | 职责 |
|---|---|---|
| 感知层 | K230 | 本地人脸检测、正脸注视判断、追踪偏移输出。 |
| 交互层 | ESP32-S3 | 设备状态、屏幕表情、音频、Wi-Fi、后端会话控制。 |
| 运动层 | ESP32 FOC | 本地电机闭环、云台/身体追踪、回中动作。 |
| 智能层 | 本地后台 | ASR、LLM/VLM、TTS、OTA、WebSocket 协议。 |

## 交互流程

```text
默认 / 待机
  -> 用户看向机器人
  -> K230 判断稳定正脸
  -> ESP32-S3 进入倾听 / 关注状态
  -> 屏幕切换表情并叠加状态文字
  -> K230 向 FOC 发送脸部偏移量
  -> FOC 驱动身体朝向用户
  -> 后台可进行短语音或小声音回应
  -> 用户看走 / 人脸消失
  -> ESP32-S3 关闭听取权限
  -> K230 向 FOC 发送 H
  -> 机器人回到安静默认状态
```

这个流程让机器人拥有一个非常直观的社交规则：你看它，它回应；你不看它，它安静。

## 仓库结构

```text
assets/
  emote-assets.bin
    Espressif Emote Gen 导出的表情动画包，用于 ESP32-S3 屏幕。

backend/
  本地 AI 后台，包括服务端源码、运行脚本、配置模板和检查工具。

firmware/k230-vision/
  K230 CanMV MicroPython 视觉程序、UART 协议、测试和单文件部署脚本。

firmware/esp32s3-main/
  ESP-IDF 主控固件，加入了上班搭子的屏幕表情、视觉唤醒、音频、
  Wi-Fi 和本地后台接入。

firmware/esp32-foc/
  PlatformIO ESP32 FOC 固件，用于云台/身体动作控制，接收 K230 的
  `F x,y` 和 `H` 指令。

docs/
  硬件接线、构建烧录、架构说明、表情驱动指南、开发状态和测试矩阵。

tools/
  构建和烧录辅助脚本。
```

## 核心技术模块

### 1. K230 本地视觉门控

K230 在端侧直接运行摄像头和人脸处理逻辑，判断用户是否正脸看向机器人。这样不需要把每一帧图像发到云端，也不需要等待大模型判断唤醒条件，交互启动会更快。

K230 当前输出两路数据：

- 给 ESP32-S3 主控：二进制人脸、存在、注视状态帧。
- 给 ESP32 FOC 控制板：轻量级追踪指令。

FOC 指令示例：

```text
F center_x,center_y
H
```

只有当 K230 判断主脸是正向时，才会发送 `F x,y`。当用户转头或人脸消失时，K230 会发送一次 `H`，让 FOC 控制板回到默认位置。

### 2. ESP32-S3 表情显示

ESP32-S3 屏幕运行时使用导出的 Emote Gen 表情包：

```text
assets/emote-assets.bin
```

当前表情包包含 11 个动画：

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

固件会挂载 `emote_gen` 分区，扫描所有 `.eaf` 文件，从首帧读取 EAF 画布尺寸，把方形动画居中显示在 320x240 屏幕上，并把状态文字/对话文字叠加在表情上方。

当前临时状态映射：

| 状态 | 表情候选 |
|---|---|
| 默认 / 中性 | `leisure_05s_` |
| 倾听 / 好奇 | `investigate_` |
| 思考 / 连接中 | `confident_08` |
| 说话 / 开心 | `laugh_05s_10` |
| 困倦 | `asleep_215s` |
| 难过 | `cry_10s_20s` |
| 生气 / 错误 | `angry_20s` |
| 调皮 | `music_25s` |

更多实现细节见 [ESP32 Emote Gen 集成指南](docs/esp32-emote-gen-guide.md)。

### 3. FOC 身体追踪

FOC 控制板对 K230 暴露的是一个非常简单的高层接口：K230 只负责告诉它“脸偏离中心多少”，FOC 控制板自己做电机闭环和动作平滑。

支持的串口命令：

```text
F x,y       人脸中心追踪偏移
T m0,m1    相对上电 home 点的绝对角度目标
M0 angle   只设置竖直轴
M1 angle   只设置水平轴
H          两个轴回到 home
S          打印状态
?          打印帮助
```

当前固件不再要求 BMI160 IMU 存在，使用 AS5600 编码器反馈和串口目标值即可运行，符合当前实机硬件状态。

### 4. 我们自己的本地 AI 后台

本地后台负责 AI 运行时能力，避免黑客松演示完全依赖远端业务服务器。设备通过 OTA 和 WebSocket 接入这个后端，后端统一负责语音识别、大模型回复、视觉模型能力和语音合成。

当前模板配置：

| 层级 | Provider |
|---|---|
| VAD | SileroVAD |
| ASR | FunASR / SenseVoiceSmall |
| LLM | ChatGLMLLM / glm-4-flash |
| VLLM | ChatGLMVLLM / glm-4v-flash |
| TTS | EdgeTTS |

运行端口：

```text
8001  设备 WebSocket
8003  OTA / HTTP 接口
```

常用检查命令：

```bash
cd backend/*local
./tools/doctor.sh
```

`doctor.sh` 会检查配置渲染、模型路径、本地环境、端口和核心工具链。

## 硬件接线

完整接线说明见 [硬件接线文档](docs/hardware-wiring.md)。

### K230 到 ESP32-S3 主控

```text
K230 GPIO11 UART2_TXD -> ESP32-S3 主控 RX GPIO10
K230 GPIO12 UART2_RXD <- 预留给未来主控命令通道
GND                   -> GND
Baud                  -> 921600
Protocol              -> binary frames, magic A5 5A, CRC-16/CCITT
```

### K230 到 ESP32 FOC 控制板

推荐使用单向命令接线：

```text
K230 GPIO3 UART1_TXD -> FOC ESP32 IO16 RX
GND                  -> GND
Baud                 -> 115200
Protocol             -> text commands
```

当前演示里 FOC 链路是命令单向链路，默认不要连接：

```text
FOC ESP32 IO17 TX -> K230 GPIO4 UART1_RXD
```

如果 FOC 先上电并且这条 TX 回传线接到了 K230 RX，可能会干扰 K230 启动时的 IO 状态，导致摄像头初始化失败。

已观察到的错误：

```text
RuntimeError: sensor(0) runerror, vicap init failed(-1)
```

推荐启动方式：

1. FOC TX 不接 K230 RX。
2. K230 和 FOC 共地。
3. 先给 K230 上电并等待摄像头初始化完成。
4. 再给 FOC 控制板上电。

## 构建和运行

### K230 视觉程序

```bash
cd firmware/k230-vision
python3 tools/build_k230_single.py
python3 -m unittest discover -s tests -p 'test_*.py' -v
```

部署生成文件到 K230：

```text
firmware/k230-vision/dist/main_vision_uart_single.py
```

在 CanMV 中运行：

```python
exec(open("/sdcard/pet/main_vision_uart_single.py").read())
```

### ESP32-S3 主控

```bash
cd firmware/esp32s3-main
. /Users/wq/esp-idf/export.sh
idf.py -B build-szpi-s3-local build
idf.py -B build-szpi-s3-local -p /dev/cu.usbmodemXXXX flash
```

烧录前请先确认芯片，当前实机 ESP32-S3 主控 MAC：

```text
10:51:db:80:e2:e8
```

预期启动日志关键字：

```text
Grind Buddy UART integration started
MMAP Assets [emote_gen] mounted successfully
emote_gen emotion runtime loaded 11 animations
```

### ESP32 FOC 控制板

```bash
cd firmware/esp32-foc
pio run
pio run -t upload --upload-port /dev/cu.wchusbserialXXXX
```

烧录前请先确认芯片，当前实机 FOC ESP32 MAC：

```text
1c:c3:ab:27:04:10
```

### 本地后台

```bash
cd backend/*local
./tools/doctor.sh
./tools/run_server.sh
```

后台依赖本地忽略的配置和模型文件。公开仓库里包含模板和脚本，但真实密钥、生成配置和模型权重不会提交。

## 给评委的 Demo 检查清单

1. 启动本地后台，确认 `8001` 和 `8003` 正在监听。
2. 给 K230 上电，确认摄像头初始化成功。
3. 给 ESP32-S3 主控上电，等待 Wi-Fi 和 WebSocket 连接。
4. 给 FOC 控制板上电。
5. 看向机器人，机器人应进入关注/倾听表情状态。
6. 保持看向机器人并移动脸部，FOC 身体应跟随脸部中心偏移。
7. 看向别处，机器人应关闭追踪和听取权限，并回到默认状态。
8. 提一个简短问题，后台应完成 ASR -> LLM -> TTS，ESP32-S3 播放回复。

## 当前已经闭环的部分

已经闭环：

- K230 本地注视门控。
- K230 到 ESP32-S3 的视觉唤醒 UART。
- K230 到 FOC 的追踪 UART。
- ESP32-S3 从 Emote Gen 动画包播放表情。
- ESP32-S3 在表情上叠加状态/对话文字。
- ESP32-S3 到本地后台的 OTA/WebSocket。
- 本地后台 ASR、LLM、TTS。
- 不依赖 BMI160 IMU 的 FOC 追踪。

仍在实验：

- 最终情绪到表情的映射。
- 每种情绪对应的身体动作库。
- 云端 VLM 场景理解策略。
- 长期记忆和主动陪伴行为。
- 完整离线化 K230 部署包。

## 安全和大文件说明

本仓库有意不提交：

- 真实后台密钥。
- 生成后的 `.config.yaml`。
- `.local.env`。
- SenseVoiceSmall、Silero VAD 等模型权重。
- 运行日志、缓存和构建产物。

需要复现本地开发环境时，请使用 `backend/` 下的本地后台示例配置和初始化脚本。

## 相关文档

- [架构说明](docs/architecture.md)
- [硬件接线](docs/hardware-wiring.md)
- [构建和烧录](docs/build-and-flash.md)
- [ESP32 Emote Gen 集成指南](docs/esp32-emote-gen-guide.md)
- [开发状态](docs/development-status.md)
- [测试矩阵](docs/test-matrix.md)

## GitHub

```text
https://github.com/wangqioo/grind-buddy-robot
```
