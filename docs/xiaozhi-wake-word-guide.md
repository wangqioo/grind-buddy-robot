# 小智唤醒词修改指南

本文档详细说明如何修改小智项目的唤醒词，从代码层面提炼核心方法和注意事项。

---

## 1. 唤醒词实现架构

### 1.1 三种唤醒词模式

小智支持三种唤醒词实现方式：

| 模式 | Kconfig选项 | 硬件要求 | 特点 |
|------|------------|----------|------|
| **禁用** | `WAKE_WORD_DISABLED` | 无 | 无唤醒词检测 |
| **Wakenet（无AFE）** | `USE_ESP_WAKE_WORD` | ESP32-C3/C5/C6 或 ESP32+PSRAM | 内置唤醒词，无回声消除 |
| **Wakenet（带AFE）** | `USE_AFE_WAKE_WORD` | ESP32-S3/P4 + PSRAM | 内置唤醒词，带回声消除（推荐） |
| **Multinet（自定义）** | `USE_CUSTOM_WAKE_WORD` | ESP32-S3/P4 + PSRAM | 可自定义唤醒词 |

### 1.2 代码结构

```
main/audio/
├── wake_word.h                    # WakeWord 抽象基类
├── wake_words/
│   ├── esp_wake_word.cc/h         # Wakenet 实现（内置唤醒词）
│   ├── afe_wake_word.cc/h         # AFE + Wakenet 实现
│   └── custom_wake_word.cc/h      # Multinet 实现（自定义唤醒词）
```

**关键接口**（wake_word.h）：

```cpp
class WakeWord {
public:
    virtual bool Initialize(AudioCodec* codec, srmodel_list_t* models_list) = 0;
    virtual void Feed(const std::vector<int16_t>& data) = 0;  // 喂音频数据
    virtual void OnWakeWordDetected(callback) = 0;            // 检测回调
    virtual const std::string& GetLastDetectedWakeWord() = 0;
};
```

---

## 2. 方法一：使用内置唤醒词（Wakenet）

### 2.1 可用的内置唤醒词

ESP-SR 的 Wakenet 模型支持以下唤醒词（示例）：

- **中文**：
  - "你好小智" (ni hao xiao zhi)
  - "小爱同学" (xiao ai tong xue)
  - "你好小杰" (ni hao xiao jie)

- **英文**：
  - "Hi Xiaozhi"
  - "Hi ESP"
  - "Alexa"

> **注意**：具体可用的唤醒词取决于编译时包含的模型文件。模型文件存储在 `model` 或 `assets` 分区。

### 2.2 配置方法

```bash
cd xiaozhi-esp32
idf.py menuconfig
```

导航到：
```
Xiaozhi Assistant
  └── Wake Word Implementation Type
      ├── [*] Wakenet model without AFE      # ESP32-C3/C5/C6
      └── [*] Wakenet model with AFE         # ESP32-S3/P4（推荐）
```

### 2.3 代码实现（esp_wake_word.cc）

**核心逻辑**：

```cpp
// 初始化（第 17-44 行）
bool EspWakeWord::Initialize(AudioCodec* codec, srmodel_list_t* models_list) {
    // 1. 从 model 分区加载模型列表
    wakenet_model_ = esp_srmodel_init("model");

    // 2. 获取模型句柄
    char *model_name = wakenet_model_->model_name[0];
    wakenet_iface_ = esp_wn_handle_from_name(model_name);

    // 3. 创建检测器实例，DET_MODE_95 表示 95% 置信度
    wakenet_data_ = wakenet_iface_->create(model_name, DET_MODE_95);

    return true;
}

// 检测逻辑（第 59-73 行）
void EspWakeWord::Feed(const std::vector<int16_t>& data) {
    // 调用 Wakenet 的 detect 方法
    int res = wakenet_iface_->detect(wakenet_data_, (int16_t *)data.data());

    if (res > 0) {  // 检测到唤醒词
        // 获取唤醒词名称
        last_detected_wake_word_ = wakenet_iface_->get_word_name(wakenet_data_, res);

        // 触发回调
        if (wake_word_detected_callback_) {
            wake_word_detected_callback_(last_detected_wake_word_);
        }
    }
}
```

### 2.4 更换内置唤醒词模型

如需使用不同的内置唤醒词：

1. **下载新模型**：从 [ESP-SR GitHub](https://github.com/espressif/esp-sr) 获取对应的 `.bin` 文件
2. **替换模型文件**：将新模型放入 `model` 或 `assets` 分区
3. **重新编译**：无需修改代码，模型会自动加载

---

## 3. 方法二：自定义唤醒词（Multinet）

### 3.1 适用场景

- 需要完全自定义的唤醒词（如品牌名、产品名）
- 支持中文拼音输入
- 需要 ESP32-S3/P4 + PSRAM

### 3.2 Kconfig 配置

位置：`main/Kconfig.projbuild` 第 615-643 行

```
config USE_CUSTOM_WAKE_WORD
    bool "Multinet model (Custom Wake Word)"
    depends on (IDF_TARGET_ESP32S3 || IDF_TARGET_ESP32P4) && SPIRAM

config CUSTOM_WAKE_WORD
    string "Custom Wake Word"
    default "xiao tu dou"
    help
        Custom Wake Word, use pinyin for Chinese, separated by spaces

config CUSTOM_WAKE_WORD_DISPLAY
    string "Custom Wake Word Display"
    default "小土豆"
    help
        Greeting sent to the server after wake word detection

config CUSTOM_WAKE_WORD_THRESHOLD
    int "Custom Wake Word Threshold (%)"
    default 20
    range 1 99
    help
        The smaller the more sensitive, default 20
```

### 3.3 配置步骤

```bash
idf.py menuconfig
```

导航到：
```
Xiaozhi Assistant
  └── Wake Word Implementation Type
      └── [*] Multinet model (Custom Wake Word)

  └── Custom Wake Word: "ni hao xiao ming"        # 拼音，空格分隔
  └── Custom Wake Word Display: "你好小明"         # 显示名称
  └── Custom Wake Word Threshold (%): 20          # 识别阈值
```

### 3.4 代码实现（custom_wake_word.cc）

**核心逻辑**：

```cpp
// 初始化（第 87-131 行）
bool CustomWakeWord::Initialize(AudioCodec* codec, srmodel_list_t* models_list) {
    // 1. 加载模型列表
    models_ = esp_srmodel_init("model");

    // 2. 从 Kconfig 读取配置
    #ifdef CONFIG_CUSTOM_WAKE_WORD
        threshold_ = CONFIG_CUSTOM_WAKE_WORD_THRESHOLD / 100.0f;
        commands_.push_back({
            CONFIG_CUSTOM_WAKE_WORD,         // 拼音："ni hao xiao ming"
            CONFIG_CUSTOM_WAKE_WORD_DISPLAY, // 显示："你好小明"
            "wake"                           // 动作类型
        });
    #endif

    // 3. 初始化 Multinet 模型
    mn_name_ = esp_srmodel_filter(models_, ESP_MN_PREFIX, language_.c_str());
    multinet_ = esp_mn_handle_from_name(mn_name_);
    multinet_model_data_ = multinet_->create(mn_name_, duration_);

    // 4. 设置识别阈值
    multinet_->set_det_threshold(multinet_model_data_, threshold_);

    // 5. 添加命令词
    esp_mn_commands_clear();
    for (int i = 0; i < commands_.size(); i++) {
        esp_mn_commands_add(i + 1, commands_[i].command.c_str());
    }
    esp_mn_commands_update();

    return true;
}

// 检测逻辑（第 145-187 行）
void CustomWakeWord::Feed(const std::vector<int16_t>& data) {
    // 处理双声道音频（只取左声道）
    if (codec_->input_channels() == 2) {
        auto mono_data = std::vector<int16_t>(data.size() / 2);
        for (size_t i = 0, j = 0; i < mono_data.size(); ++i, j += 2) {
            mono_data[i] = data[j];
        }
        mn_state = multinet_->detect(multinet_model_data_, mono_data.data());
    } else {
        mn_state = multinet_->detect(multinet_model_data_, data.data());
    }

    // 处理检测结果
    if (mn_state == ESP_MN_STATE_DETECTED) {
        esp_mn_results_t *mn_result = multinet_->get_results(multinet_model_data_);
        auto& command = commands_[mn_result->command_id[0] - 1];

        if (command.action == "wake") {
            last_detected_wake_word_ = command.text;  // "你好小明"
            if (wake_word_detected_callback_) {
                wake_word_detected_callback_(last_detected_wake_word_);
            }
        }
    }
}
```

### 3.5 自定义唤醒词配置示例

| 唤醒词 | CUSTOM_WAKE_WORD | CUSTOM_WAKE_WORD_DISPLAY |
|--------|------------------|--------------------------|
| 你好小智 | `ni hao xiao zhi` | `你好小智` |
| 小爱同学 | `xiao ai tong xue` | `小爱同学` |
| 你好小明 | `ni hao xiao ming` | `你好小明` |
| 嗨小助手 | `hai xiao zhu shou` | `嗨小助手` |

---

## 4. 高级配置：通过 index.json 动态配置

### 4.1 配置文件位置

`assets` 分区中的 `index.json` 文件可以动态配置唤醒词（无需重新编译）。

### 4.2 JSON 配置格式

位置：代码中第 39-84 行解析逻辑

```json
{
  "multinet_model": {
    "language": "cn",
    "duration": 3000,
    "threshold": 0.2,
    "commands": [
      {
        "command": "ni hao xiao zhi",
        "text": "你好小智",
        "action": "wake"
      },
      {
        "command": "xiao ai tong xue",
        "text": "小爱同学",
        "action": "wake"
      }
    ]
  }
}
```

**字段说明**：

| 字段 | 类型 | 说明 |
|------|------|------|
| `language` | string | 语言代码（"cn"、"en"） |
| `duration` | int | 检测持续时间（毫秒） |
| `threshold` | float | 识别阈值（0.0-1.0，越小越敏感） |
| `commands[].command` | string | 唤醒词拼音 |
| `commands[].text` | string | 显示文本 |
| `commands[].action` | string | 动作类型（"wake"） |

### 4.3 动态配置优势

- **无需重新编译**：直接修改 assets 分区的 JSON 文件
- **支持多个唤醒词**：一个设备可以响应多个唤醒词
- **OTA 更新配置**：通过网络更新唤醒词配置

---

## 5. 关键参数调优

### 5.1 识别阈值（Threshold）

**作用**：控制唤醒词识别的灵敏度

| 阈值范围 | 效果 | 适用场景 |
|---------|------|---------|
| 1-10 | 极高敏感 | 安静环境，误唤醒率高 |
| 10-20 | 高敏感 | 家庭环境（推荐） |
| 20-30 | 中等敏感 | 有一定噪声的环境 |
| 30-50 | 低敏感 | 嘈杂环境，漏检率高 |

**配置位置**：
- Kconfig: `CUSTOM_WAKE_WORD_THRESHOLD`（百分比）
- JSON: `threshold`（0.0-1.0）

### 5.2 检测模式（Detection Mode）

**Wakenet 支持的模式**（esp_wake_word.cc 第 38 行）：

```cpp
wakenet_data_ = wakenet_iface_->create(model_name, DET_MODE_95);
```

| 模式 | 置信度 | 误报率 | 漏检率 |
|------|--------|--------|--------|
| `DET_MODE_90` | 90% | 较高 | 较低 |
| `DET_MODE_95` | 95% | 中等 | 中等 |
| `DET_MODE_2CH_90` | 90%（双通道） | 较高 | 较低 |
| `DET_MODE_2CH_95` | 95%（双通道） | 中等 | 中等 |

### 5.3 音频参数

**采样率**：16kHz（固定）
**采样深度**：16bit（固定）
**声道数**：
- 单声道：直接处理
- 双声道：自动提取左声道（第 152-156 行）

---

## 6. 注意事项与常见问题

### 6.1 硬件限制

| 芯片型号 | PSRAM要求 | 支持的唤醒词类型 |
|---------|-----------|-----------------|
| ESP32-S3 | 必须 | 全部支持（推荐） |
| ESP32-P4 | 必须 | 全部支持 |
| ESP32-C3/C5/C6 | 否 | 仅内置 Wakenet |
| ESP32 | 必须 | 仅内置 Wakenet |

**检查方法**：
```bash
idf.py menuconfig
# Component config → ESP PSRAM → Support for external PSRAM
```

### 6.2 模型文件管理

**分区布局**：

- **v1 分区**（旧版）：
  - `model` 分区（960KB）：存储 Wakenet/Multinet 模型

- **v2 分区**（新版）：
  - `assets` 分区（1.4MB-3.9MB）：统一存储模型和资源
  - 支持网络动态下载

**模型初始化**（第 21-24 行）：

```cpp
// 从 model 或 assets 分区加载
wakenet_model_ = esp_srmodel_init("model");
```

### 6.3 中文拼音规范

**正确格式**：
- ✅ `ni hao xiao zhi`（空格分隔）
- ✅ `xiao ai tong xue`
- ❌ `nihaoxiaozhi`（没有空格）
- ❌ `ni-hao-xiao-zhi`（不能用连字符）

**声调处理**：
- 不需要声调标记
- Multinet 会自动处理声调变化

### 6.4 唤醒词长度建议

| 长度 | 音节数 | 示例 | 识别效果 |
|------|--------|------|---------|
| 短 | 2-3 | "小智" | 容易误触发 |
| 中 | 4-5 | "你好小智" | 平衡（推荐） |
| 长 | 6+ | "你好我的小助手" | 识别率降低 |

### 6.5 回声消除（AEC）配置

**相关配置**（Kconfig.projbuild 第 659-668 行）：

```
config USE_DEVICE_AEC
    bool "Enable Device-Side AEC"
    depends on USE_AUDIO_PROCESSOR
```

**影响**：
- 开启 AEC：减少扬声器播放对唤醒词识别的干扰
- 关闭 AEC：可能在播放音频时无法唤醒

### 6.6 发送唤醒词音频数据

**配置**（第 645-650 行）：

```
config SEND_WAKE_WORD_DATA
    bool "Send Wake Word Data"
    default y
```

**作用**：
- 启用：将唤醒词前 2 秒的音频发送给服务端（用于改善识别）
- 禁用：只发送唤醒后的语音

**实现逻辑**（第 196-203 行）：

```cpp
// 存储最近 2 秒音频数据
void CustomWakeWord::StoreWakeWordData(const std::vector<int16_t>& data) {
    wake_word_pcm_.push_back(data);
    while (wake_word_pcm_.size() > 2000 / 30) {  // 2秒 / 30ms每帧
        wake_word_pcm_.pop_front();
    }
}
```

---

## 7. 完整操作流程

### 7.1 使用自定义唤醒词（Multinet）

```bash
# 1. 进入项目目录
cd /Users/wq/grind-buddy-robot/firmware/esp32s3-main

# 2. 配置编译选项
idf.py menuconfig
# → Xiaozhi Assistant
#   → Wake Word Implementation Type → Multinet model (Custom Wake Word)
#   → Custom Wake Word: "ni hao xiao ming"
#   → Custom Wake Word Display: "你好小明"
#   → Custom Wake Word Threshold: 20

# 3. 编译固件
idf.py build

# 4. 烧录固件
idf.py flash

# 5. 查看日志验证
idf.py monitor
# 应看到：Command: ni hao xiao ming, Text: 你好小明, Action: wake
```

### 7.2 调整识别灵敏度

如果出现误唤醒：
```bash
idf.py menuconfig
# → Custom Wake Word Threshold: 30  # 增加阈值
```

如果唤醒困难：
```bash
idf.py menuconfig
# → Custom Wake Word Threshold: 10  # 降低阈值
```

### 7.3 切换到内置唤醒词

```bash
idf.py menuconfig
# → Wake Word Implementation Type → Wakenet model with AFE
idf.py build
idf.py flash
```

---

## 8. 调试技巧

### 8.1 查看日志

```bash
idf.py monitor
```

**关键日志**：

```
CustomWakeWord: Command: ni hao xiao ming, Text: 你好小明, Action: wake
CustomWakeWord: Custom wake word detected: command_id=1, string=ni hao xiao ming, prob=0.95
```

### 8.2 检查模型加载

```cpp
// 第 103-117 行的错误检查
if (models_ == nullptr || models_->num == -1) {
    ESP_LOGE(TAG, "Failed to initialize wakenet model");
    ESP_LOGI(TAG, "Please refer to https://xxx to add custom wake word");
    return false;
}
```

### 8.3 音频输入检查

确保麦克风正常工作：
```bash
# 启用音频调试器
idf.py menuconfig
# → Xiaozhi Assistant → Enable Audio Debugger
```

---

## 9. 代码修改清单

如需通过代码修改（不推荐，优先使用 Kconfig）：

| 修改目标 | 文件位置 | 行号 |
|---------|---------|------|
| 默认唤醒词拼音 | `main/Kconfig.projbuild` | 625 |
| 默认显示名称 | `main/Kconfig.projbuild` | 632 |
| 默认阈值 | `main/Kconfig.projbuild` | 639 |
| 检测模式 | `main/audio/wake_words/esp_wake_word.cc` | 38 |
| 音频缓存时长 | `main/audio/wake_words/custom_wake_word.cc` | 200 |

---

## 10. 参考资料

- **ESP-SR 官方文档**：https://github.com/espressif/esp-sr
- **小智自定义唤醒词教程**：https://pcn7cs20v8cr.feishu.cn/wiki/CpQjwQsCJiQSWSkYEvrcxcbVnwh
- **Multinet 配置说明**（代码第 116 行引用）

---

**总结**：小智的唤醒词系统设计灵活，支持从配置文件到编译选项的多层次修改，推荐使用 Kconfig 或 JSON 动态配置，避免直接修改代码。
