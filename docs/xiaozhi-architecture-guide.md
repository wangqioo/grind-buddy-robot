# 小智项目架构开发指南

本文档总结小智项目的代码架构设计原则，用于指导后续开发，保持代码结构清晰。

## 1. 整体架构

```
xiaozhi-esp32/main/
├── application.cc/h          # 核心应用入口（硬件无关）
├── audio/                    # 音频处理模块
├── display/                  # 显示系统模块
├── protocols/                # 网络协议模块
├── led/                      # LED控制模块
├── iot/                      # IoT设备控制
├── boards/
│   ├── common/               # BSP共享代码（40+文件）
│   └── <板子名>/             # 各板子特定实现（109个）
├── Kconfig.projbuild         # 配置菜单定义
└── CMakeLists.txt            # 条件编译规则
```

## 2. 分层架构原则

```
┌─────────────────────────────────────────────────┐
│  应用层 (Application)                            │
│  - 业务逻辑、状态机、事件循环                      │
│  - 绝对不引用具体硬件代码                          │
├─────────────────────────────────────────────────┤
│  服务层 (Services)                               │
│  - AudioService, Protocol, Display               │
│  - 通过抽象接口调用底层                            │
├─────────────────────────────────────────────────┤
│  抽象层 (Abstract Interfaces)                    │
│  - Board*, AudioCodec*, Display* 基类            │
│  - 定义虚拟方法，不关心实现                        │
├─────────────────────────────────────────────────┤
│  BSP层 (Board Support Package)                  │
│  - boards/common/ 共享实现                       │
│  - boards/<板子>/ 具体板子实现                    │
└─────────────────────────────────────────────────┘
```

**核心原则**：上层永远不知道下层的具体实现，只通过接口通信。

## 3. 关键文件职责

| 文件/目录 | 职责 | 注意事项 |
|-----------|------|----------|
| `application.cc` | 事件循环、状态管理、模块调度 | 只做调度，不做具体实现 |
| `audio/` | 音频采集、播放、编解码 | 通过AudioCodec接口访问硬件 |
| `display/` | UI渲染、LVGL框架 | 通过Display接口访问硬件 |
| `protocols/` | WebSocket/MQTT通信 | 可插拔，通过配置选择 |
| `boards/common/board.h` | Board基类定义 | 所有板子的抽象接口 |
| `boards/common/*.cc` | 共享实现 | WiFi、按键、背光等通用功能 |
| `boards/<板子>/config.h` | 硬件参数定义 | 只放GPIO、地址等宏定义 |
| `boards/<板子>/board.cc` | 板子初始化实现 | 继承Board类，实现虚拟方法 |

## 4. 硬件抽象模式

### 4.1 虚拟基类定义接口

```cpp
// boards/common/board.h
class Board {
public:
    virtual AudioCodec* GetAudioCodec() = 0;  // 纯虚函数，必须实现
    virtual Display* GetDisplay();             // 可选实现，默认返回nullptr
    virtual void StartNetwork() = 0;
    virtual std::string GetBoardType() = 0;
    // ...
};
```

### 4.2 工厂模式创建实例

```cpp
// 每个板子用宏导出工厂
DECLARE_BOARD(EspBox3Board)

// 运行时统一获取
Board& board = Board::GetInstance();
board.GetAudioCodec()->Play(data);  // 不关心具体是哪个板子
```

### 4.3 配置与代码分离

```
boards/esp-box-3/
├── config.h       # 只有宏定义（GPIO、I2C地址等参数）
└── board.cc       # 只有初始化逻辑，引用config.h的宏
```

**好处**：换引脚只改 config.h，逻辑代码不变。

## 5. 编译时选择机制

### 5.1 Kconfig菜单配置

在 `Kconfig.projbuild` 中定义板子选项：

```
choice BOARD_TYPE
    config BOARD_TYPE_ESP_BOX_3
        bool "Espressif ESP-BOX-3"
        depends on IDF_TARGET_ESP32S3
    config BOARD_TYPE_KEVIN_C3
        bool "Kevin C3"
        depends on IDF_TARGET_ESP32C3
endchoice
```

### 5.2 CMakeLists.txt条件编译

```cmake
if(CONFIG_BOARD_TYPE_ESP_BOX_3)
    set(BOARD_TYPE "esp-box-3")
elseif(CONFIG_BOARD_TYPE_KEVIN_C3)
    set(BOARD_TYPE "kevin-c3")
endif()

# 动态加载对应板子源码
file(GLOB BOARD_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_TYPE}/*.cc)
```

## 6. 添加新功能的规范流程

### 场景A：添加新的硬件板子

```bash
# 1. 复制相似板子作为模板
cp -r boards/esp-box-3 boards/my-new-board

# 2. 修改 config.h 中的硬件参数
# 3. 修改 board.cc 中的类名和初始化逻辑
# 4. 在 Kconfig.projbuild 添加菜单项
# 5. 在 CMakeLists.txt 添加编译条件

# 核心代码零修改！
```

详细步骤参考：[custom-board.md](./custom-board.md)

### 场景B：添加新的外设类型

例如添加新传感器支持：

```
1. 在 boards/common/ 添加抽象基类
   └── sensor.h  (定义虚拟接口)

2. 在 Board 基类添加可选方法
   └── virtual Sensor* GetSensor() { return nullptr; }

3. 需要此功能的板子实现具体类
   └── boards/xxx/xxx_sensor.cc

4. 应用层通过接口使用
   └── if (auto s = board.GetSensor()) s->Read();
```

### 场景C：添加新的应用功能

例如添加新协议支持：

```
1. 在 protocols/ 添加新协议实现
   └── 继承 Protocol 基类

2. 通过配置选择
   └── Kconfig 中添加协议选项

3. Application 中通过工厂获取
   └── 不硬编码具体协议类
```

## 7. 代码审查检查清单

添加新代码时自问：

- [ ] 这段代码放对层了吗？（硬件相关→BSP，业务逻辑→应用层）
- [ ] 是否通过接口而非具体类调用？
- [ ] 新增的硬件参数是否放在 config.h？
- [ ] 能否复用 boards/common/ 中的已有实现？
- [ ] 是否需要在 Kconfig 添加开关？
- [ ] 其他板子是否会受影响？

## 8. 架构守则

| 守则 | 说明 |
|------|------|
| **单向依赖** | 上层依赖下层，下层绝不引用上层头文件 |
| **接口隔离** | 基类只定义必要方法，可选功能返回 nullptr |
| **配置外置** | 硬件参数放 config.h，运行参数放 Kconfig |
| **共享优先** | 通用代码放 common/，避免各板子重复 |
| **编译时决定** | 用 `#ifdef` 和 CMake 条件，而非运行时判断 |
| **命名一致** | 板子目录名 = Kconfig选项名 = CMake变量名 |

## 9. 快速参考模板

### 新建板子的 config.h 模板

```cpp
#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

// 板子标识
#define BOARD_NAME "my-board"

// 音频配置
#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_X
#define AUDIO_I2S_GPIO_WS   GPIO_NUM_X
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_X
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_X
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_X

// I2C 配置
#define AUDIO_CODEC_I2C_SDA_PIN  GPIO_NUM_X
#define AUDIO_CODEC_I2C_SCL_PIN  GPIO_NUM_X
#define AUDIO_CODEC_ES8311_ADDR  ES8311_CODEC_DEFAULT_ADDR

// 按钮配置
#define BOOT_BUTTON_GPIO GPIO_NUM_0

// 显示屏配置（可选）
#define DISPLAY_WIDTH   320
#define DISPLAY_HEIGHT  240

#endif // _BOARD_CONFIG_H_
```

### 新建板子的 board.cc 模板

```cpp
#include "config.h"
#include "boards/common/wifi_board.h"

class MyBoard : public WifiBoard {
public:
    MyBoard() {
        // 构造函数：初始化成员变量
    }

    void Initialize() override {
        // 1. 初始化I2C/SPI总线
        // 2. 初始化音频编解码器
        // 3. 初始化显示屏（如果有）
        // 4. 初始化按键
    }

    std::string GetBoardType() override {
        return BOARD_NAME;
    }

    AudioCodec* GetAudioCodec() override {
        return &audio_codec_;
    }

private:
    Es8311AudioCodec audio_codec_;  // 根据实际芯片选择
};

DECLARE_BOARD(MyBoard)
```

## 10. 常见继承关系

```
Board (抽象基类)
├── WifiBoard (WiFi网络支持)
│   ├── EspBox3Board
│   ├── KevinC3Board
│   └── ...
├── Ml307Board (4G网络支持)
│   └── ...
└── DualNetworkBoard (WiFi+4G双网)
    └── ...
```

## 11. 目录导航

- **核心应用**: `main/application.cc`
- **音频处理**: `main/audio/`
- **显示系统**: `main/display/`
- **网络协议**: `main/protocols/`
- **Board基类**: `main/boards/common/board.h`
- **共享BSP**: `main/boards/common/`
- **板子实现**: `main/boards/<板子名>/`
- **配置定义**: `main/Kconfig.projbuild`
- **编译规则**: `main/CMakeLists.txt`

---

**架构精髓**：让添加新硬件像填表一样简单，核心逻辑一行不改。
