#include "grind_buddy_uart_integration.h"

#include "application.h"
#include "board.h"
#include "config.h"
#include "display.h"
#include "emotion_policy.h"
#include "grind_buddy_config.h"
#include "grind_buddy_controller.h"
#include "k230_binary_protocol.h"
#include "k230_vision_adapter.h"

#include <driver/gpio.h>
#include <driver/uart.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#ifndef K230_VISION_UART_PORT
#define K230_VISION_UART_PORT UART_NUM_1
#endif

#ifndef K230_VISION_UART_BAUD_RATE
#define K230_VISION_UART_BAUD_RATE 921600
#endif

#ifndef K230_VISION_UART_TX_PIN
#define K230_VISION_UART_TX_PIN UART_PIN_NO_CHANGE
#endif

#ifndef K230_VISION_UART_RX_PIN
#define K230_VISION_UART_RX_PIN GPIO_NUM_10
#endif

#ifndef K230_VISION_UART_RX_BUFFER_SIZE
#define K230_VISION_UART_RX_BUFFER_SIZE 2048
#endif

namespace {

constexpr const char* TAG = "GrindBuddyUart";
constexpr uart_port_t kUartPort = K230_VISION_UART_PORT;
constexpr int kBaudRate = K230_VISION_UART_BAUD_RATE;
constexpr int kRxBufferSize = K230_VISION_UART_RX_BUFFER_SIZE;
constexpr int kTaskStackSize = 4096;
constexpr int kTaskPriority = 5;
constexpr int kTxPin = K230_VISION_UART_TX_PIN;
constexpr gpio_num_t kRxPin = K230_VISION_UART_RX_PIN;
constexpr const char* kVisualWakeWord = "你好小智";

enum class GrindBuddyInteractionMode {
    XiaozhiNative,
    VisionWake,
};

constexpr GrindBuddyInteractionMode kInteractionMode = GrindBuddyInteractionMode::VisionWake;

GrindBuddyController controller(kInteractionMode == GrindBuddyInteractionMode::VisionWake);
K230VisionAdapter vision_adapter;
TaskHandle_t task_handle = nullptr;
int64_t listening_window_deadline_us = 0;
bool vision_allows_listening = false;
uint32_t last_valid_frame_ms = 0;
bool vision_available = false;

const char* ToString(InteractionState state) {
    switch (state) {
        case InteractionState::Idle:
            return "Idle";
        case InteractionState::Aware:
            return "Aware";
        case InteractionState::ListeningWindow:
            return "ListeningWindow";
        case InteractionState::Listening:
            return "Listening";
        case InteractionState::Thinking:
            return "Thinking";
        case InteractionState::Speaking:
            return "Speaking";
        case InteractionState::Error:
            return "Error";
    }
    return "Unknown";
}

const char* ToString(GrindBuddyActionType type) {
    switch (type) {
        case GrindBuddyActionType::ReportVisionEvent:
            return "ReportVisionEvent";
        case GrindBuddyActionType::OpenListeningWindow:
            return "OpenListeningWindow";
        case GrindBuddyActionType::StartVoiceSession:
            return "StartVoiceSession";
        case GrindBuddyActionType::StopVoiceSession:
            return "StopVoiceSession";
        case GrindBuddyActionType::SetDisplayState:
            return "SetDisplayState";
    }
    return "Unknown";
}

void ApplyCurrentVisionGate() {
    if (vision_allows_listening) {
        return;
    }
    auto& app = Application::GetInstance();
    auto state = app.GetDeviceState();
    if (state == kDeviceStateListening) {
        ESP_LOGI(TAG, "VisionWake: blocking listening without current frontal gaze");
        app.SilenceListening();
    }
}

void ExecuteAction(const GrindBuddyAction& action) {
    if (kInteractionMode != GrindBuddyInteractionMode::VisionWake) {
        return;
    }

    auto& app = Application::GetInstance();
    auto app_state = app.GetDeviceState();
    switch (action.type) {
        case GrindBuddyActionType::StartVoiceSession:
            vision_allows_listening = true;
            if (app_state == kDeviceStateIdle) {
                ESP_LOGI(TAG, "VisionWake: invoking Xiaozhi wake word");
                app.WakeWordInvoke(kVisualWakeWord);
            } else {
                ESP_LOGI(TAG, "VisionWake: skip wake, Xiaozhi state=%d", static_cast<int>(app_state));
            }
            break;
        case GrindBuddyActionType::StopVoiceSession:
            vision_allows_listening = false;
            if (app_state == kDeviceStateListening) {
                ESP_LOGI(TAG, "VisionWake: silently stopping Xiaozhi listening");
                app.SilenceListening();
            } else if (app_state == kDeviceStateConnecting || app_state == kDeviceStateSpeaking) {
                ESP_LOGI(TAG, "VisionWake: current gaze is not frontal, Xiaozhi state=%d", static_cast<int>(app_state));
            } else {
                ESP_LOGI(TAG, "VisionWake: skip stop, Xiaozhi state=%d", static_cast<int>(app_state));
            }
            break;
        case GrindBuddyActionType::SetDisplayState: {
            const auto output = EmotionOutputForState(action.state);
            auto display = Board::GetInstance().GetDisplay();
            if (display) {
                display->SetEmotion(output.emotion);
                display->SetChatMessage("system", output.status_text);
            }
            ESP_LOGI(TAG, "Emotion output: state=%s emotion=%s motion=%s sound=%s text=%s",
                     ToString(action.state),
                     output.emotion,
                     output.motion,
                     output.sound[0] ? output.sound : "-",
                     output.status_text[0] ? output.status_text : "-");
            break;
        }
        default:
            break;
    }
}

void LogActions() {
    for (const auto& action : controller.actions()) {
        ESP_LOGI(TAG, "Grind action: %s event=%s state=%s",
                 ToString(action.type),
                 action.event_type.empty() ? "-" : action.event_type.c_str(),
                 ToString(action.state));
        ExecuteAction(action);
    }
    controller.ClearActions();
}

void ArmListeningWindowTimer() {
    listening_window_deadline_us =
        esp_timer_get_time() + grind_buddy_config::kListeningWindowTimeoutUs;
}

void MaybeExpireListeningWindow() {
    if (controller.state() != InteractionState::ListeningWindow) {
        listening_window_deadline_us = 0;
        return;
    }
    if (listening_window_deadline_us > 0 && esp_timer_get_time() >= listening_window_deadline_us) {
        listening_window_deadline_us = 0;
        controller.OnListeningWindowTimeout();
        LogActions();
    }
}

void LogVisionEvent(const K230VisionEvent& event) {
    if (event.type == "face.pose") {
        ESP_LOGD(TAG, "K230 pose yaw=%.2f pitch=%.2f roll=%.2f confidence=%.2f",
                 event.yaw,
                 event.pitch,
                 event.roll,
                 event.confidence);
        return;
    }

    ESP_LOGI(TAG, "K230 event: %s confidence=%.2f",
             event.type.c_str(),
             event.confidence);
}

void OnVisionEvent(const K230VisionEvent& event) {
    LogVisionEvent(event);
    controller.OnK230Event(event);
    if (controller.state() == InteractionState::ListeningWindow) {
        ArmListeningWindowTimer();
    }
    LogActions();
}

void OnVisionEvents(const std::vector<K230VisionEvent>& events) {
    for (const auto& event : events) {
        OnVisionEvent(event);
    }
}

void NoteValidFrame(uint32_t now_ms) {
    last_valid_frame_ms = now_ms;
    vision_available = true;
}

void MaybeExpireVision(uint32_t now_ms) {
    if (!vision_available) {
        return;
    }

    if (static_cast<uint32_t>(now_ms - last_valid_frame_ms) <=
        grind_buddy_config::kVisionTimeoutMs) {
        return;
    }

    vision_available = false;
    ESP_LOGW(TAG, "K230 vision timeout");
    OnVisionEvents(vision_adapter.OnVisionTimeout(now_ms));
}

void OnBinaryFrame(const K230BinaryFrame& frame, uint32_t now_ms) {
    NoteValidFrame(now_ms);

    switch (frame.type) {
        case K230BinaryMessageType::Heartbeat:
            ESP_LOGD(TAG, "K230 heartbeat seq=%u ts=%lu",
                     static_cast<unsigned>(frame.sequence),
                     static_cast<unsigned long>(frame.timestamp_ms));
            break;
        case K230BinaryMessageType::Face: {
            K230FaceObservation observation = {};
            if (!DecodeK230Face(frame, observation)) {
                ESP_LOGW(TAG, "Invalid K230 face payload length=%u",
                         static_cast<unsigned>(frame.payload_length));
                return;
            }
            OnVisionEvents(vision_adapter.OnFace(observation, frame.timestamp_ms));
            break;
        }
        case K230BinaryMessageType::FaceLost:
            OnVisionEvents(vision_adapter.OnFaceLost(frame.timestamp_ms));
            break;
        case K230BinaryMessageType::Error: {
            uint16_t error_code = 0;
            if (DecodeK230Error(frame, error_code)) {
                ESP_LOGW(TAG, "K230 error code=%u", static_cast<unsigned>(error_code));
            } else {
                ESP_LOGW(TAG, "Invalid K230 error payload length=%u",
                         static_cast<unsigned>(frame.payload_length));
            }
            break;
        }
        default:
            ESP_LOGW(TAG, "Unknown K230 message type=%u",
                     static_cast<unsigned>(frame.type));
            break;
    }
}

void UartTask(void*) {
    K230BinaryParser parser;
    K230BinaryFrame frame;
    uint8_t buffer[128];

    while (true) {
        int len = uart_read_bytes(kUartPort, buffer, sizeof(buffer), pdMS_TO_TICKS(100));
        if (len < 0) {
            ESP_LOGW(TAG, "UART read failed: %d", len);
            continue;
        }
        for (int i = 0; i < len; ++i) {
            if (parser.Feed(buffer[i], frame)) {
                OnBinaryFrame(frame, static_cast<uint32_t>(esp_timer_get_time() / 1000));
            }
        }
        MaybeExpireListeningWindow();
        MaybeExpireVision(static_cast<uint32_t>(esp_timer_get_time() / 1000));
        ApplyCurrentVisionGate();
    }
}

}  // namespace

void StartGrindBuddyUartIntegration() {
    if (task_handle != nullptr) {
        ESP_LOGW(TAG, "Grind Buddy UART integration already started");
        return;
    }

    uart_config_t uart_config = {};
    uart_config.baud_rate = kBaudRate;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = UART_SCLK_DEFAULT;

    ESP_ERROR_CHECK(uart_driver_install(kUartPort, kRxBufferSize, 0, 0, nullptr, 0));
    ESP_ERROR_CHECK(uart_param_config(kUartPort, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(kUartPort, kTxPin, kRxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    BaseType_t created = xTaskCreate(UartTask,
                                     "grind_uart",
                                     kTaskStackSize,
                                     nullptr,
                                     kTaskPriority,
                                     &task_handle);
    if (created != pdPASS) {
        task_handle = nullptr;
        ESP_LOGE(TAG, "Failed to create UART task");
        return;
    }

    ESP_LOGI(TAG, "Grind Buddy UART integration started: uart=%d rx=%d baud=%d",
             static_cast<int>(kUartPort),
             static_cast<int>(kRxPin),
             kBaudRate);
}
