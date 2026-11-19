#include "drivers/hikvision.h"

HikvisionClient::HikvisionClient(const char* ip, int port, const char* user, const char* pass, const char* googleScriptUrl)
    : _ip(ip), _port(port), _user(user), _pass(pass), _googleScriptUrl(googleScriptUrl), _lastEventTime("") {}

void HikvisionClient::fetchEvents()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("[Hikvision] WiFi disconnected!");
        return;
    }

    IPAddress espIp = WiFi.localIP();
    IPAddress espGw = WiFi.gatewayIP();

    // 1. Kiem tra xem ESP32 va Hikvision co cung dai mang LAN khong
    IPAddress targetIp;
    if (targetIp.fromString(_ip))
    {
        if ((espIp[0] != targetIp[0]) || (espIp[1] != targetIp[1]) || (espIp[2] != targetIp[2]))
        {
            Serial.println("\n****************************************************************");
            Serial.println("[Hikvision] CANH BAO NGHIEP TRONG: KHAC LOP MANG LAN!");
            Serial.printf(" - ESP32 IP:      %s (Gateway: %s)\n", espIp.toString().c_str(), espGw.toString().c_str());
            Serial.printf(" - Hikvision IP:  %s (Gateway tren SADP: 192.168.0.1)\n", _ip);
            Serial.println(" -> ESP32 KHONG THE ket noi den Hikvision vi khac lop mang!");
            Serial.println(" -> KHAC PHUC:");
            Serial.println("    Mo SADP tren PC, chon thiet bi va tich bat [DHCP]");
            Serial.printf("    hoac doi IP cua Hikvision thanh cung dai mang %d.%d.%d.xxx\n", espIp[0], espIp[1], espIp[2]);
            Serial.println("****************************************************************\n");
        }
    }

    // 2. Kiem tra ket noi TCP (thu port cau hinh, neu fail thu port 80 / 8000)
    int targetPort = _port;
    WiFiClient testClient;

    if (!testClient.connect(_ip, targetPort))
    {
        int fallbackPort = (targetPort == 80) ? 8000 : 80;
        if (testClient.connect(_ip, fallbackPort))
        {
            targetPort = fallbackPort;
            _port = fallbackPort;
            Serial.printf("[Hikvision] Ket noi TCP thanh cong qua cong: %d\n", targetPort);
            testClient.stop();
        }
        else
        {
            Serial.printf("[Hikvision] TCP Connection toi %s:%d (va %d) THAT BAI!\n", _ip, _port, fallbackPort);
            testClient.stop();
            return;
        }
    }
    else
    {
        testClient.stop();
    }

    // 3. Gui HTTP POST ISAPI
    HTTPClient http;
    String url = String("http://") + _ip + ":" + String(targetPort) + "/ISAPI/AccessControl/AcsEvent?format=json";

    http.setConnectTimeout(4000);
    http.setTimeout(4000);

    if (!http.begin(url))
    {
        Serial.println("[Hikvision] HTTP begin failed!");
        return;
    }

    http.addHeader("Content-Type", "application/json");
    http.setAuthorization(_user, _pass);

    // timeReverseOrder: true -> lay ban ghi moi nhat truoc
    // minor: 0 -> lay tat ca su kien Access Control
    String payload =
        "{\"AcsEventCond\":{"
        "\"searchID\":\"1\","
        "\"searchResultPosition\":0,"
        "\"maxResults\":5,"
        "\"major\":5,"
        "\"minor\":0,"
        "\"timeReverseOrder\":true"
        "}}";

    int code = http.POST(payload);

    if (code == 401)
    {
        Serial.println("\n[Hikvision] LOI 401 UNAUTHORIZED!");
        Serial.println(" -> Thiet bi Hikvision yeu cau xac thuc Digest.");
        Serial.println(" -> Vao web Hikvision: Configuration -> System -> Security -> Authentication");
        Serial.println("    doi ISAPI/WEB Authentication sang 'digest/basic'.\n");
    }
    else if (code == HTTP_CODE_OK)
    {
        String response = http.getString();
        parseAndForwardEvent(response);
    }
    else if (code > 0)
    {
        Serial.printf("[Hikvision] HTTP response code: %d\n", code);
    }
    else
    {
        Serial.printf("[Hikvision] HTTP POST loi: %s\n", http.errorToString(code).c_str());
    }

    http.end();
}

void HikvisionClient::parseAndForwardEvent(const String& jsonResponse)
{
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonResponse);

    if (error)
    {
        Serial.print("[Hikvision] deserializeJson() failed: ");
        Serial.println(error.c_str());
        return;
    }

    JsonArray infoList = doc["AcsEvent"]["InfoList"];
    if (infoList.isNull() || infoList.size() == 0)
    {
        return;
    }

    // Su kien moi nhat nam o phan tu dau tien do timeReverseOrder = true
    JsonObject latestEvent = infoList[0];
    const char* time = latestEvent["time"] | "";
    const char* empId = latestEvent["employeeNoString"] | "Unknown";
    const char* name = latestEvent["name"] | "Unknown";
    int minor = latestEvent["minor"] | 0;

    // Khoi tao moc thoi gian o lan doc dau tien de tranh in lai su kien cu
    if (_lastEventTime.isEmpty())
    {
        _lastEventTime = time;
        Serial.printf("[Hikvision] Da ket noi! Su kien gan nhat: [%s] ID: %s. Dang cho luot quet mat moi...\n", time, empId);
        return;
    }

    // Neu thoi gian trung voi lan da xu ly thi bo qua de khong bi lap lai moi 5s
    if (_lastEventTime == time)
    {
        return;
    }

    // Cap nhat moc thoi gian moi nhat
    _lastEventTime = time;

    // In ro rang len Serial Monitor
    Serial.println("\n=======================================================");
    Serial.println("  [HIKVISION] PHAT HIEN QUET KHUON MAT MOI!            ");
    Serial.printf("  - Ma Nhan Vien (ID): %s\n", empId);
    Serial.printf("  - Ho va Ten:         %s\n", name);
    Serial.printf("  - Thoi gian quet:    %s\n", time);
    Serial.printf("  - Ma su kien (Minor): %d (0x%02X)\n", minor, minor);
    Serial.println("=======================================================\n");

    // Neu co URL Google Sheets thi day tiep
    if (_googleScriptUrl != nullptr && strlen(_googleScriptUrl) > 0)
    {
        sendToGoogleSheets(empId, name, time);
    }
}

void HikvisionClient::sendToGoogleSheets(const char* empId, const char* name, const char* timeStr)
{
    if (_googleScriptUrl == nullptr || strlen(_googleScriptUrl) == 0)
    {
        return;
    }

    HTTPClient http;
    http.begin(_googleScriptUrl);
    http.addHeader("Content-Type", "application/json");
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(4000);

    JsonDocument doc;
    doc["emp_id"] = empId;
    doc["name"]   = name;
    doc["time"]   = timeStr;

    String payload;
    serializeJson(doc, payload);

    int code = http.POST(payload);

    if (code > 0)
    {
        Serial.printf("[GoogleSheets] Day du lieu thanh cong! Code: %d\n", code);
    }
    else
    {
        Serial.printf("[GoogleSheets] Loi khi day du lieu: %s\n", http.errorToString(code).c_str());
    }

    http.end();
}
