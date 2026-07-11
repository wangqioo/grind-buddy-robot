#include "default_wifi_credentials.h"

#if __has_include("sdkconfig.h")
#include "sdkconfig.h"
#endif

#ifndef CONFIG_GRIND_BUDDY_DEFAULT_WIFI_SSID
#define CONFIG_GRIND_BUDDY_DEFAULT_WIFI_SSID ""
#endif

#ifndef CONFIG_GRIND_BUDDY_DEFAULT_WIFI_PASSWORD
#define CONFIG_GRIND_BUDDY_DEFAULT_WIFI_PASSWORD ""
#endif

DefaultWifiCredentials GetDefaultWifiCredentials() {
    return {
        CONFIG_GRIND_BUDDY_DEFAULT_WIFI_SSID,
        CONFIG_GRIND_BUDDY_DEFAULT_WIFI_PASSWORD,
    };
}

bool HasDefaultWifiCredentials() {
    return CONFIG_GRIND_BUDDY_DEFAULT_WIFI_SSID[0] != '\0';
}
