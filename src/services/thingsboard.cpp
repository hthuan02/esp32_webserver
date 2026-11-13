#include "services/thingsboard.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <MQTT.h>
#include <WiFi.h>
#include <esp_system.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "drivers/ds3231.h"
#if __has_include("services/thingsboard_secrets.h")
#include "services/thingsboard_secrets.h"
#else
namespace ThingsBoardSecrets {
constexpr char kHost[] = "mqtt.thingsboard.cloud";
constexpr uint16_t kPort = 1883;
constexpr char kAccessToken[] = "";
} // namespace ThingsBoardSecrets
#endif

namespace ThingsBoard {
namespace {
constexpr uint32_t kSampleIntervalMs = 10000;
constexpr uint32_t kReconnectIntervalMs = 5000;
constexpr uint32_t kPublishRetryIntervalMs = 500;
constexpr time_t kMinimumValidEpoch = 1704067200; // 2024-01-01 UTC
constexpr size_t kRecordLimit = 7;
constexpr size_t kBacklogCapacity = 256; // About 42 minutes at a 10-second sample interval.
constexpr char kTelemetryTopic[] = "v1/devices/me/telemetry";
constexpr char kQueuePath[] = "/thingsboard_queue.csv";
constexpr char kQueueCursorPath[] = "/thingsboard_queue.ptr";
constexpr char kQueueCursorTempPath[] = "/thingsboard_queue.ptr.tmp";

struct DemoTelemetry
{
    float sensor1 = 0.0f;
    float sensor2 = 0.0f;
    float sensor3 = 0.0f;
    float sensor4 = 0.0f;
};

struct DemoRecord
{
    char timestamp[20] = {};
    DemoTelemetry values;
};

struct TelemetrySample
{
    uint64_t timestampMs = 0;
    DemoTelemetry values;
    uint32_t fileEndOffset = 0;
};

WebServer* telemetryServer = nullptr;
AuthenticationCheck authenticationCheck = nullptr;
WiFiClient mqttNetwork;
MQTTClient mqttClient(256);
DemoTelemetry telemetry;
DemoRecord records[kRecordLimit];
TelemetrySample pendingSamples[kBacklogCapacity];
size_t recordCount = 0;
size_t queueHead = 0;
size_t queueCount = 0;
uint32_t lastCycleAt = 0;
uint32_t lastConnectAttemptAt = 0;
uint32_t lastPublishAttemptAt = 0;
uint32_t droppedSamples = 0;
bool persistentQueueAvailable = true;
String sendStatus = "Dang khoi tao MQTT ThingsBoard.";
String lastSentAt = "Chua gui";

bool isAuthorized()
{
    return authenticationCheck != nullptr && authenticationCheck();
}

float randomFloat(float minimum, float maximum)
{
    const float unit = static_cast<float>(esp_random()) / 4294967295.0f;
    return minimum + unit * (maximum - minimum);
}

void generateDemoTelemetry()
{
    telemetry.sensor1 = randomFloat(0.0f, 100.0f);
    telemetry.sensor2 = randomFloat(0.0f, 100.0f);
    telemetry.sensor3 = randomFloat(0.0f, 100.0f);
    telemetry.sensor4 = randomFloat(0.0f, 100.0f);
}

void appendDemoRecord(time_t epoch)
{
    if (recordCount == kRecordLimit)
    {
        recordCount = 0;
    }

    struct tm localTime;
    if (localtime_r(&epoch, &localTime) == nullptr)
    {
        return;
    }

    DemoRecord& record = records[recordCount];
    strftime(record.timestamp, sizeof(record.timestamp), "%d/%m/%Y-%H:%M:%S", &localTime);
    record.values = telemetry;
    ++recordCount;
}

bool parseQueueLine(const String& line, TelemetrySample& sample)
{
    unsigned long long timestampMs = 0;
    float sensor1 = 0.0f;
    float sensor2 = 0.0f;
    float sensor3 = 0.0f;
    float sensor4 = 0.0f;
    const int parsed = sscanf(line.c_str(), "%llu,%f,%f,%f,%f",
                              &timestampMs, &sensor1, &sensor2, &sensor3, &sensor4);
    if (parsed != 5)
    {
        return false;
    }

    sample.timestampMs = static_cast<uint64_t>(timestampMs);
    sample.values = {sensor1, sensor2, sensor3, sensor4};
    return sample.timestampMs >= static_cast<uint64_t>(kMinimumValidEpoch) * 1000ULL;
}

void pushLoadedSample(const TelemetrySample& sample)
{
    if (queueCount >= kBacklogCapacity)
    {
        return;
    }

    const size_t tail = (queueHead + queueCount) % kBacklogCapacity;
    pendingSamples[tail] = sample;
    ++queueCount;
}

void loadPendingQueue()
{
    File queueFile = LittleFS.open(kQueuePath, "r");
    if (!queueFile)
    {
        // No backlog file is normal on a clean boot. Check filesystem availability when queuing.
        return;
    }

    uint32_t acknowledgedOffset = 0;
    File cursorFile = LittleFS.open(kQueueCursorPath, "r");
    if (cursorFile)
    {
        if (cursorFile.size() == sizeof(acknowledgedOffset))
        {
            cursorFile.read(reinterpret_cast<uint8_t*>(&acknowledgedOffset), sizeof(acknowledgedOffset));
        }
        cursorFile.close();
    }

    const uint32_t queueFileSize = static_cast<uint32_t>(queueFile.size());
    if (acknowledgedOffset > queueFileSize)
    {
        acknowledgedOffset = 0;
    }
    queueFile.seek(acknowledgedOffset, SeekSet);

    while (queueFile.available() && queueCount < kBacklogCapacity)
    {
        String line = queueFile.readStringUntil('\n');
        TelemetrySample sample;
        if (parseQueueLine(line, sample))
        {
            sample.fileEndOffset = static_cast<uint32_t>(queueFile.position());
            pushLoadedSample(sample);
        }
        else
        {
            Serial.println("ThingsBoard queue: bo qua dong backlog khong hop le.");
        }
    }
    queueFile.close();

    if (queueCount == 0 && acknowledgedOffset >= queueFileSize)
    {
        LittleFS.remove(kQueuePath);
        LittleFS.remove(kQueueCursorPath);
    }
    Serial.printf("ThingsBoard: da nap %u ban ghi cho gui bu.\n", static_cast<unsigned>(queueCount));
}

bool saveQueueCursor(uint32_t offset)
{
    File tempFile = LittleFS.open(kQueueCursorTempPath, "w");
    if (!tempFile)
    {
        return false;
    }

    const size_t written = tempFile.write(reinterpret_cast<const uint8_t*>(&offset), sizeof(offset));
    tempFile.flush();
    tempFile.close();
    if (written != sizeof(offset))
    {
        LittleFS.remove(kQueueCursorTempPath);
        return false;
    }

    LittleFS.remove(kQueueCursorPath);
    return LittleFS.rename(kQueueCursorTempPath, kQueueCursorPath);
}

bool persistSample(TelemetrySample& sample)
{
    if (!persistentQueueAvailable)
    {
        return true; // Keep buffering in RAM if LittleFS is unavailable.
    }

    File queueFile = LittleFS.open(kQueuePath, "a");
    if (!queueFile)
    {
        persistentQueueAvailable = false;
        Serial.println("ThingsBoard queue: LittleFS khong ghi duoc; backlog chi duoc giu trong RAM.");
        return true;
    }

    char line[112];
    const int lineLength = snprintf(line, sizeof(line), "%llu,%.2f,%.2f,%.2f,%.2f\n",
                                    static_cast<unsigned long long>(sample.timestampMs),
                                    sample.values.sensor1, sample.values.sensor2,
                                    sample.values.sensor3, sample.values.sensor4);
    const size_t written = lineLength > 0 ? queueFile.write(
        reinterpret_cast<const uint8_t*>(line), static_cast<size_t>(lineLength)) : 0;
    queueFile.flush();
    sample.fileEndOffset = static_cast<uint32_t>(queueFile.position());
    queueFile.close();

    if (lineLength <= 0 || written != static_cast<size_t>(lineLength))
    {
        persistentQueueAvailable = false;
        Serial.println("ThingsBoard queue: ghi LittleFS that bai; backlog chi duoc giu trong RAM.");
        return true;
    }
    return true;
}

bool enqueueSample(const TelemetrySample& sample)
{
    if (queueCount >= kBacklogCapacity)
    {
        ++droppedSamples;
        sendStatus = "Hang doi gui bu da day; bo mau moi. Hay ket noi lai ThingsBoard.";
        Serial.println("ThingsBoard queue full; dropped newest telemetry sample.");
        return false;
    }

    TelemetrySample queued = sample;
    if (!persistSample(queued))
    {
        return false;
    }

    const size_t tail = (queueHead + queueCount) % kBacklogCapacity;
    pendingSamples[tail] = queued;
    ++queueCount;
    return true;
}

TelemetrySample* queueFront()
{
    return queueCount == 0 ? nullptr : &pendingSamples[queueHead];
}

void removeAcknowledgedSample()
{
    TelemetrySample* sample = queueFront();
    if (sample == nullptr)
    {
        return;
    }

    const uint32_t nextOffset = sample->fileEndOffset;
    queueHead = (queueHead + 1) % kBacklogCapacity;
    --queueCount;

    if (queueCount == 0)
    {
        // A QoS 1 PUBACK was received, so it is safe to clear the drained file.
        LittleFS.remove(kQueuePath);
        LittleFS.remove(kQueueCursorPath);
        LittleFS.remove(kQueueCursorTempPath);
        return;
    }

    if (nextOffset > 0 && !saveQueueCursor(nextOffset))
    {
        Serial.println("ThingsBoard queue: khong luu duoc con tro; co the gui lap lai sau khi khoi dong.");
    }
}

String makePayload(const TelemetrySample& sample)
{
    char payload[192];
    snprintf(payload, sizeof(payload),
             "{\"ts\":%llu,\"values\":{\"sensor1\":%.2f,\"sensor2\":%.2f,\"sensor3\":%.2f,\"sensor4\":%.2f}}",
             static_cast<unsigned long long>(sample.timestampMs),
             sample.values.sensor1, sample.values.sensor2,
             sample.values.sensor3, sample.values.sensor4);
    return String(payload);
}

void updateLastSentTime(uint64_t timestampMs)
{
    const time_t epoch = static_cast<time_t>(timestampMs / 1000ULL);
    struct tm localTime;
    if (localtime_r(&epoch, &localTime) != nullptr)
    {
        char timeText[24];
        strftime(timeText, sizeof(timeText), "%H:%M:%S %d/%m/%Y", &localTime);
        lastSentAt = timeText;
    }
}

bool publishSample(const TelemetrySample& sample)
{
    if (!mqttClient.connected())
    {
        return false;
    }

    const String payload = makePayload(sample);
    // QoS 1 returns success only after the broker acknowledges the publication.
    return mqttClient.publish(kTelemetryTopic, payload, false, 1);
}

void createMeasurement()
{
    time_t epoch = 0;
    if (!Ds3231::readUnixTime(epoch))
    {
        sendStatus = "RTC DS3231 chua co gio hop le; chua tao ban ghi telemetry.";
        Serial.println("ThingsBoard: khong tao mau vi DS3231 chua co thoi gian hop le.");
        return;
    }

    appendDemoRecord(epoch);
    TelemetrySample sample;
    sample.timestampMs = static_cast<uint64_t>(epoch) * 1000ULL;
    sample.values = telemetry;

    if (queueCount == 0 && publishSample(sample))
    {
        updateLastSentTime(sample.timestampMs);
        // sendStatus = "MQTT QoS 1 da duoc ThingsBoard xac nhan.";
        Serial.printf("ThingsBoard PUBACK: sensor1=%.2f sensor2=%.2f sensor3=%.2f sensor4=%.2f\n",
                      telemetry.sensor1, telemetry.sensor2, telemetry.sensor3, telemetry.sensor4);
        return;
    }

    if (enqueueSample(sample))
    {
        sendStatus = mqttClient.connected()
                         ? "Dang co backlog; mau moi da xep hang cho gui theo thu tu."
                         : "Mat ket noi MQTT; mau da luu de gui bu.";
    }
}

void connectToBrokerIfDue(uint32_t now)
{
    if (WiFi.status() != WL_CONNECTED || mqttClient.connected() ||
        now - lastConnectAttemptAt < kReconnectIntervalMs)
    {
        return;
    }

    lastConnectAttemptAt = now;
    String clientId = "esp32-" + WiFi.macAddress();
    clientId.replace(":", "");
    Serial.println("ThingsBoard MQTT: dang ket noi broker...");
    if (mqttClient.connect(clientId.c_str(), ThingsBoardSecrets::kAccessToken, ""))
    {
        Serial.println("ThingsBoard MQTT: da ket noi.");
        sendStatus = queueCount > 0 ? "MQTT da ket noi; dang gui bu backlog." : "MQTT da ket noi ThingsBoard.";
    }
    else
    {
        Serial.printf("ThingsBoard MQTT: ket noi that bai, ma loi %d.\n", mqttClient.returnCode());
        sendStatus = "Khong ket noi duoc MQTT ThingsBoard; ESP32 se thu lai.";
    }
}

void flushOnePendingSample(uint32_t now)
{
    if (queueCount == 0 || !mqttClient.connected() ||
        now - lastPublishAttemptAt < kPublishRetryIntervalMs)
    {
        return;
    }

    lastPublishAttemptAt = now;
    TelemetrySample* sample = queueFront();
    if (sample == nullptr)
    {
        return;
    }

    if (!publishSample(*sample))
    {
        sendStatus = "Chua nhan PUBACK; giu backlog va se gui lai.";
        Serial.println("ThingsBoard MQTT: chua co PUBACK; giu ban ghi trong hang doi.");
        return;
    }

    const uint64_t sentTimestamp = sample->timestampMs;
    updateLastSentTime(sentTimestamp);
    removeAcknowledgedSample();
    sendStatus = queueCount == 0
                     ? "ThingsBoard đã kết nối."
                     : "ThingsBoard đã xác nhận một bản ghi. Tiếp tục gửi bù.";
    Serial.printf("ThingsBoard MQTT: đã nhận PUBACK, backlog còn %u.\n",
                  static_cast<unsigned>(queueCount));
}

void handleTelemetryRequest()
{
    if (!isAuthorized())
    {
        telemetryServer->send(401, "application/json; charset=utf-8",
                              "{\"available\":false,\"message\":\"Vui long dang nhap.\"}");
        return;
    }

    String json;
    json.reserve(1500);
    json = "{\"available\":true,\"configured\":";
    json += (ThingsBoardSecrets::kAccessToken[0] != '\0') ? "true" : "false";
    json += ",\"mqttConnected\":";
    json += mqttClient.connected() ? "true" : "false";
    json += ",\"pending\":" + String(queueCount);
    json += ",\"dropped\":" + String(droppedSamples);
    json += ",\"sensor1\":" + String(telemetry.sensor1, 2);
    json += ",\"sensor2\":" + String(telemetry.sensor2, 2);
    json += ",\"sensor3\":" + String(telemetry.sensor3, 2);
    json += ",\"sensor4\":" + String(telemetry.sensor4, 2);
    json += ",\"records\":[";

    for (size_t i = 0; i < recordCount; ++i)
    {
        if (i > 0)
        {
            json += ',';
        }
        const DemoRecord& record = records[i];
        json += "{\"timestamp\":\"";
        json += record.timestamp;
        json += "\",\"sensor1\":" + String(record.values.sensor1, 2);
        json += ",\"sensor2\":" + String(record.values.sensor2, 2);
        json += ",\"sensor3\":" + String(record.values.sensor3, 2);
        json += ",\"sensor4\":" + String(record.values.sensor4, 2);
        json += '}';
    }

    json += "],\"status\":\"";
    json += sendStatus;
    json += "\",\"lastSent\":\"";
    json += lastSentAt;
    json += "\"}";
    telemetryServer->send(200, "application/json; charset=utf-8", json);
}
} // namespace

void begin()
{
    mqttClient.begin(ThingsBoardSecrets::kHost, ThingsBoardSecrets::kPort, mqttNetwork);
    mqttClient.setOptions(60, true, 3000);
    loadPendingQueue();

    generateDemoTelemetry();
    createMeasurement();
    lastCycleAt = millis();

    Serial.println("ThingsBoard MQTT: lay mau moi moi 10 giay; backlog duoc luu trong LittleFS.");
    if (ThingsBoardSecrets::kAccessToken[0] == '\0')
    {
        Serial.println("ThingsBoard MQTT: chua co device access token.");
    }
}

void loop()
{
    if (mqttClient.connected())
    {
        mqttClient.loop();
    }

    const uint32_t now = millis();
    connectToBrokerIfDue(now);

    if (now - lastCycleAt >= kSampleIntervalMs)
    {
        lastCycleAt = now;
        generateDemoTelemetry();
        createMeasurement();
    }

    flushOnePendingSample(millis());
}

void registerRoutes(WebServer& server, AuthenticationCheck isAuthenticated)
{
    telemetryServer = &server;
    authenticationCheck = isAuthenticated;
    server.on("/api/demo", HTTP_GET, handleTelemetryRequest);
}

} // namespace ThingsBoard
