#ifndef HIKVISION_H
#define HIKVISION_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

class HikvisionClient {
private:
    const char* _ip;
    int _port;
    const char* _user;
    const char* _pass;
    const char* _googleScriptUrl;
    String _lastEventTime;

    // Các hàm xử lý nội bộ
    void parseAndForwardEvent(const String& jsonResponse);
    void sendToGoogleSheets(const char* empId, const char* name, const char* timeStr);

public:
    // Constructor khởi tạo thông số thiết bị và URL Google Sheet
    HikvisionClient(const char* ip, int port, const char* user, const char* pass, const char* googleScriptUrl = nullptr);

    // Hàm chính gọi từ loop() để truy vấn sự kiện
    void fetchEvents();
};

#endif // HIKVISION_H