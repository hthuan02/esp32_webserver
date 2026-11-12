#include "services/rtc.h"

#include <Arduino.h>
#include <time.h>

namespace RtcManager {
namespace {
constexpr char kTimezone[] = "ICT-7";
constexpr time_t kMinimumValidEpoch = 1704067200; // 2024-01-01 00:00:00 UTC
WebServer* rtcServer = nullptr;
AuthenticationCheck authenticationCheck = nullptr;

bool isAuthorized()
{
    return authenticationCheck != nullptr && authenticationCheck();
}

bool hasValidSystemTime()
{
    return time(nullptr) >= kMinimumValidEpoch;
}

void handleTimeRequest()
{
    if (!isAuthorized())
    {
        rtcServer->send(401, "application/json; charset=utf-8",
                        "{\"available\":false,\"message\":\"Người dùng xác thực.\"}");
        return;
    }

    if (!hasValidSystemTime())
    {
        rtcServer->send(503, "application/json; charset=utf-8",
                        "{\"available\":false,\"message\":\"ESP32 đang chờ bật đồng bộ thời gian qua Wifi/NTP.\"}");
        return;
    }

    time_t now = time(nullptr);
    struct tm localTime;
    if (localtime_r(&now, &localTime) == nullptr)
    {
        rtcServer->send(503, "application/json; charset=utf-8",
                        "{\"available\":false,\"message\":\"Khong doc duoc dong ho he thong ESP32.\"}");
        return;
    }

    char timeText[9];
    char dateText[11];
    strftime(timeText, sizeof(timeText), "%H:%M:%S", &localTime);
    strftime(dateText, sizeof(dateText), "%d/%m/%Y", &localTime);

    String json = "{\"available\":true,\"time\":\"";
    json += timeText;
    json += "\",\"date\":\"";
    json += dateText;
    json += "\"}";
    rtcServer->send(200, "application/json; charset=utf-8", json);
}
} // namespace

void begin()
{
    // ICT is UTC+7. NTP runs in the background and updates the ESP32 system clock.
    configTzTime(kTimezone, "pool.ntp.org", "time.google.com", "time.nist.gov");
    Serial.println("Da bat dong bo thoi gian NTP cho RTC noi ESP32.");
}

void registerRoutes(WebServer& server, AuthenticationCheck isAuthenticated)
{
    rtcServer = &server;
    authenticationCheck = isAuthenticated;
    server.on("/api/rtc", HTTP_GET, handleTimeRequest);
}

} // namespace RtcManager
