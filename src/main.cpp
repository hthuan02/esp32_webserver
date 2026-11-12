
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

// WiFi configuration
const char *ssid = "MEI_RD_2.4G";
const char *pass_word = "@778899@";

WebServer server(80);

void viewer()
{
    String html = R"HTML(
    <!doctype html>
    <html lang="vi">
    <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta name="theme-color" content="#f0f3f9">
    <title>BOARD MADIN | Đăng nhập</title>
    <style>
        *{box-sizing:border-box}
        body{margin:0;min-height:100vh;display:flex;flex-direction:column;background:#f0f3f9;color:#263746;font-family:Arial,Helvetica,sans-serif}
        .topbar{height:88px;padding:0 30px;display:flex;align-items:center;justify-content:space-between;background:#fff;border-bottom:1px solid #e6e9ef}
        .brand{display:flex;align-items:center;gap:14px;font-size:28px;letter-spacing:-.5px}
        .brand-name strong{color:#168f91}
        .mark{width:54px;height:54px;border-radius:14px;display:grid;place-items:center;background:#168f91;color:#fff;font-size:22px;font-weight:700;box-shadow:inset 0 0 0 2px #ffffff55}
        .theme{width:54px;height:52px;border:1px solid #e4e7ec;border-radius:12px;background:#fff;color:#263746;font-size:19px;cursor:pointer}
        main{flex:1;display:grid;place-items:start center;padding:80px 20px 40px}
        .card{width:min(510px,100%);padding:28px 30px 68px;background:#fff;border:1px solid #e1e5eb;border-radius:19px;box-shadow:0 8px 28px #1d35570a}
        .eyebrow{margin:0 0 14px;color:#7c8992;font-size:15px;letter-spacing:.06em;text-transform:uppercase}
        h1{margin:0 0 22px;color:#263746;font-size:30px;line-height:1.2}
        .field{margin:0 0 20px}
        label{display:block;margin-bottom:9px;color:#697781;font-size:15px}
        input{width:100%;height:56px;padding:0 16px;border:1px solid #e2e7ef;border-radius:11px;background:#eaf0fb;color:#17212b;font:16px Arial, sans-serif;outline:none;transition:.2s}
        input:focus{border-color:#168f91;box-shadow:0 0 0 3px #168f9125;background:#fff}
        .submit{min-width:144px;height:52px;margin-top:7px;padding:0 22px;border:0;border-radius:11px;background:#108f91;color:#fff;font-size:17px;font-weight:600;cursor:pointer;transition:.2s}
        .submit:hover{background:#087c7e;transform:translateY(-1px)}
        footer{padding:0 16px 24px;text-align:center;color:#79858e;font-size:15px;line-height:1.7}
        footer span{color:#263746}
        body.dark{background:#161d27;color:#eaf0f5}
        body.dark .topbar,body.dark .card,body.dark .theme{background:#202a36;color:#eaf0f5;border-color:#334252}
        body.dark h1,body.dark footer span{color:#eaf0f5}
        body.dark label,body.dark .eyebrow,body.dark footer{color:#aab6c1}
        body.dark input{background:#293646;color:#f5f8fb;border-color:#39495a}
        @media(max-width:560px){.topbar{height:72px;padding:0 16px}.brand{gap:10px;font-size:22px}.mark{width:44px;height:44px;border-radius:12px}.theme{width:44px;height:44px}main{padding:38px 16px 28px}.card{padding:25px 22px 42px}h1{font-size:27px}}
    </style>
    </head>
    <body>
    <header class="topbar">
        <div class="brand"><div class="mark" aria-hidden="true"></div>
        <button class="theme" type="button" aria-label="Đổi giao diện" onclick="document.body.classList.toggle('dark')">◐</button>
    </header>
    <main>
        <section class="card" aria-labelledby="login-title">
        <p class="eyebrow">Thông tin đăng nhập</p>
        <h1 id="login-title">Đăng nhập</h1>
        <form action="/connect" method="post" autocomplete="on">
            <div class="field"><label for="username">Tài khoản</label><input id="username" name="ssid" type="text" placeholder="Nhập tài khoản" autocomplete="username" required></div>
            <div class="field"><label for="password">Mật khẩu</label><input id="password" name="password" type="password" placeholder="Nhập mật khẩu" autocomplete="current-password" required></div>
            <button class="submit" type="submit">Đăng nhập</button>
        </form>
        </section>
    </main>
   
    </body>
    </html>
    )HTML";

    server.send(200, "text/html", html);
}

void wifi_config()
{
    String new_ssid = server.arg("ssid");
    String new_password = server.arg("password");

    Serial.println("New WiFi Account: " + new_ssid);
    Serial.println("New WiFi Password: " + new_password);

    server.send(200, "text/html", "WiFi configuration received!");
}

void setup()
{
    Serial.begin(115200);

    WiFi.begin(ssid, pass_word);

    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.printf("..");
        delay(100);
    }

    Serial.println("Connected...good!");
    Serial.println(WiFi.localIP());

    server.on("/", HTTP_GET, viewer);
    server.on("/connect", HTTP_POST, wifi_config);

    server.begin();
}

void loop()
{
    server.handleClient();
}
