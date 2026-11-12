#include "services/ota.h"

#include <Update.h>

namespace OtaManager {
namespace {
WebServer* otaServer = nullptr;
AuthenticationCheck authenticationCheck = nullptr;
bool updateStarted = false;
bool updateFailed = false;
bool updateFinished = false;
bool uploadRejected = false;
String updateError;

bool isAuthorized()
{
    return authenticationCheck != nullptr && authenticationCheck();
}

void handleFirmwareUpload()
{
    if (!isAuthorized())
    {
        uploadRejected = true;
        return;
    }

    HTTPUpload& upload = otaServer->upload();
    switch (upload.status)
    {
    case UPLOAD_FILE_START:
    {
        updateStarted = false;
        updateFailed = false;
        updateFinished = false;
        uploadRejected = false;
        updateError = "";

        String filename = upload.filename;
        filename.toLowerCase();
        if (!filename.endsWith(".bin"))
        {
            updateFailed = true;
            updateError = "Chi chap nhan file firmware .bin.";
            return;
        }

        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH))
        {
            updateFailed = true;
            updateError = Update.errorString();
            return;
        }
        updateStarted = true;
        Serial.printf("OTA firmware: bat dau nap %s\n", upload.filename.c_str());
        break;
    }

    case UPLOAD_FILE_WRITE:
        if (updateStarted && !updateFailed)
        {
            const size_t written = Update.write(upload.buf, upload.currentSize);
            if (written != upload.currentSize)
            {
                updateFailed = true;
                updateError = Update.errorString();
                Update.abort();
            }
        }
        break;

    case UPLOAD_FILE_END:
        if (updateStarted && !updateFailed)
        {
            if (Update.end(true))
            {
                updateFinished = true;
                Serial.printf("OTA firmware: da ghi %u byte\n", static_cast<unsigned>(upload.totalSize));
            }
            else
            {
                updateFailed = true;
                updateError = Update.errorString();
            }
        }
        break;

    case UPLOAD_FILE_ABORTED:
        if (updateStarted)
        {
            Update.abort();
        }
        updateStarted = false;
        updateFailed = true;
        updateError = "Qua trinh upload firmware bi huy.";
        break;

    default:
        break;
    }
}

void handleFirmwareResult()
{
    if (!isAuthorized() || uploadRejected)
    {
        otaServer->send(401, "text/plain; charset=utf-8", "Vui long dang nhap de cap nhat firmware.");
        return;
    }

    if (!updateFinished || updateFailed)
    {
        String message = "Cap nhat firmware that bai.";
        if (updateError.length())
        {
            message += " ";
            message += updateError;
        }
        otaServer->send(400, "text/plain; charset=utf-8", message);
        return;
    }

    otaServer->send(200, "text/plain; charset=utf-8",
                    "Firmware da cap nhat. ESP32 dang khoi dong lai.");
    Serial.println("OTA firmware: cap nhat thanh cong, khoi dong lai sau 1 giay.");
    delay(1000);
    ESP.restart();
}
} // namespace

void registerRoutes(WebServer& server, AuthenticationCheck isAuthenticated)
{
    otaServer = &server;
    authenticationCheck = isAuthenticated;
    server.on("/ota/update", HTTP_POST, handleFirmwareResult, handleFirmwareUpload);
}

} // namespace OtaManager
