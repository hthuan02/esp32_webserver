#include "services/wifi.h"

#include <Preferences.h>
#include <WiFi.h>

namespace WifiConfig {
namespace {
constexpr char kDefaultSsid[] = "MEI_RD_2.4G";
constexpr char kDefaultPassword[] = "@778899@";
constexpr unsigned long kWifiTimeoutMs = 15000;
constexpr unsigned long kReconnectIntervalMs = 10000;

Preferences preferences;
String currentSsid;
String currentPassword;
WebServer* wifiServer = nullptr;
WifiConfig::AuthenticationCheck authenticationCheck = nullptr;
unsigned long lastReconnectAttempt = 0;
bool connectionWasLogged = false;

String escapeJson(const String& value)
{
    String escaped;
    escaped.reserve(value.length() + 8);
    for (size_t i = 0; i < value.length(); ++i)
    {
        const char character = value[i];
        if (character == '"')
        {
            escaped += "\\\"";
        }
        else if (character == '\\')
        {
            escaped += "\\\\";
        }
        else if (static_cast<uint8_t>(character) < 0x20)
        {
            char encoded[7];
            snprintf(encoded, sizeof(encoded), "\\u%04x", static_cast<uint8_t>(character));
            escaped += encoded;
        }
        else
        {
            escaped += character;
        }
    }
    return escaped;
}

bool isAuthorized()
{
    return authenticationCheck != nullptr && authenticationCheck();
}

void handleScanNetworks()
{
    if (!isAuthorized())
    {
        wifiServer->send(401, "application/json; charset=utf-8",
                         "{\"available\":false,\"message\":\"Vui long dang nhap.\"}");
        return;
    }

    const int networkCount = WiFi.scanNetworks(false, false);
    if (networkCount < 0)
    {
        wifiServer->send(503, "application/json; charset=utf-8",
                         "{\"available\":false,\"message\":\"Quet Wi-Fi that bai. Hay thu lai.\"}");
        return;
    }

    String json = "{\"available\":true,\"networks\":[";
    for (int i = 0; i < networkCount; ++i)
    {
        if (i > 0)
        {
            json += ',';
        }
        json += "{\"ssid\":\"";
        json += escapeJson(WiFi.SSID(i));
        json += "\",\"rssi\":";
        json += String(WiFi.RSSI(i));
        json += ",\"secured\":";
        json += (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "false" : "true";
        json += '}';
    }
    json += "]}";
    WiFi.scanDelete();
    wifiServer->send(200, "application/json; charset=utf-8", json);
}

void handleForgetSavedNetwork()
{
    if (!isAuthorized())
    {
        wifiServer->send(401, "text/plain; charset=utf-8", "Vui long dang nhap.");
        return;
    }

    wifiServer->send(200, "text/plain; charset=utf-8",
                     "Da xoa Wi-Fi da luu. ESP32 se khoi dong lai voi Wi-Fi mac dinh.");
    delay(250);
    resetSavedNetwork();
}

bool waitForWifi(unsigned long timeoutMs)
{
    const unsigned long startedAt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startedAt < timeoutMs)
    {
        delay(250);
    }
    return WiFi.status() == WL_CONNECTED;
}

void saveCredentials()
{
    preferences.begin("wifi-config", false);
    preferences.putString("ssid", currentSsid);
    preferences.putString("password", currentPassword);
    preferences.end();
}
} // namespace

void connectSavedNetwork()
{
    preferences.begin("wifi-config", true);
    currentSsid = preferences.getString("ssid", kDefaultSsid);
    currentPassword = preferences.getString("password", kDefaultPassword);
    preferences.end();

    WiFi.mode(WIFI_STA);
    WiFi.begin(currentSsid.c_str(), currentPassword.c_str());
    Serial.print("Dang ket noi Wi-Fi: ");
    Serial.println(currentSsid);
    lastReconnectAttempt = millis();
    connectionWasLogged = false;
}

void loop()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        if (!connectionWasLogged)
        {
            Serial.print("Wi-Fi connected. ESP32 IP: http://");
            Serial.println(WiFi.localIP());
            connectionWasLogged = true;
        }
        return;
    }

    connectionWasLogged = false;
    if (currentSsid.isEmpty() || millis() - lastReconnectAttempt < kReconnectIntervalMs)
    {
        return;
    }

    Serial.print("Wi-Fi disconnected; retrying: ");
    Serial.println(currentSsid);
    WiFi.disconnect(false, false);
    WiFi.begin(currentSsid.c_str(), currentPassword.c_str());
    lastReconnectAttempt = millis();
}

bool changeNetwork(const String& ssid, const String& password)
{
    const String oldSsid = currentSsid;
    const String oldPassword = currentPassword;

    Serial.print("Dang ket noi Wi-Fi moi: ");
    Serial.println(ssid);
    connectionWasLogged = false;
    WiFi.disconnect(false, false);
    WiFi.begin(ssid.c_str(), password.c_str());

    if (waitForWifi(kWifiTimeoutMs))
    {
        currentSsid = ssid;
        currentPassword = password;
        saveCredentials();
        Serial.println("Da ket noi Wi-Fi moi.");
        Serial.print("IP moi cua ESP32: http://");
        Serial.println(WiFi.localIP());
        connectionWasLogged = true;
        return true;
    }

    Serial.println("Khong ket noi duoc Wi-Fi moi; dang khoi phuc cau hinh cu.");
    WiFi.disconnect(false, false);
    WiFi.begin(oldSsid.c_str(), oldPassword.c_str());
    if (waitForWifi(kWifiTimeoutMs))
    {
        Serial.print("Da khoi phuc Wi-Fi cu. IP: http://");
        Serial.println(WiFi.localIP());
        connectionWasLogged = true;
    }
    else
    {
        Serial.println("Khong ket noi duoc Wi-Fi cu. Kiem tra router va cau hinh.");
    }
    return false;
}

void resetSavedNetwork()
{
    preferences.begin("wifi-config", false);
    preferences.clear();
    preferences.end();

    Serial.println("Da xoa Wi-Fi da luu. ESP32 se khoi dong lai voi Wi-Fi mac dinh.");
    delay(500);
    ESP.restart();
}

void registerRoutes(WebServer& server, AuthenticationCheck isAuthenticated)
{
    wifiServer = &server;
    authenticationCheck = isAuthenticated;
    server.on("/api/wifi/scan", HTTP_GET, handleScanNetworks);
    server.on("/wifi/forget", HTTP_POST, handleForgetSavedNetwork);
}

} // namespace WifiConfig
