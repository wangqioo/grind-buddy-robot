#ifndef DEFAULT_WIFI_CREDENTIALS_H
#define DEFAULT_WIFI_CREDENTIALS_H

struct DefaultWifiCredentials {
    const char* ssid;
    const char* password;
};

DefaultWifiCredentials GetDefaultWifiCredentials();
bool HasDefaultWifiCredentials();

#endif  // DEFAULT_WIFI_CREDENTIALS_H
