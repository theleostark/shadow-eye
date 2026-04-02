#pragma once

#include <WiFiType.h>
#include "wifi-types.h"

// RTC-persisted WiFi channel cache (defined in bl.cpp, survives deep sleep)
extern RTC_DATA_ATTR uint8_t cached_wifi_channel;
extern RTC_DATA_ATTR uint8_t cached_wifi_bssid[6];
extern RTC_DATA_ATTR bool cached_wifi_valid;

WifiConnectionResult initiateConnectionAndWaitForOutcome(const WifiCredentials credentials,
                                                          uint8_t channel = 0,
                                                          const uint8_t *bssid = nullptr);
wl_status_t waitForConnectResult(uint32_t timeout);
void disableWpa2Enterprise();