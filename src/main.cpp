#include <Arduino.h>
#include <LittleFS.h>
#include <WebServer.h>

#include "services/ota_manager/ota_manager.h"
#include "services/user_manager/user_manager.h"
#include "services/wifi_config/wifi_config.h"

namespace {
WebServer server(80);
bool isAuthenticated = false;

bool isUserAuthenticated()
{
    return isAuthenticated;
}

void servePage(const char* path)
{
    File page = LittleFS.open(path, "r");
    if (!page)
    {
        Serial.print("Khong mo duoc file giao dien: ");
        Serial.println(path);
        server.send(500, "text/plain; charset=utf-8",
                    "Thieu file giao dien trong LittleFS. Hay upload filesystem image.");
        return;
    }

    server.streamFile(page, "text/html; charset=utf-8");
    page.close();
}

void redirectToLogin()
{
    server.sendHeader("Location", "/");
    server.send(303, "text/plain", "Dang chuyen ve trang dang nhap.");
}

void viewer()
{
    servePage("/pages/login.html");
}

void dashboard()
{
    if (!isAuthenticated)
    {
        redirectToLogin();
        return;
    }

    servePage("/pages/dashboard.html");
}

void handleLogin()
{
    if (UserManager::authenticate(server.arg("username"), server.arg("password")))
    {
        isAuthenticated = true;
        server.sendHeader("Location", "/dashboard");
        server.send(303, "text/plain", "Dang nhap thanh cong.");
        return;
    }

    server.sendHeader("Location", "/");
    server.send(303, "text/plain; charset=utf-8", "Sai tai khoan hoac mat khau.");
}

void wifi_config()
{
    if (!isAuthenticated)
    {
        redirectToLogin();
        return;
    }

    String newSsid = server.arg("ssid");
    String newPassword = server.arg("password");
    newSsid.trim();
    if (newSsid.length() == 0)
    {
        server.send(400, "text/plain; charset=utf-8", "Ten Wi-Fi khong duoc de trong.");
        return;
    }

    server.send(200, "text/plain; charset=utf-8",
                "Dang doi Wi-Fi. Kiem tra Serial Monitor de xem IP moi.");
    delay(300);
    WifiConfig::changeNetwork(newSsid, newPassword);
}

void handleHome()
{
    if (isAuthenticated)
    {
        dashboard();
        return;
    }
    viewer();
}

void handleLogout()
{
    isAuthenticated = false;
    redirectToLogin();
}
} // namespace

void setup()
{
    Serial.begin(115200);

    if (!LittleFS.begin(false))
    {
        Serial.println("LittleFS mount that bai. Hay upload filesystem image.");
    }

    WifiConfig::connectSavedNetwork();

    server.on("/", HTTP_GET, handleHome);
    server.on("/login", HTTP_POST, handleLogin);
    server.on("/dashboard", HTTP_GET, dashboard);
    server.on("/wifi", HTTP_POST, wifi_config);
    server.on("/logout", HTTP_GET, handleLogout);
    OtaManager::registerRoutes(server, isUserAuthenticated);
    server.onNotFound([]() {
        server.send(404, "text/plain; charset=utf-8", "Khong tim thay trang.");
    });
    server.begin();
    Serial.println("Web server da bat dau tren cong 80.");
}

void loop()
{
    if (Serial.available() > 0)
    {
        const char command = static_cast<char>(Serial.read());
        if (command == 'r' || command == 'R')
        {
            WifiConfig::resetSavedNetwork();
        }
    }

    server.handleClient();
}
