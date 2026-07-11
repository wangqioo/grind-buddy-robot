#include "emote_display.h"

// Standard C++ headers
#include <cstring>
#include <memory>
#include <unordered_map>
#include <tuple>

// Standard C headers
#include <sys/time.h>
#include <time.h>

// ESP-IDF headers
#include <esp_log.h>
#include <esp_lcd_panel_io.h>
#include <esp_timer.h>

// FreeRTOS headers
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Project headers
#include "assets.h"
#include "assets/lang_config.h"
#include "board.h"
#include "display/emote_corner_badge_asset.h"
#include "display/emote_pack_utils.h"
#include "gfx.h"
#include "sdkconfig.h"

LV_FONT_DECLARE(BUILTIN_TEXT_FONT);

namespace emote {

// ============================================================================
// Constants and Type Definitions
// ============================================================================

static const char* TAG = "EmoteDisplay";
// UI Element Names - Centralized Management
#define UI_ELEMENT_EYE_ANIM      "eye_anim"
#define UI_ELEMENT_TOAST_LABEL   "toast_label"
#define UI_ELEMENT_CLOCK_LABEL   "clock_label"
#define UI_ELEMENT_LISTEN_ANIM   "listen_anim"
#define UI_ELEMENT_STATUS_ICON   "status_icon"
#define UI_ELEMENT_CORNER_BADGE  "corner_badge"

// Icon Names - Centralized Management
#define ICON_MIC                 "icon_mic"
#define ICON_BATTERY             "icon_Battery"
#define ICON_SPEAKER_ZZZ         "icon_speaker_zzz"
#define ICON_WIFI_FAILED         "icon_WiFi_failed"
#define ICON_WIFI_OK             "icon_wifi"
#define ICON_LISTEN              "listen"

using FlushIoReadyCallback = std::function<bool(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*, void*)>;
using FlushCallback = std::function<void(gfx_handle_t, int, int, int, int, const void*)>;

// ============================================================================
// Global Variables
// ============================================================================

// UI element management
static gfx_obj_t* g_obj_label_toast = nullptr;
static gfx_obj_t* g_obj_label_clock = nullptr;
static gfx_obj_t* g_obj_anim_eye = nullptr;
static gfx_obj_t* g_obj_anim_listen = nullptr;
static gfx_obj_t* g_obj_img_status = nullptr;
static gfx_obj_t* g_obj_img_corner_badge = nullptr;

// Track current icon to determine when to show time
static std::string g_current_icon_type = ICON_WIFI_FAILED;
static gfx_image_dsc_t g_icon_img_dsc;

#ifndef CONFIG_GRIND_BUDDY_EMOTE_GEN_PREVIEW_NAME
#define CONFIG_GRIND_BUDDY_EMOTE_GEN_PREVIEW_NAME "asleep_215s"
#endif


// ============================================================================
// Forward Declarations
// ============================================================================

class EmoteDisplay;
class EmoteEngine;

enum class UIDisplayMode : uint8_t {
    SHOW_LISTENING = 1,  // Show g_obj_anim_listen
    SHOW_TIME = 2,      // Show g_obj_label_clock
    SHOW_TIPS = 3       // Show g_obj_label_toast
};

// ============================================================================
// Helper Functions
// ============================================================================

// Function to convert align string to GFX_ALIGN enum value
char StringToGfxAlign(const std::string &align_str)
{
    static const std::unordered_map<std::string, char> align_map = {
        {"GFX_ALIGN_DEFAULT",           GFX_ALIGN_DEFAULT},
        {"GFX_ALIGN_TOP_LEFT",          GFX_ALIGN_TOP_LEFT},
        {"GFX_ALIGN_TOP_MID",           GFX_ALIGN_TOP_MID},
        {"GFX_ALIGN_TOP_RIGHT",         GFX_ALIGN_TOP_RIGHT},
        {"GFX_ALIGN_LEFT_MID",          GFX_ALIGN_LEFT_MID},
        {"GFX_ALIGN_CENTER",            GFX_ALIGN_CENTER},
        {"GFX_ALIGN_RIGHT_MID",         GFX_ALIGN_RIGHT_MID},
        {"GFX_ALIGN_BOTTOM_LEFT",       GFX_ALIGN_BOTTOM_LEFT},
        {"GFX_ALIGN_BOTTOM_MID",        GFX_ALIGN_BOTTOM_MID},
        {"GFX_ALIGN_BOTTOM_RIGHT",      GFX_ALIGN_BOTTOM_RIGHT},
        {"GFX_ALIGN_OUT_TOP_LEFT",      GFX_ALIGN_OUT_TOP_LEFT},
        {"GFX_ALIGN_OUT_TOP_MID",       GFX_ALIGN_OUT_TOP_MID},
        {"GFX_ALIGN_OUT_TOP_RIGHT",     GFX_ALIGN_OUT_TOP_RIGHT},
        {"GFX_ALIGN_OUT_LEFT_TOP",      GFX_ALIGN_OUT_LEFT_TOP},
        {"GFX_ALIGN_OUT_LEFT_MID",      GFX_ALIGN_OUT_LEFT_MID},
        {"GFX_ALIGN_OUT_LEFT_BOTTOM",   GFX_ALIGN_OUT_LEFT_BOTTOM},
        {"GFX_ALIGN_OUT_RIGHT_TOP",     GFX_ALIGN_OUT_RIGHT_TOP},
        {"GFX_ALIGN_OUT_RIGHT_MID",     GFX_ALIGN_OUT_RIGHT_MID},
        {"GFX_ALIGN_OUT_RIGHT_BOTTOM",  GFX_ALIGN_OUT_RIGHT_BOTTOM},
        {"GFX_ALIGN_OUT_BOTTOM_LEFT",   GFX_ALIGN_OUT_BOTTOM_LEFT},
        {"GFX_ALIGN_OUT_BOTTOM_MID",    GFX_ALIGN_OUT_BOTTOM_MID},
        {"GFX_ALIGN_OUT_BOTTOM_RIGHT",  GFX_ALIGN_OUT_BOTTOM_RIGHT}
    };

    const auto it = align_map.find(align_str);
    if (it != align_map.cend()) {
        return it->second;
    }

    ESP_LOGW(TAG, "Unknown align string: %s, using GFX_ALIGN_DEFAULT", align_str.c_str());
    return GFX_ALIGN_DEFAULT;
}

// ============================================================================
// EmoteEngine Class Declaration
// ============================================================================

class EmoteEngine {
public:
    EmoteEngine(const esp_lcd_panel_handle_t panel, const esp_lcd_panel_io_handle_t panel_io,
                const int width, const int height, EmoteDisplay* const display);
    ~EmoteEngine();

    void SetEyes(const std::string &emoji_name, const bool repeat, const int fps, EmoteDisplay* const display);
    void SetIcon(const std::string &icon_name, EmoteDisplay* const display);

    void* GetEngineHandle() const
    {
        return engine_handle_;
    }

    // Callback functions (public to be accessible from static helper functions)
    static bool OnFlushIoReady(const esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t* const edata, void* const user_ctx);
    static void OnFlush(const gfx_handle_t handle, const int x_start, const int y_start, const int x_end, const int y_end, const void* const color_data);

private:
    gfx_handle_t engine_handle_;
};

// ============================================================================
// UI Management Functions
// ============================================================================

static void SetUIDisplayMode(const UIDisplayMode mode, EmoteDisplay* const display)
{
    if (!display) {
        ESP_LOGE(TAG, "SetUIDisplayMode: display is nullptr");
        return;
    }

    gfx_obj_set_visible(g_obj_anim_listen, false);
    gfx_obj_set_visible(g_obj_label_clock, false);
    gfx_obj_set_visible(g_obj_label_toast, false);

    // Show the selected control
    switch (mode) {
    case UIDisplayMode::SHOW_LISTENING: {
        gfx_obj_set_visible(g_obj_anim_listen, true);
        const AssetData emoji_data = display->GetIconData(ICON_LISTEN);
        if (emoji_data.data) {
            gfx_anim_set_src(g_obj_anim_listen, emoji_data.data, emoji_data.size);
            gfx_anim_set_segment(g_obj_anim_listen, 0, 0xFFFF, 20, true);
            gfx_anim_start(g_obj_anim_listen);
        }
        break;
    }
    case UIDisplayMode::SHOW_TIME:
        gfx_obj_set_visible(g_obj_label_clock, true);
        break;
    case UIDisplayMode::SHOW_TIPS:
        gfx_obj_set_visible(g_obj_label_toast, true);
        break;
    }
}

// ============================================================================
// Graphics Initialization Functions
// ============================================================================

static void InitializeGraphics(const esp_lcd_panel_handle_t panel, gfx_handle_t* const engine_handle,
                               const int width, const int height)
{
    if (!panel || !engine_handle) {
        ESP_LOGE(TAG, "InitializeGraphics: Invalid parameters");
        return;
    }

    gfx_core_config_t gfx_cfg = {
        .flush_cb = EmoteEngine::OnFlush,
        .user_data = panel,
        .flags = {
            .swap = true,
            .double_buffer = true,
            .buff_dma = true,
        },
        .h_res = static_cast<uint32_t>(width),
        .v_res = static_cast<uint32_t>(height),
        .fps = 30,
        .buffers = {
            .buf1 = nullptr,
            .buf2 = nullptr,
            .buf_pixels = static_cast<size_t>(width * 16),
        },
        .task = GFX_EMOTE_INIT_CONFIG()
    };

    gfx_cfg.task.task_stack_caps = MALLOC_CAP_DEFAULT;
    gfx_cfg.task.task_affinity = 0;
    gfx_cfg.task.task_priority = 5;
    gfx_cfg.task.task_stack = 8 * 1024;

    *engine_handle = gfx_emote_init(&gfx_cfg);
}

static void SetupUI(const gfx_handle_t engine_handle, EmoteDisplay* const display,
                    const int width, const int height)
{
    if (!display) {
        ESP_LOGE(TAG, "SetupUI: display is nullptr");
        return;
    }

    gfx_emote_set_bg_color(engine_handle, GFX_COLOR_HEX(0x000000));

    g_obj_anim_eye = gfx_anim_create(engine_handle);
    gfx_obj_align(g_obj_anim_eye, GFX_ALIGN_LEFT_MID, 10, 30);
    gfx_anim_set_auto_mirror(g_obj_anim_eye, true);
    gfx_obj_set_visible(g_obj_anim_eye, false);

    g_obj_label_toast = gfx_label_create(engine_handle);
    gfx_obj_align(g_obj_label_toast, GFX_ALIGN_TOP_MID, 0, 20);
    gfx_obj_set_size(g_obj_label_toast, 200, 40);
    gfx_label_set_text(g_obj_label_toast, Lang::Strings::INITIALIZING);
    gfx_label_set_color(g_obj_label_toast, GFX_COLOR_HEX(0xFFFFFF));
    gfx_label_set_text_align(g_obj_label_toast, GFX_TEXT_ALIGN_CENTER);
    gfx_label_set_long_mode(g_obj_label_toast, GFX_LABEL_LONG_SCROLL);
    gfx_label_set_scroll_speed(g_obj_label_toast, 20);
    gfx_label_set_scroll_loop(g_obj_label_toast, true);
    gfx_label_set_font(g_obj_label_toast, (gfx_font_t)&BUILTIN_TEXT_FONT);

    g_obj_label_clock = gfx_label_create(engine_handle);
    gfx_obj_align(g_obj_label_clock, GFX_ALIGN_TOP_MID, 0, 15);
    gfx_obj_set_size(g_obj_label_clock, 200, 50);
    gfx_label_set_text(g_obj_label_clock, "--:--");
    gfx_label_set_color(g_obj_label_clock, GFX_COLOR_HEX(0xFFFFFF));
    gfx_label_set_text_align(g_obj_label_clock, GFX_TEXT_ALIGN_CENTER);
    gfx_label_set_font(g_obj_label_clock, (gfx_font_t)&BUILTIN_TEXT_FONT);

    g_obj_anim_listen = gfx_anim_create(engine_handle);
    gfx_obj_align(g_obj_anim_listen, GFX_ALIGN_TOP_MID, 0, 5);
    gfx_anim_start(g_obj_anim_listen);
    gfx_obj_set_visible(g_obj_anim_listen, false);

    g_obj_img_status = gfx_img_create(engine_handle);
    gfx_obj_align(g_obj_img_status, GFX_ALIGN_TOP_MID, -120, 18);

    g_obj_img_corner_badge = gfx_img_create(engine_handle);
    const auto corner_badge_bounds = TopRightOverlayBounds(
        width,
        height,
        kEmoteCornerBadgeSize,
        8);
    gfx_obj_align(g_obj_img_corner_badge, GFX_ALIGN_TOP_LEFT, corner_badge_bounds.x, corner_badge_bounds.y);
    gfx_obj_set_size(g_obj_img_corner_badge, corner_badge_bounds.width, corner_badge_bounds.height);
    gfx_img_set_src(g_obj_img_corner_badge, const_cast<gfx_image_dsc_t*>(&kEmoteCornerBadgeImage));

    SetUIDisplayMode(UIDisplayMode::SHOW_TIPS, display);
}

static void RegisterCallbacks(const esp_lcd_panel_io_handle_t panel_io, const gfx_handle_t engine_handle)
{
    if (!panel_io) {
        ESP_LOGE(TAG, "RegisterCallbacks: panel_io is nullptr");
        return;
    }

    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = EmoteEngine::OnFlushIoReady,
    };
    esp_lcd_panel_io_register_event_callbacks(panel_io, &cbs, engine_handle);
}

// ============================================================================
// EmoteEngine Class Implementation
// ============================================================================

EmoteEngine::EmoteEngine(const esp_lcd_panel_handle_t panel, const esp_lcd_panel_io_handle_t panel_io,
                         const int width, const int height, EmoteDisplay* const display)
{
    InitializeGraphics(panel, &engine_handle_, width, height);

    if (display) {
        gfx_emote_lock(engine_handle_);
        SetupUI(engine_handle_, display, width, height);
        gfx_emote_unlock(engine_handle_);
    }

    RegisterCallbacks(panel_io, engine_handle_);
}

EmoteEngine::~EmoteEngine()
{
    if (engine_handle_) {
        gfx_emote_deinit(engine_handle_);
        engine_handle_ = nullptr;
    }
}

void EmoteEngine::SetEyes(const std::string &emoji_name, const bool repeat, const int fps, EmoteDisplay* const display)
{
    if (!engine_handle_) {
        ESP_LOGE(TAG, "SetEyes: engine_handle_ is nullptr");
        return;
    }

    if (!display) {
        ESP_LOGE(TAG, "SetEyes: display is nullptr");
        return;
    }

    const AssetData emoji_data = display->GetEmojiData(emoji_name);
    if (emoji_data.data) {
        DisplayLockGuard lock(display);
        gfx_anim_set_src(g_obj_anim_eye, emoji_data.data, emoji_data.size);
        gfx_anim_set_segment(g_obj_anim_eye, 0, 0xFFFF, fps, repeat);
        gfx_obj_set_visible(g_obj_anim_eye, true);
        gfx_anim_start(g_obj_anim_eye);
    } else {
        ESP_LOGW(TAG, "SetEyes: No emoji data found for %s", emoji_name.c_str());
    }
}

void EmoteEngine::SetIcon(const std::string &icon_name, EmoteDisplay* const display)
{
    if (!engine_handle_) {
        ESP_LOGE(TAG, "SetIcon: engine_handle_ is nullptr");
        return;
    }

    if (!display) {
        ESP_LOGE(TAG, "SetIcon: display is nullptr");
        return;
    }

    const AssetData icon_data = display->GetIconData(icon_name);
    if (icon_data.data) {
        DisplayLockGuard lock(display);

        std::memcpy(&g_icon_img_dsc.header, icon_data.data, sizeof(gfx_image_header_t));
        g_icon_img_dsc.data = static_cast<const uint8_t*>(icon_data.data) + sizeof(gfx_image_header_t);
        g_icon_img_dsc.data_size = icon_data.size - sizeof(gfx_image_header_t);

        gfx_img_set_src(g_obj_img_status, &g_icon_img_dsc);
    } else {
        ESP_LOGW(TAG, "SetIcon: No icon data found for %s", icon_name.c_str());
    }
    g_current_icon_type = icon_name;
}

bool EmoteEngine::OnFlushIoReady(const esp_lcd_panel_io_handle_t panel_io,
                                 esp_lcd_panel_io_event_data_t* const edata,
                                 void* const user_ctx)
{
    gfx_handle_t handle = static_cast<gfx_handle_t>(user_ctx);
    if (handle) {
        gfx_emote_flush_ready(handle, true);
    }
    return false;
}

void EmoteEngine::OnFlush(const gfx_handle_t handle, const int x_start, const int y_start,
                          const int x_end, const int y_end, const void* const color_data)
{
    auto* const panel = static_cast<esp_lcd_panel_handle_t>(gfx_emote_get_user_data(handle));
    if (panel) {
        esp_lcd_panel_draw_bitmap(panel, x_start, y_start, x_end, y_end, color_data);
    }
}

// ============================================================================
// EmoteDisplay Class Implementation
// ============================================================================

EmoteDisplay::EmoteDisplay(const esp_lcd_panel_handle_t panel, const esp_lcd_panel_io_handle_t panel_io,
                           const int width, const int height)
{
    width_ = width;
    height_ = height;
    InitializeEngine(panel, panel_io, width, height);
    TryStartEmoteGenPreview();
}

EmoteDisplay::~EmoteDisplay()
{
    if (emote_gen_assets_) {
        mmap_assets_del(emote_gen_assets_);
        emote_gen_assets_ = nullptr;
    }
}

void EmoteDisplay::SetEmotion(const char* const emotion)
{
    if (!emotion) {
        ESP_LOGE(TAG, "SetEmotion: emotion is nullptr");
        return;
    }

    ESP_LOGI(TAG, "SetEmotion: %s", emotion);
    if (!engine_) {
        return;
    }

    if (emote_gen_runtime_active_) {
        if (PlayEmoteGenAssetByName(EmoteAssetFilenameForEmotion(emotion))) {
            return;
        }
        ESP_LOGW(TAG, "No emote_gen mapping found for emotion: %s", emotion);
    }

    const AssetData emoji_data = GetEmojiData(emotion);
    bool repeat = emoji_data.loop;
    int fps = emoji_data.fps > 0 ? emoji_data.fps : 20;

    if (std::strcmp(emotion, "idle") == 0 || std::strcmp(emotion, "neutral") == 0) {
        repeat = false;
    }

    DisplayLockGuard lock(this);
    engine_->SetEyes(emotion, repeat, fps, this);
}

void EmoteDisplay::SetChatMessage(const char* const role, const char* const content)
{
    if (!engine_) {
        return;
    }

    DisplayLockGuard lock(this);
    if (content && strlen(content) > 0) {
        gfx_label_set_text(g_obj_label_toast, content);
        SetUIDisplayMode(UIDisplayMode::SHOW_TIPS, this);
    }
}

void EmoteDisplay::SetStatus(const char* const status)
{
    if (!status) {
        ESP_LOGE(TAG, "SetStatus: status is nullptr");
        return;
    }

    if (!engine_) {
        return;
    }

    if (emote_gen_runtime_active_) {
        if (std::strcmp(status, Lang::Strings::LISTENING) == 0) {
            PlayEmoteGenAssetByName(EmoteAssetFilenameForEmotion("listening"));
        } else if (std::strcmp(status, Lang::Strings::STANDBY) == 0) {
            PlayEmoteGenAssetByName(EmoteAssetFilenameForEmotion("idle"));
        } else if (std::strcmp(status, Lang::Strings::SPEAKING) == 0) {
            PlayEmoteGenAssetByName(EmoteAssetFilenameForEmotion("speaking"));
        } else if (std::strcmp(status, Lang::Strings::ERROR) == 0) {
            PlayEmoteGenAssetByName(EmoteAssetFilenameForEmotion("error"));
        } else if (std::strcmp(status, Lang::Strings::CONNECTING) == 0) {
            PlayEmoteGenAssetByName(EmoteAssetFilenameForEmotion("connecting"));
        }

        DisplayLockGuard lock(this);
        SetUIDisplayMode(UIDisplayMode::SHOW_TIPS, this);
        gfx_label_set_text(g_obj_label_toast, status);
        return;
    }

    DisplayLockGuard lock(this);

    if (std::strcmp(status, Lang::Strings::LISTENING) == 0) {
        SetUIDisplayMode(UIDisplayMode::SHOW_LISTENING, this);
        engine_->SetEyes("happy", true, 20, this);
        engine_->SetIcon(ICON_MIC, this);
    } else if (std::strcmp(status, Lang::Strings::STANDBY) == 0) {
        SetUIDisplayMode(UIDisplayMode::SHOW_TIME, this);
        engine_->SetIcon(ICON_BATTERY, this);
    } else if (std::strcmp(status, Lang::Strings::SPEAKING) == 0) {
        SetUIDisplayMode(UIDisplayMode::SHOW_TIPS, this);
        engine_->SetIcon(ICON_SPEAKER_ZZZ, this);
    } else if (std::strcmp(status, Lang::Strings::ERROR) == 0) {
        SetUIDisplayMode(UIDisplayMode::SHOW_TIPS, this);
        engine_->SetIcon(ICON_WIFI_FAILED, this);
    }

    if (std::strcmp(status, Lang::Strings::CONNECTING) != 0) {
        gfx_label_set_text(g_obj_label_toast, status);
    }
}

void EmoteDisplay::ShowNotification(const char* notification, int duration_ms)
{
    if (!notification || !engine_) {
        return;
    }
    ESP_LOGI(TAG, "ShowNotification: %s", notification);

    DisplayLockGuard lock(this);
    gfx_label_set_text(g_obj_label_toast, notification);
    SetUIDisplayMode(UIDisplayMode::SHOW_TIPS, this);
}

void EmoteDisplay::UpdateStatusBar(bool update_all)
{
    if (!engine_) {
        return;
    }

    // Only display time when battery icon is shown
    DisplayLockGuard lock(this);
    if (g_current_icon_type == ICON_BATTERY) {
        time_t now;
        struct tm timeinfo;
        time(&now);

        setenv("TZ", "GMT+0", 1);
        tzset();
        localtime_r(&now, &timeinfo);

        char time_str[6];
        snprintf(time_str, sizeof(time_str), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

        DisplayLockGuard lock(this);
        gfx_label_set_text(g_obj_label_clock, time_str);
        SetUIDisplayMode(UIDisplayMode::SHOW_TIME, this);
    }
}

void EmoteDisplay::SetPowerSaveMode(bool on)
{
    if (!engine_) {
        return;
    }

    DisplayLockGuard lock(this);
    ESP_LOGI(TAG, "SetPowerSaveMode: %s", on ? "ON" : "OFF");
    if (on) {
        gfx_anim_stop(g_obj_anim_eye);
    } else {
        gfx_anim_start(g_obj_anim_eye);
    }
}

void EmoteDisplay::SetPreviewImage(const void* image)
{
    if (image) {
        ESP_LOGI(TAG, "SetPreviewImage: Preview image not supported, using default icon");
        if (engine_) {
        }
    }
}

void EmoteDisplay::SetTheme(Theme* const theme)
{
    ESP_LOGI(TAG, "SetTheme: %p", theme);

}
void EmoteDisplay::AddEmojiData(const std::string &name, const void* const data, const size_t size,
                                uint8_t fps, bool loop, bool lack)
{
    emoji_data_map_[name] = AssetData(data, size, fps, loop, lack);
    ESP_LOGD(TAG, "Added emoji data: %s, size: %d, fps: %d, loop: %s, lack: %s",
             name.c_str(), size, fps, loop ? "true" : "false", lack ? "true" : "false");

    DisplayLockGuard lock(this);
    if (name == "happy") {
        engine_->SetEyes("happy", loop, fps > 0 ? fps : 20, this);
    }
}

void EmoteDisplay::AddIconData(const std::string &name, const void* const data, const size_t size)
{
    icon_data_map_[name] = AssetData(data, size);
    ESP_LOGD(TAG, "Added icon data: %s, size: %d", name.c_str(), size);

    DisplayLockGuard lock(this);
    if (name == ICON_WIFI_FAILED) {
        SetUIDisplayMode(UIDisplayMode::SHOW_TIPS, this);
        engine_->SetIcon(ICON_WIFI_FAILED, this);
    }
}

void EmoteDisplay::AddLayoutData(const std::string &name, const std::string &align_str,
                                 const int x, const int y, const int width, const int height)
{
    const char align_enum = StringToGfxAlign(align_str);
    ESP_LOGI(TAG, "layout: %-12s | %-20s(%d) | %4d, %4d | %4dx%-4d",
             name.c_str(), align_str.c_str(), align_enum, x, y, width, height);

    struct UIElement {
        gfx_obj_t* obj;
        const char* name;
    };

    const UIElement elements[] = {
        {g_obj_anim_eye,     UI_ELEMENT_EYE_ANIM},
        {g_obj_label_toast,  UI_ELEMENT_TOAST_LABEL},
        {g_obj_label_clock,  UI_ELEMENT_CLOCK_LABEL},
        {g_obj_anim_listen,  UI_ELEMENT_LISTEN_ANIM},
        {g_obj_img_status,   UI_ELEMENT_STATUS_ICON},
        {g_obj_img_corner_badge, UI_ELEMENT_CORNER_BADGE}
    };

    DisplayLockGuard lock(this);
    for (const auto &element : elements) {
        if (name == element.name && element.obj) {
            gfx_obj_align(element.obj, align_enum, x, y);
            if (width > 0 && height > 0) {
                gfx_obj_set_size(element.obj, width, height);
            }
            return;
        }
    }

    ESP_LOGW(TAG, "AddLayoutData: UI element '%s' not found", name.c_str());
}

void EmoteDisplay::AddTextFont(std::shared_ptr<LvglFont> text_font)
{
    if (!text_font) {
        ESP_LOGW(TAG, "AddTextFont: text_font is nullptr");
        return;
    }

    text_font_ = text_font;
    ESP_LOGD(TAG, "AddTextFont: Text font added successfully");

    DisplayLockGuard lock(this);
    if (g_obj_label_toast && text_font_) {
        gfx_label_set_font(g_obj_label_toast, const_cast<void*>(static_cast<const void*>(text_font_->font())));
    }
    if (g_obj_label_clock && text_font_) {
        gfx_label_set_font(g_obj_label_clock, const_cast<void*>(static_cast<const void*>(text_font_->font())));
    }
}

AssetData EmoteDisplay::GetEmojiData(const std::string &name) const
{
    const auto it = emoji_data_map_.find(name);
    if (it != emoji_data_map_.cend()) {
        return it->second;
    }
    return AssetData();
}

AssetData EmoteDisplay::GetIconData(const std::string &name) const
{
    const auto it = icon_data_map_.find(name);
    if (it != icon_data_map_.cend()) {
        return it->second;
    }
    return AssetData();
}

EmoteEngine* EmoteDisplay::GetEngine() const
{
    return engine_.get();
}

void* EmoteDisplay::GetEngineHandle() const
{
    return engine_ ? engine_->GetEngineHandle() : nullptr;
}

void EmoteDisplay::InitializeEngine(const esp_lcd_panel_handle_t panel, const esp_lcd_panel_io_handle_t panel_io,
                                    const int width, const int height)
{
    engine_ = std::make_unique<EmoteEngine>(panel, panel_io, width, height, this);
}

void EmoteDisplay::TryStartEmoteGenPreview()
{
#if CONFIG_GRIND_BUDDY_EMOTE_GEN_PREVIEW
    if (!engine_) {
        return;
    }

    mmap_assets_config_t asset_config = {};
    asset_config.partition_label = "emote_gen";
    asset_config.flags.mmap_enable = 1;
    asset_config.flags.full_check = 1;

    esp_err_t err = mmap_assets_new(&asset_config, &emote_gen_assets_);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to mount emote_gen partition: %s", esp_err_to_name(err));
        return;
    }

    const int stored_files = mmap_assets_get_stored_files(emote_gen_assets_);
    for (int i = 0; i < stored_files; ++i) {
        const char* name = mmap_assets_get_name(emote_gen_assets_, i);
        if (IsEafAssetFilename(name)) {
            emote_gen_asset_index_by_name_[name] = i;
        }
    }

    if (emote_gen_asset_index_by_name_.empty()) {
        const std::string filename = EmoteAssetFilenameFromName(CONFIG_GRIND_BUDDY_EMOTE_GEN_PREVIEW_NAME);
        for (int i = 0; i < stored_files; ++i) {
            const char* name = mmap_assets_get_name(emote_gen_assets_, i);
            if (name && filename == name) {
                emote_gen_asset_index_by_name_[name] = i;
                break;
            }
        }
    }

    if (emote_gen_asset_index_by_name_.empty()) {
        ESP_LOGW(TAG, "No emote_gen EAF assets found");
        return;
    }

    ESP_LOGI(TAG, "emote_gen emotion runtime loaded %u animations",
        static_cast<unsigned>(emote_gen_asset_index_by_name_.size()));
    emote_gen_runtime_active_ = true;
    PlayEmoteGenAssetByName(EmoteAssetFilenameForEmotion("neutral"));
#endif
}

void EmoteDisplay::PlayEmoteGenAssetByIndex(int asset_index)
{
#if CONFIG_GRIND_BUDDY_EMOTE_GEN_PREVIEW
    if (!emote_gen_assets_ || asset_index < 0) {
        return;
    }

    const char* filename = mmap_assets_get_name(emote_gen_assets_, asset_index);
    const uint8_t* data = mmap_assets_get_mem(emote_gen_assets_, asset_index);
    const int size = mmap_assets_get_size(emote_gen_assets_, asset_index);
    if (!data || size <= 0) {
        ESP_LOGW(TAG, "emote_gen asset is empty: %s", filename ? filename : "(unknown)");
        return;
    }

    ESP_LOGI(TAG, "Playing emote_gen emotion: %s (%d bytes)", filename ? filename : "(unknown)", size);
    PlayStandaloneEmote(data, static_cast<size_t>(size), 25);
#endif
}

bool EmoteDisplay::PlayEmoteGenAssetByName(const std::string& filename)
{
    const auto it = emote_gen_asset_index_by_name_.find(filename);
    if (it == emote_gen_asset_index_by_name_.end()) {
        return false;
    }

    PlayEmoteGenAssetByIndex(it->second);
    return true;
}

void EmoteDisplay::PlayStandaloneEmote(const void* data, size_t size, uint8_t fps)
{
    if (!engine_ || !data || size == 0) {
        return;
    }

    DisplayLockGuard lock(this);
    gfx_obj_set_visible(g_obj_anim_listen, false);
    gfx_obj_set_visible(g_obj_label_clock, false);
    gfx_obj_set_visible(g_obj_img_status, false);

    const auto geometry = ReadEafCanvasGeometry(data, size);
    int anim_width = geometry.width > 0 ? geometry.width : width_;
    int anim_height = geometry.height > 0 ? geometry.height : height_;
    if (anim_width > width_) {
        anim_width = width_;
    }
    if (anim_height > height_) {
        anim_height = height_;
    }
    const auto offset = CenteredOffset(width_, height_, anim_width, anim_height);

    gfx_anim_set_auto_mirror(g_obj_anim_eye, false);
    gfx_obj_align(g_obj_anim_eye, GFX_ALIGN_TOP_LEFT, offset.x, offset.y);
    gfx_obj_set_size(g_obj_anim_eye, anim_width, anim_height);
    gfx_anim_set_src(g_obj_anim_eye, data, size);
    gfx_anim_set_segment(g_obj_anim_eye, 0, 0xFFFFFFFF, fps, true);
    gfx_obj_set_visible(g_obj_anim_eye, true);
    gfx_anim_start(g_obj_anim_eye);
}

bool EmoteDisplay::Lock(const int timeout_ms)
{
    if (engine_ && engine_->GetEngineHandle()) {
        gfx_emote_lock(engine_->GetEngineHandle());
        return true;
    }
    return false;
}

void EmoteDisplay::Unlock()
{
    if (engine_ && engine_->GetEngineHandle()) {
        gfx_emote_unlock(engine_->GetEngineHandle());
    }
}

} // namespace emote
