#include "services/things_board.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <esp_system.h>
#include <time.h>

namespace ThingsBoard {
namespace {
constexpr uint32_t kSendIntervalMs = 10000;
constexpr time_t kMinimumValidEpoch = 1704067200; // 2024-01-01 UTC

// Điền URL máy chủ và Access Token thiết bị khi đã tạo ThingsBoard.
// Ví dụ URL nội bộ: "http://192.168.1.10:8080"
constexpr char kServerUrl[] = "";
constexpr char kDeviceToken[] = "";

struct DemoTelemetry
{
    float var1 = 0.0f;
    float var2 = 0.0f;
    float var3 = 0.0f;
    float var4 = 0.0f;
};

WebServer* telemetryServer = nullptr;
AuthenticationCheck authenticationCheck = nullptr;
DemoTelemetry telemetry;
uint32_t lastCycleAt = 0;
String sendStatus = "Chua cau hinh ThingsBoard (URL va Access Token).";
String lastSentAt = "Chua gui";

bool isAuthorized()
{
    return authenticationCheck != nullptr && authenticationCheck();
}

bool hasValidSystemTime()
{
    return time(nullptr) >= kMinimumValidEpoch;
}

float randomFloat(float minimum, float maximum)
{
    const float unit = static_cast<float>(esp_random()) / 4294967295.0f;
    return minimum + unit * (maximum - minimum);
}

void generateDemoTelemetry()
{
    telemetry.var1 = randomFloat(0.0f, 100.0f);
    telemetry.var2 = randomFloat(0.0f, 100.0f);
    telemetry.var3 = randomFloat(0.0f, 100.0f);
    telemetry.var4 = randomFloat(0.0f, 100.0f);
}

String makePayload()
{
    char payload[128];
    snprintf(payload, sizeof(payload),
             "{\"var1\":%.2f,\"var2\":%.2f,\"var3\":%.2f,\"var4\":%.2f}",
             telemetry.var1, telemetry.var2, telemetry.var3, telemetry.var4);
    return String(payload);
}

void updateLastSentTime()
{
    const time_t now = time(nullptr);
    struct tm localTime;
    if (localtime_r(&now, &localTime) != nullptr)
    {
        char timeText[20];
        strftime(timeText, sizeof(timeText), "%H:%M:%S %d/%m/%Y", &localTime);
        lastSentAt = timeText;
    }
}

void sendTelemetry()
{
    if (kServerUrl[0] == '\0' || kDeviceToken[0] == '\0')
    {
        sendStatus = "Chưa cấu hình ThingsBoard.";
        return;
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        sendStatus = "Mất kết nối wifi, dữ liệu chưa được gửi.";
        return;
    }

    if (!hasValidSystemTime())
    {
        sendStatus = "Đang chờ đồng bộ thời gian";
        return;
    }

    String url = kServerUrl;
    while (url.endsWith("/"))
    {
        url.remove(url.length() - 1);
    }
    url += "/api/v1/";
    url += kDeviceToken;
    url += "/telemetry";

    HTTPClient http;
    http.setTimeout(4000);
    if (!http.begin(url))
    {
        sendStatus = "Không khởi tạo được kết nối HTTP.";
        return;
    }

    http.addHeader("Content-Type", "application/json");
    const int responseCode = http.POST(makePayload());
    if (responseCode >= 200 && responseCode < 300)
    {
        sendStatus = "Gửi ThingsBoard thành công (HTTP " + String(responseCode) + ").";
        updateLastSentTime();
        Serial.printf("ThingsBoard: var1=%.2f var2=%.2f var3=%.2f var4=%.2f\n",
                      telemetry.var1, telemetry.var2, telemetry.var3, telemetry.var4);
    }
    else
    {
        sendStatus = "Gui that bai (HTTP " + String(responseCode) + ").";
        Serial.printf("ThingsBoard: gui that bai, HTTP %d\n", responseCode);
    }
    http.end();
}

void handleTelemetryRequest()
{
    if (!isAuthorized())
    {
        telemetryServer->send(401, "application/json; charset=utf-8",
                              "{\"available\":false,\"message\":\"Vui long dang nhap.\"}");
        return;
    }

    char json[384];
    snprintf(json, sizeof(json),
             "{\"available\":true,\"configured\":%s,\"var1\":%.2f,\"var2\":%.2f,\"var3\":%.2f,\"var4\":%.2f,\"status\":\"%s\",\"lastSent\":\"%s\"}",
             (kServerUrl[0] != '\0' && kDeviceToken[0] != '\0') ? "true" : "false",
             telemetry.var1, telemetry.var2, telemetry.var3, telemetry.var4,
             sendStatus.c_str(), lastSentAt.c_str());
    telemetryServer->send(200, "application/json; charset=utf-8", json);
}
} // namespace

void begin()
{
    generateDemoTelemetry();
    lastCycleAt = millis();
    Serial.println("ThingsBoard demo: Đã tạo ngẫu nhiên 4 giá trị mỗi 10s.");
    if (kServerUrl[0] == '\0' || kDeviceToken[0] == '\0')
    {
        Serial.println("ThingsBoard demo: Chưa cấu hình URL/Access Token.");
    }
}

void loop()
{
    const uint32_t now = millis();
    if (now - lastCycleAt < kSendIntervalMs)
    {
        return;
    }

    lastCycleAt = now;
    generateDemoTelemetry();
    sendTelemetry();
}

void registerRoutes(WebServer& server, AuthenticationCheck isAuthenticated)
{
    telemetryServer = &server;
    authenticationCheck = isAuthenticated;
    server.on("/api/demo", HTTP_GET, handleTelemetryRequest);
}

} // namespace ThingsBoard
