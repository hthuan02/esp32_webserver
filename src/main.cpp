#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

namespace {
constexpr char kDefaultSsid[] = "MEI_RD_2.4G";
constexpr char kDefaultPassword[] = "@778899@";
constexpr char kLoginUsername[] = "esp32";
constexpr char kLoginPassword[] = "1234";
constexpr unsigned long kWifiTimeoutMs = 15000;

WebServer server(80);
Preferences preferences;
String currentSsid;
String currentPassword;
bool isAuthenticated = false;

bool waitForWifi(unsigned long timeoutMs)
{
    const unsigned long startedAt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startedAt < timeoutMs)
    {
        delay(250);
    }
    return WiFi.status() == WL_CONNECTED;
}

void redirectToLogin()
{
    server.sendHeader("Location", "/");
    server.send(303, "text/plain", "Dang chuyen ve trang dang nhap.");
}

void viewer()
{
    String html = R"HTML(
<!doctype html><html lang="vi"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#f0f3f9"><title>MEI CONNECT | Đăng nhập</title>
<style>
*{box-sizing:border-box}body{margin:0;min-height:100vh;display:flex;flex-direction:column;background:#f0f3f9;color:#263746;font-family:Arial,Helvetica,sans-serif}
.topbar{height:88px;padding:0 30px;display:flex;align-items:center;justify-content:space-between;background:#fff;border-bottom:1px solid #e6e9ef}
.brand{display:flex;align-items:center;gap:14px;font-size:28px;letter-spacing:-.5px}.brand strong{color:#168f91}
.mark{width:54px;height:54px;border-radius:14px;display:grid;place-items:center;background:#168f91;color:#fff;font-size:22px;font-weight:700;box-shadow:inset 0 0 0 2px #ffffff55}
.theme{width:54px;height:52px;border:1px solid #e4e7ec;border-radius:12px;background:#fff;color:#263746;font-size:19px;cursor:pointer}
main{flex:1;display:grid;place-items:start center;padding:80px 20px 40px}.card{width:min(510px,100%);padding:28px 30px 68px;background:#fff;border:1px solid #e1e5eb;border-radius:19px;box-shadow:0 8px 28px #1d35570a}
.eyebrow{margin:0 0 14px;color:#7c8992;font-size:15px;letter-spacing:.06em;text-transform:uppercase}h1{margin:0 0 22px;color:#263746;font-size:30px;line-height:1.2}
.field{margin:0 0 20px}label{display:block;margin-bottom:9px;color:#697781;font-size:15px}input{width:100%;height:56px;padding:0 16px;border:1px solid #e2e7ef;border-radius:11px;background:#eaf0fb;color:#17212b;font:16px Arial,sans-serif;outline:none}
input:focus{border-color:#168f91;box-shadow:0 0 0 3px #168f9125;background:#fff}.submit{min-width:144px;height:52px;margin-top:7px;padding:0 22px;border:0;border-radius:11px;background:#108f91;color:#fff;font-size:17px;font-weight:600;cursor:pointer}.submit:hover{background:#087c7e}
footer{padding:0 16px 24px;text-align:center;color:#79858e;font-size:15px;line-height:1.7}footer span{color:#263746}
body.dark{background:#161d27;color:#eaf0f5}body.dark .topbar,body.dark .card,body.dark .theme{background:#202a36;color:#eaf0f5;border-color:#334252}body.dark h1,body.dark footer span{color:#eaf0f5}body.dark label,body.dark .eyebrow,body.dark footer{color:#aab6c1}body.dark input{background:#293646;color:#f5f8fb;border-color:#39495a}
@media(max-width:560px){.topbar{height:72px;padding:0 16px}.brand{gap:10px;font-size:22px}.mark{width:44px;height:44px}.theme{width:44px;height:44px}main{padding:38px 16px 28px}.card{padding:25px 22px 42px}h1{font-size:27px}}
</style></head><body>
<header class="topbar"><div class="brand"><div class="mark" aria-hidden="true">M</div><div><strong>MEI</strong> CONNECT</div><button class="theme" type="button" aria-label="Đổi giao diện" onclick="document.body.classList.toggle('dark')">◐</button></header>
<main><section class="card" aria-labelledby="login-title"><p class="eyebrow">Quản trị thiết bị</p><h1 id="login-title">Đăng nhập</h1>
<form action="/login" method="post" autocomplete="on"><div class="field"><label for="username">Tài khoản</label><input id="username" name="username" type="text" placeholder="Nhập tài khoản" autocomplete="username" required></div>
<div class="field"><label for="password">Mật khẩu</label><input id="password" name="password" type="password" placeholder="Nhập mật khẩu" autocomplete="current-password" required></div><button class="submit" type="submit">Đăng nhập</button></form></section></main>
<footer>MEIGROUP IOT, IIOT, AIOT<br><span>© 2026 Powered by MEIGROUP.VN</span></footer></body></html>
)HTML";
    server.send(200, "text/html; charset=utf-8", html);
}

void dashboard()
{
    if (!isAuthenticated)
    {
        redirectToLogin();
        return;
    }

    String html = R"HTML(
<!doctype html><html lang="vi"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>MEI CONNECT | Quản trị</title><style>
*{box-sizing:border-box}body{margin:0;min-height:100vh;background:#f0f3f9;color:#263746;font-family:Arial,Helvetica,sans-serif}
header{height:76px;padding:0 28px;display:flex;align-items:center;justify-content:space-between;background:#fff;border-bottom:1px solid #e2e7ed}
.brand{font-size:25px}.brand strong{color:#168f91}.menu{position:relative}.menu-button,.submit{border:0;border-radius:9px;background:#108f91;color:white;padding:12px 18px;font-size:15px;cursor:pointer}
.menu-list{display:none;position:absolute;right:0;top:48px;z-index:2;min-width:190px;padding:8px;background:#fff;border:1px solid #e1e5eb;border-radius:10px;box-shadow:0 8px 24px #1d355720}.menu-list.open{display:block}.menu-list a{display:block;padding:11px 12px;border-radius:7px;color:#263746;text-decoration:none}.menu-list a:hover{background:#eef4f6}
main{width:min(760px,100%);margin:48px auto;padding:0 18px}.card{margin-bottom:22px;padding:26px;background:#fff;border:1px solid #e1e5eb;border-radius:16px;box-shadow:0 8px 28px #1d35570a}h1{margin:0 0 12px;font-size:27px}h2{margin:0 0 10px;font-size:21px}.muted{color:#74818b;line-height:1.6}.field{margin:18px 0}label{display:block;margin-bottom:8px;color:#65737e}input{width:100%;height:50px;padding:0 14px;border:1px solid #dce3eb;border-radius:9px;background:#f5f7fb;font-size:16px}.submit{margin-top:5px}.status{padding:12px;border-radius:8px;background:#edf7f5;color:#176c62}
@media(max-width:560px){header{padding:0 16px}.brand{font-size:21px}main{margin:28px auto}.card{padding:21px}}
</style></head><body>
<header><div class="brand"><strong>MEI</strong> CONNECT</div><div class="menu"><button class="menu-button" type="button" onclick="document.getElementById('menu-list').classList.toggle('open')">☰ Menu</button>
<nav class="menu-list" id="menu-list"><a href="/dashboard">Tổng quan</a><a href="#wifi-settings" onclick="document.getElementById('menu-list').classList.remove('open')">Chuyển đổi Wi-Fi</a><a href="/logout">Đăng xuất</a></nav></div></header>
<main><section class="card"><h1>Quản trị thiết bị</h1><p class="muted">Đăng nhập thành công. Chọn Menu ở phía trên để mở phần cấu hình Wi-Fi.</p></section>
<section class="card" id="wifi-settings"><h2>Chuyển đổi Wi-Fi</h2><p class="muted">Nhập tên và mật khẩu mạng Wi-Fi mới. Sau khi kết nối, địa chỉ IP mới sẽ được in trên Serial Monitor.</p>
<form action="/wifi" method="post" autocomplete="off"><div class="field"><label for="ssid">Tên Wi-Fi (SSID)</label><input id="ssid" name="ssid" type="text" placeholder="Nhập tên Wi-Fi" required></div>
<div class="field"><label for="wifi-password">Mật khẩu Wi-Fi</label><input id="wifi-password" name="password" type="password" placeholder="Nhập mật khẩu Wi-Fi"></div><button class="submit" type="submit">Kết nối Wi-Fi mới</button></form></section></main></body></html>
)HTML";
    server.send(200, "text/html; charset=utf-8", html);
}

void handleLogin()
{
    if (server.arg("username") == kLoginUsername && server.arg("password") == kLoginPassword)
    {
        isAuthenticated = true;
        server.sendHeader("Location", "/dashboard");
        server.send(303, "text/plain", "Dang nhap thanh cong.");
        return;
    }

    server.send(401, "text/html; charset=utf-8",
                "<meta charset='utf-8'><p>Tài khoản hoặc mật khẩu không đúng.</p><a href='/'>Thử lại</a>");
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

    const String oldSsid = currentSsid;
    const String oldPassword = currentPassword;
    server.send(200, "text/html; charset=utf-8",
                "<!doctype html><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
                "<title>Đang đổi Wi-Fi</title><h2>Đang kết nối Wi-Fi mới...</h2>"
                "<p>Vui lòng chờ. Địa chỉ IP mới sẽ xuất hiện trên Serial Monitor.</p>");
    delay(300);

    Serial.print("Dang ket noi Wi-Fi moi: ");
    Serial.println(newSsid);
    WiFi.disconnect(false, false);
    WiFi.begin(newSsid.c_str(), newPassword.c_str());

    if (waitForWifi(kWifiTimeoutMs))
    {
        currentSsid = newSsid;
        currentPassword = newPassword;
        preferences.begin("wifi-config", false);
        preferences.putString("ssid", currentSsid);
        preferences.putString("password", currentPassword);
        preferences.end();
        Serial.println("Da ket noi Wi-Fi moi.");
        Serial.print("IP moi cua ESP32: http://");
        Serial.println(WiFi.localIP());
        return;
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

    server.on("/", HTTP_GET, handleHome);
    server.on("/login", HTTP_POST, handleLogin);
    server.on("/dashboard", HTTP_GET, dashboard);
    server.on("/wifi", HTTP_POST, wifi_config);
    server.on("/logout", HTTP_GET, handleLogout);
    server.onNotFound([]() {
        server.send(404, "text/plain; charset=utf-8", "Khong tim thay trang.");
    });
    server.begin();
}

void loop()
{
    server.handleClient();
}
