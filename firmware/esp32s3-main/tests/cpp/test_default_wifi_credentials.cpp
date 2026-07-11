#include <cassert>
#include <cstring>

#include "../../main/boards/common/default_wifi_credentials.h"

int main() {
    auto credentials = GetDefaultWifiCredentials();

    assert(HasDefaultWifiCredentials());
    assert(std::strcmp(credentials.ssid, "test-ssid") == 0);
    assert(std::strcmp(credentials.password, "test-password") == 0);

    return 0;
}
