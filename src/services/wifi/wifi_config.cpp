#include "services/wifi/wifi_config.h"

#include <Preferences.h>
#include <WiFi.h>

namespace WifiConfig {
namespace {
constexpr char kDefaultSsid[] = "MEI_RD_2.4G";
constexpr char kDefaultPassword[] = "@778899@";
constexpr unsigned long kWifiTimeoutMs = 15000;

Preferences preferences;
String currentSsid;
String currentPassword;

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

    while (!waitForWifi(kWifiTimeoutMs))
    {
        Serial.println("Chua ket noi duoc. Thu lai sau 3 giay...");
        delay(3000);
        WiFi.disconnect(false, false);
        WiFi.begin(currentSsid.c_str(), currentPassword.c_str());
    }

    Serial.print("IP cua ESP32: http://");
    Serial.println(WiFi.localIP());
}

bool changeNetwork(const String& ssid, const String& password)
{
    const String oldSsid = currentSsid;
    const String oldPassword = currentPassword;

    Serial.print("Dang ket noi Wi-Fi moi: ");
    Serial.println(ssid);
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
        return true;
    }

    Serial.println("Khong ket noi duoc Wi-Fi moi; dang khoi phuc cau hinh cu.");
    WiFi.disconnect(false, false);
    WiFi.begin(oldSsid.c_str(), oldPassword.c_str());
    if (waitForWifi(kWifiTimeoutMs))
    {
        Serial.print("Da khoi phuc Wi-Fi cu. IP: http://");
        Serial.println(WiFi.localIP());
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

} // namespace WifiConfig
