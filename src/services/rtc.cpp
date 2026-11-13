#include "services/rtc.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <time.h>

#include "drivers/ds3231.h"

namespace RtcManager {
namespace {
constexpr char kTimezone[] = "ICT-7";
constexpr time_t kMinimumValidEpoch = 1704067200; // 2024-01-01 UTC

WebServer* rtcServer = nullptr;
AuthenticationCheck authenticationCheck = nullptr;
bool ntpStarted = false;
volatile bool ntpSyncPending = false;
uint32_t lastRtcLogAt = 0;
bool rtcInvalidWasLogged = false;

bool isAuthorized()
{
    return authenticationCheck != nullptr && authenticationCheck();
}

void onNtpTimeSynchronized(struct timeval*)
{
    ntpSyncPending = true;
}

void handleTimeRequest()
{
    if (!isAuthorized())
    {
        rtcServer->send(401, "application/json; charset=utf-8",
                        "{\"available\":false,\"message\":\"Vui long dang nhap.\"}");
        return;
    }

    time_t now = 0;
    if (!Ds3231::readUnixTime(now))
    {
        rtcServer->send(503, "application/json; charset=utf-8",
                        "{\"available\":false,\"message\":\"Khong doc duoc thoi gian hop le tu RTC DS3231.\"}");
        return;
    }

    struct tm localTime;
    if (localtime_r(&now, &localTime) == nullptr)
    {
        rtcServer->send(503, "application/json; charset=utf-8",
                        "{\"available\":false,\"message\":\"Khong the dinh dang thoi gian DS3231.\"}");
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
    setenv("TZ", kTimezone, 1);
    tzset();
    Ds3231::begin(); // Restore ESP32 time from the battery-backed RTC when valid.
}

void loop()
{
    const uint32_t now = millis();
    if (now - lastRtcLogAt >= 1000)
    {
        lastRtcLogAt = now;
        time_t rtcEpoch = 0;
        if (Ds3231::readUnixTime(rtcEpoch))
        {
            struct tm rtcLocalTime;
            if (localtime_r(&rtcEpoch, &rtcLocalTime) != nullptr)
            {
                Serial.printf("[RTC DS3231] %02d/%02d/%04d %02d:%02d:%02d\n",
                              rtcLocalTime.tm_mday, rtcLocalTime.tm_mon + 1,
                              rtcLocalTime.tm_year + 1900, rtcLocalTime.tm_hour,
                              rtcLocalTime.tm_min, rtcLocalTime.tm_sec);
            }
            rtcInvalidWasLogged = false;
        }
        else if (!rtcInvalidWasLogged)
        {
            Serial.println("[RTC DS3231] Chua co thoi gian hop le.");
            rtcInvalidWasLogged = true;
        }
    }

    if (!ntpStarted && WiFi.status() == WL_CONNECTED)
    {
        sntp_set_time_sync_notification_cb(onNtpTimeSynchronized);
        configTzTime(kTimezone, "pool.ntp.org", "time.google.com", "time.nist.gov");
        ntpStarted = true;
        Serial.println("NTP da bat; khi dong bo thanh cong se ghi xuong DS3231.");
    }

    if (ntpSyncPending)
    {
        ntpSyncPending = false;
        const time_t ntpEpoch = time(nullptr);
        if (ntpEpoch >= kMinimumValidEpoch && Ds3231::writeUnixTime(ntpEpoch))
        {
            Serial.println("NTP da cap nhat RTC DS3231.");
        }
        else
        {
            Serial.println("Khong ghi duoc thoi gian NTP xuong DS3231.");
        }
    }
}

void registerRoutes(WebServer& server, AuthenticationCheck isAuthenticated)
{
    rtcServer = &server;
    authenticationCheck = isAuthenticated;
    server.on("/api/rtc", HTTP_GET, handleTimeRequest);
}

} // namespace RtcManager
