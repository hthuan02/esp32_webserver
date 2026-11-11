
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

// WiFi configuration
const char *ssid = "iPhone";
const char *pass_word = "20138131";

WebServer server(80);

void viewer()
{
    String html = "<html><body>";

    html += "<h2>WiFi Configuration</h2>";

    html += "<form action='/connect' method='POST'>";

    html += "<label>WiFi Account:</label><br>";
    html += "<input type='text' name='ssid' placeholder='Enter WiFi name'><br><br>";

    html += "<label>WiFi Password:</label><br>";
    html += "<input type='password' name='password' placeholder='Enter WiFi password'><br><br>";

    html += "<input type='submit' value='Connect'>";

    html += "</form>";
    html += "</body></html>";

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
